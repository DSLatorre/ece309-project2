// tests/p2/test_p2.cpp
//
// YOUR test suite goes here. At least 12 assert-based test cases — see
// spec §5 for the required categories and the sample test for the
// expected level of rigor.
//
// This file is a stub so the project builds out of the box; replace the
// body of main() with your own tests.

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
class FixedModelClient : public ModelClient {
public:
    void generate(const Conversation&, TokenSink& sink) override {
        sink.on_chunk("fixed reply");
        sink.on_complete();
    }
};

class SplitSentinelModelClient : public ModelClient {
public:
    void generate(const Conversation&, TokenSink& sink) override {
        sink.on_chunk("Goodbye.<|end_");
        sink.on_chunk("conversation|>");
        sink.on_complete();
    }
};

class RecordingModelClient : public ModelClient {
public:
    void generate(const Conversation& conversation, TokenSink& sink) override {
        system_message_first =
            conversation.size() >= 2 &&
            conversation.at(0).role() == Role::System &&
            conversation.at(0).content() == "system instructions" &&
            conversation.at(1).role() == Role::User;
        sink.on_chunk("recorded reply");
        sink.on_complete();
    }

    bool system_message_first = false;
};

class OneLineInput : public InputSource {
public:
    std::string read_line() override { return "hello"; }
    bool is_eof() const override { return false; }
};

class EofInput : public InputSource {
public:
    std::string read_line() override { return ""; }
    bool is_eof() const override { return true; }
};

class CollectingOutput : public OutputSink {
public:
    void write(std::string_view text) override { text_ += text; }
    const std::string& text() const { return text_; }

private:
    std::string text_;
};

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

    Conversation conversation;
    conversation.append(Message(Role::User, "hello"));
    assert(conversation.size() == 1);
    assert(conversation.at(0).role() == Role::User);
    assert(conversation.at(0).content() == "hello");

    Conversation growing;
    for (int i = 0; i < 20; ++i) {
        growing.append(Message(Role::User, "message " + std::to_string(i)));
    }
    assert(growing.size() == 20);
    assert(growing.capacity_ == 32);
    for (std::size_t i = 0; i < growing.size(); ++i) {
        assert(growing.at(i).content() == "message " + std::to_string(i));
    }

    Conversation capacity_check;
    assert(capacity_check.capacity_ == 0);
    capacity_check.append(Message(Role::User, "0"));
    assert(capacity_check.capacity_ == 1);
    capacity_check.append(Message(Role::User, "1"));
    assert(capacity_check.capacity_ == 2);
    capacity_check.append(Message(Role::User, "2"));
    assert(capacity_check.capacity_ == 4);

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
    destination = self;
    assert(destination.size() == 2);
    assert(destination.at(0).content() == "source user");
    assert(destination.at(1).content() == "source assistant");

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

    SentinelScanner clean_scanner("<|end_conversation|>");
    auto clean_out = clean_scanner.feed("Hello there");
    assert(!clean_out.sentinel_found);
    auto clean_tail = clean_scanner.flush();
    assert(!clean_tail.sentinel_found);
    assert(clean_out.safe_text + clean_tail.safe_text == "Hello there");

    const std::string split_input = "Goodbye.<|end_conversation|>";
    for (std::size_t split = 0; split <= split_input.size(); ++split) {
        SentinelScanner split_scanner("<|end_conversation|>");
        auto first_half = split_scanner.feed(split_input.substr(0, split));
        auto second_half = split_scanner.feed(split_input.substr(split));
        assert(first_half.sentinel_found || second_half.sentinel_found);
        assert(first_half.safe_text + second_half.safe_text == "Goodbye.");
    }

    SentinelScanner false_alarm_scanner("<|end_conversation|>");
    auto false_alarm_out = false_alarm_scanner.feed("Hello <|end_world|>");
    auto false_alarm_tail = false_alarm_scanner.flush();
    assert(!false_alarm_out.sentinel_found);
    assert(!false_alarm_tail.sentinel_found);
    assert(false_alarm_out.safe_text + false_alarm_tail.safe_text ==
           "Hello <|end_world|>");

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
        reconstructed += result.safe_text;
        adversarial_found = adversarial_found || result.sentinel_found;
        assert(bounded_scanner.pending_.size() <=
               bounded_scanner.sentinel_.size() - 1);
    }
    auto adversarial_tail = bounded_scanner.flush();
    reconstructed += adversarial_tail.safe_text;
    adversarial_found = adversarial_found || adversarial_tail.sentinel_found;
    assert(!adversarial_found);
    assert(reconstructed == adversarial);

    SentinelScanner pending_bound_scanner("<|end_conversation|>");
    for (std::size_t i = 0; i < pending_bound_scanner.sentinel_.size(); ++i) {
        pending_bound_scanner.feed(
            std::string_view(pending_bound_scanner.sentinel_.data() + i, 1));
        assert(pending_bound_scanner.pending_.size() <=
               pending_bound_scanner.sentinel_.size() - 1);
    }

    HarnessConfig turn_limit_config;
    turn_limit_config.max_turns = 1;
    Harness harness(std::make_unique<FixedModelClient>(), turn_limit_config);
    OneLineInput input;
    CollectingOutput output;
    StopReason turn_limit = harness.run(input, output);
    assert(turn_limit.kind == StopReason::Kind::TurnLimit);

    Harness eof_harness(std::make_unique<FixedModelClient>(), HarnessConfig{});
    EofInput eof_input;
    CollectingOutput eof_output;
    StopReason eof_stop = eof_harness.run(eof_input, eof_output);
    assert(eof_stop.kind == StopReason::Kind::UserExit);

    Harness sentinel_harness(
        std::make_unique<SplitSentinelModelClient>(), HarnessConfig{});
    OneLineInput sentinel_input;
    CollectingOutput sentinel_output;
    StopReason sentinel_stop = sentinel_harness.run(sentinel_input, sentinel_output);
    assert(sentinel_stop.kind == StopReason::Kind::Sentinel);
    assert(sentinel_output.text().find("Goodbye.") != std::string::npos);
    assert(sentinel_output.text().find("<|end_conversation|>") ==
           std::string::npos);

    auto recording_model = std::make_unique<RecordingModelClient>();
    RecordingModelClient* recording_model_ptr = recording_model.get();
    HarnessConfig system_config;
    system_config.max_turns = 1;
    system_config.system_message = "system instructions";
    Harness system_harness(std::move(recording_model), system_config);
    OneLineInput system_input;
    CollectingOutput system_output;
    system_harness.run(system_input, system_output);
    assert(recording_model_ptr->system_message_first);

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

    return 0;
}
