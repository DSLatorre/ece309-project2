#include "core/conversation.h"

#include <memory>
#include <stdexcept>

Conversation::Conversation() = default;

// Allocate matching storage and copy each initialized message.
Conversation::Conversation(const Conversation& other)
	: data_(nullptr), size_(other.size_), capacity_(other.capacity_) {
	if (capacity_ == 0) return;

	std::unique_ptr<Message[]> new_data(new Message[capacity_]);
	for (std::size_t i = 0; i < size_; ++i) {
		new_data[i] = other.data_[i];
	}
	data_ = new_data.release();
}

// Move construction takes the buffer pointer and leaves the source empty.
Conversation::Conversation(Conversation&& other) noexcept
	: data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
	other.data_ = nullptr;
	other.size_ = 0;
	other.capacity_ = 0;
}

// Copy assignment builds replacement storage before changing this object.
Conversation& Conversation::operator=(const Conversation& other) {
	if (this == &other) return *this;

	std::unique_ptr<Message[]> new_data;
	if (other.capacity_ != 0) {
		new_data.reset(new Message[other.capacity_]);
		for (std::size_t i = 0; i < other.size_; ++i) {
			new_data[i] = other.data_[i];
		}
	}

	delete[] data_;
	data_ = new_data.release();
	size_ = other.size_;
	capacity_ = other.capacity_;
	return *this;
}

// Move assignment discards the old buffer and transfers the source buffer.
Conversation& Conversation::operator=(Conversation&& other) noexcept {
	if (this == &other) return *this;

	delete[] data_;
	data_ = other.data_;
	size_ = other.size_;
	capacity_ = other.capacity_;

	other.data_ = nullptr;
	other.size_ = 0;
	other.capacity_ = 0;
	return *this;
}

// The array was allocated with new[], so it must be released with delete[].
Conversation::~Conversation() {
	delete[] data_;
}

// Return the logical element count, not the allocated capacity.
std::size_t Conversation::size() const noexcept {
	return size_;
}

// Bounds checking keeps invalid indexes from accessing the raw array.
const Message& Conversation::at(std::size_t i) const {
	if (i >= size_) {
		throw std::out_of_range("Conversation index out of range");
	}

	return data_[i];
}

// These pointer endpoints allow read-only iteration over initialized elements.
const Message* Conversation::begin() const noexcept {
	return data_;
}

const Message* Conversation::end() const noexcept {
	return data_ == nullptr ? nullptr : data_ + size_;
}

void Conversation::append(Message m) {
	if (size_ == capacity_) {
		// Doubling capacity gives amortized constant-time appends.
		const std::size_t new_capacity = capacity_ == 0 ? 1 : capacity_ * 2;
		std::unique_ptr<Message[]> new_data(new Message[new_capacity]);

		for (std::size_t i = 0; i < size_; ++i) {
			new_data[i] = data_[i];
		}

		delete[] data_;
		data_ = new_data.release();
		capacity_ = new_capacity;
	}

	// The slot is available after the growth check above.
	data_[size_] = m;
	++size_;
}
