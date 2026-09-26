#ifndef CORE_MESSAGE_H
#define CORE_MESSAGE_H

#include <string>

// Identifies the speaker associated with a message.
enum class Role {
	System,
	User,
	Assistant
};

// Stores one role-labeled piece of conversation text.
class Message {
public:
	// Create an empty system message.
	Message() : role_(Role::System), content_() {}
	// Create a message with the supplied speaker role and text.
	Message(Role role, std::string content) : role_(role), content_(content) {}

    // Return the message's speaker role without modifying the message.
    Role role() const noexcept { return role_; }
    // Return the message text without copying it.
    const std::string& content() const noexcept { return content_; }

private:
	// Speaker role and text are kept together as one conversation entry.
	Role role_;
	std::string content_;
};

#endif
