// Tests for message storage, streaming sentinel detection, and the harness.

#include <cstddef>
#include <string>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "core/conversation.h"
#include "core/sentinel_scanner.h"
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
// Write a deterministic script for a test that uses the provided client.
void write_script(const std::string& path, const std::string& content) {
    std::ofstream script(path);
    script << content;
}

// Input source that always supplies one line and never reaches EOF.
class OneLineInput : public InputSource {
public:
    std::string read_line() override { return "hello"; }
    bool is_eof() const override { return false; }
};

// Input source used to verify the harness stops cleanly at EOF.
class EofInput : public InputSource {
public:
    std::string read_line() override { return ""; }
    bool is_eof() const override { return true; }
};

// Output sink that makes all emitted text available to assertions.
class CollectingOutput : public OutputSink {
public:
    void write(std::string_view text) override { text_ += text; }
    const std::string& text() const { return text_; }

private:
    std::string text_;
};

// Token sink that records generated text and completion callbacks.
class CollectingTokenSink : public TokenSink {
public:
    void on_chunk(std::string_view chunk) override { text += chunk; }
    void on_complete() override { completed = true; }
    void clear() {
        text.clear();
        completed = false;
    }

    std::string text;
    bool completed = false;
};
}  // namespace

int main() {
    // Empty conversations support size, iteration, and checked access.
    Conversation empty;
    assert(empty.size() == 0);
    assert(empty.begin() == empty.end());

    bool out_of_range = false;
    try {
        empty.at(0);
    } catch (const std::out_of_range&) {
        out_of_range = true;
    }
    assert(out_of_range);

    // Appending preserves message roles and contents.
    Conversation conversation;
    conversation.append(Message(Role::User, "hello"));
    assert(conversation.size() == 1);
    assert(conversation.at(0).role() == Role::User);
    assert(conversation.at(0).content() == "hello");

    // Repeated appends preserve all entries as the container grows.
    Conversation growing;
    std::size_t expected_capacity = 0;
    for (int i = 0; i < 20; ++i) {
        growing.append(Message(Role::User, "message " + std::to_string(i)));
        if (growing.size() > expected_capacity) {
            expected_capacity = expected_capacity == 0 ? 1 : expected_capacity * 2;
        }
        assert(growing.capacity_ == expected_capacity);
    }
    assert(growing.size() == 20);
    for (std::size_t i = 0; i < growing.size(); ++i) {
        assert(growing.at(i).content() == "message " + std::to_string(i));
    }

    // Copy construction and copy assignment duplicate storage, not just values.
    Conversation original;
    original.append(Message(Role::System, "system message"));
    original.append(Message(Role::Assistant, "assistant message"));
    Conversation copy(original);
    assert(copy.size() == original.size());
    assert(copy.at(0).role() == original.at(0).role());
    assert(copy.at(0).content() == original.at(0).content());
    assert(copy.at(1).role() == original.at(1).role());
    assert(copy.at(1).content() == original.at(1).content());
    assert(copy.begin() != original.begin());

    Conversation source;
    source.append(Message(Role::User, "source user"));
    source.append(Message(Role::Assistant, "source assistant"));
    Conversation destination;
    destination.append(Message(Role::System, "old destination"));
    destination = source;
    assert(destination.size() == source.size());
    assert(destination.at(0).role() == source.at(0).role());
    assert(destination.at(0).content() == source.at(0).content());
    assert(destination.at(1).role() == source.at(1).role());
    assert(destination.at(1).content() == source.at(1).content());
    assert(destination.begin() != source.begin());
    Conversation& self = destination;
    // Self-assignment leaves the destination unchanged.
    destination = self;
    assert(destination.size() == 2);
    assert(destination.at(0).content() == "source user");
    assert(destination.at(1).content() == "source assistant");

    // Move operations transfer the buffer and reset the moved-from object.
    Conversation moving_source;
    moving_source.append(Message(Role::User, "move me"));
    const Message* old_address = moving_source.begin();
    Conversation moved(std::move(moving_source));
    assert(moved.size() == 1);
    assert(moved.begin() == old_address);
    assert(moved.at(0).role() == Role::User);
    assert(moved.at(0).content() == "move me");
    assert(moving_source.size() == 0);
    assert(moving_source.begin() == moving_source.end());

    Conversation move_assignment_source;
    move_assignment_source.append(Message(Role::Assistant, "move assignment"));
    const Message* move_address = move_assignment_source.begin();
    Conversation move_assignment_destination;
    move_assignment_destination.append(Message(Role::System, "replace me"));
    move_assignment_destination = std::move(move_assignment_source);
    assert(move_assignment_destination.size() == 1);
    assert(move_assignment_destination.begin() == move_address);
    assert(move_assignment_destination.at(0).role() == Role::Assistant);
    assert(move_assignment_destination.at(0).content() == "move assignment");
    assert(move_assignment_source.size() == 0);
    assert(move_assignment_source.begin() == move_assignment_source.end());

    // Text without a marker is emitted on flush.
    SentinelScanner clean_scanner("<|end_conversation|>");
    auto clean_out = clean_scanner.feed("Hello there");
    assert(!clean_out.sentinel_found);
    auto clean_tail = clean_scanner.flush();
    assert(!clean_tail.sentinel_found);
    assert(clean_out.safe_text + clean_tail.safe_text == "Hello there");

    // Every possible chunk split must still detect a marker spanning chunks.
    const std::string split_input = "Goodbye.<|end_conversation|>";
    for (std::size_t split = 0; split <= split_input.size(); ++split) {
        SentinelScanner split_scanner("<|end_conversation|>");
        auto first_half = split_scanner.feed(split_input.substr(0, split));
        auto second_half = split_scanner.feed(split_input.substr(split));
        assert(first_half.sentinel_found || second_half.sentinel_found);
        assert(first_half.safe_text + second_half.safe_text == "Goodbye.");
    }

    // Similar-looking text must not be mistaken for the exact marker.
    SentinelScanner false_alarm_scanner("<|end_conversation|>");
    auto false_alarm_out = false_alarm_scanner.feed("Hello <|end_world|>");
    auto false_alarm_tail = false_alarm_scanner.flush();
    assert(!false_alarm_out.sentinel_found);
    assert(!false_alarm_tail.sentinel_found);
    assert(false_alarm_out.safe_text + false_alarm_tail.safe_text ==
           "Hello <|end_world|>");

    // Adversarial one-byte input must preserve the complete safe output.
    std::string adversarial;
    for (int i = 0; i < 250000; ++i) {
        adversarial += "<|end_conversatioX";
    }
    SentinelScanner bounded_scanner("<|end_conversation|>");
    std::string reconstructed;
    bool adversarial_found = false;
    for (std::size_t i = 0; i < adversarial.size(); ++i) {
        auto result = bounded_scanner.feed(
            std::string_view(adversarial.data() + i, 1));
        assert(bounded_scanner.pending_.size() <=
               bounded_scanner.sentinel_.size() - 1);
        reconstructed += result.safe_text;
        adversarial_found = adversarial_found || result.sentinel_found;
    }
    auto adversarial_tail = bounded_scanner.flush();
    reconstructed += adversarial_tail.safe_text;
    adversarial_found = adversarial_found || adversarial_tail.sentinel_found;
    assert(!adversarial_found);
    assert(reconstructed == adversarial);

    const std::string harness_script_path = "/tmp/ece309_harness_test.script";
    write_script(harness_script_path,
                 "role: assistant\n"
                 "fixed reply\n"
                 "---\n");

    // The harness reports its configured turn limit.
    HarnessConfig turn_limit_config;
    turn_limit_config.max_turns = 1;
    Harness harness(std::make_unique<ScriptedModelClient>(harness_script_path),
                    turn_limit_config);
    OneLineInput input;
    CollectingOutput output;
    StopReason turn_limit = harness.run(input, output);
    assert(turn_limit.kind == StopReason::Kind::TurnLimit);

    // EOF is reported as a user exit without requiring a model response.
    Harness eof_harness(
        std::make_unique<ScriptedModelClient>(harness_script_path),
        HarnessConfig{});
    EofInput eof_input;
    CollectingOutput eof_output;
    StopReason eof_stop = eof_harness.run(eof_input, eof_output);
    assert(eof_stop.kind == StopReason::Kind::UserExit);

    const std::string sentinel_script_path = "/tmp/ece309_sentinel_test.script";
    write_script(sentinel_script_path,
                 "chunk: 12\n"
                 "role: assistant\n"
                 "Goodbye.<|end_conversation|>\n"
                 "---\n");

    // A provided scripted client can split the sentinel, and the harness stops
    // generation without printing the sentinel.
    Harness sentinel_harness(
        std::make_unique<ScriptedModelClient>(sentinel_script_path),
        HarnessConfig{});
    OneLineInput sentinel_input;
    CollectingOutput sentinel_output;
    StopReason sentinel_stop = sentinel_harness.run(sentinel_input, sentinel_output);
    assert(sentinel_stop.kind == StopReason::Kind::Sentinel);
    assert(sentinel_output.text().find("Goodbye.") != std::string::npos);
    assert(sentinel_output.text().find("<|end_conversation|>") ==
           std::string::npos);

    // The provided client's public system-message accessor supplies the
    // harness configuration, and the conversation exposes the resulting order.
    const std::string system_script_path = "/tmp/ece309_system_test.script";
    write_script(system_script_path,
                 "role: system\n"
                 "system instructions\n"
                 "---\n"
                 "role: assistant\n"
                 "recorded reply\n"
                 "---\n");
    auto system_model = std::make_unique<ScriptedModelClient>(system_script_path);
    HarnessConfig system_config;
    system_config.max_turns = 1;
    system_config.system_message = system_model->system_message();
    Harness system_harness(std::move(system_model), system_config);
    OneLineInput system_input;
    CollectingOutput system_output;
    system_harness.run(system_input, system_output);
    assert(system_harness.conversation().size() >= 2);
    assert(system_harness.conversation().at(0).role() == Role::System);
    assert(system_harness.conversation().at(0).content() ==
           "system instructions");
    assert(system_harness.conversation().at(1).role() == Role::User);

    // Replay output follows transcript order and invokes completion each time.
    const std::string transcript_path = "/tmp/ece309_replay_test.txt";
    {
        std::ofstream transcript(transcript_path);
        transcript << "role: system\n"
                   << "Be concise.\n"
                   << "---\n"
                   << "role: user\n"
                   << "hello\n"
                   << "---\n"
                   << "role: assistant\n"
                   << "Hi there.\n"
                   << "---\n"
                   << "role: user\n"
                   << "bye\n"
                   << "---\n"
                   << "role: assistant\n"
                   << "Goodbye.\n"
                   << "---\n";
    }
    ReplayModelClient replay(transcript_path);
    assert(replay.system_message() == "Be concise.");
    Conversation replay_conversation;
    CollectingTokenSink token_sink;
    replay.generate(replay_conversation, token_sink);
    assert(token_sink.text == "Hi there.");
    assert(token_sink.completed);
    token_sink.clear();
    replay.generate(replay_conversation, token_sink);
    assert(token_sink.text == "Goodbye.");
    assert(token_sink.completed);
    std::remove(transcript_path.c_str());
    std::remove(harness_script_path.c_str());
    std::remove(sentinel_script_path.c_str());
    std::remove(system_script_path.c_str());

    return 0;
}
