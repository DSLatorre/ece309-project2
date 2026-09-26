#ifndef CORE_CONVERSATION_H
#define CORE_CONVERSATION_H

#include "core/message.h"

#include <cstddef>

// Owns an ordered, growable sequence of conversation messages.
class Conversation {
public:
	// Start with no allocated message storage.
	Conversation();
	// Release the storage owned by this conversation.
	~Conversation();

	// Copy the messages and allocated capacity from another conversation.
	Conversation(const Conversation& other);
	// Replace this conversation with an independent copy of another one.
	Conversation& operator=(const Conversation& other);

	// Transfer storage ownership from another conversation without copying.
	Conversation(Conversation&& other) noexcept;
	// Release current storage and take ownership of another conversation's data.
	Conversation& operator=(Conversation&& other) noexcept;

	// Append a message, growing the backing array when it is full.
	void append(Message m);

	// Return the number of messages currently stored.
	std::size_t size() const noexcept;

	// Return the message at an index, or throw when the index is out of bounds.
	const Message& at(std::size_t i) const;

	// Return a pointer to the first message, or nullptr for an empty allocation.
	const Message* begin() const noexcept;
	// Return one past the last message, suitable for range-based iteration.
	const Message* end() const noexcept;

private:
	// Dynamically allocated message array owned by this object.
	Message* data_ = nullptr;
	// Number of initialized message elements in data_.
	std::size_t size_ = 0;
	// Number of message slots currently allocated in data_.
	std::size_t capacity_ = 0;
};

#endif
