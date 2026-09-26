#include "core/sentinel_scanner.h"

#include <algorithm>

SentinelScanner::SentinelScanner(std::string sentinel)
	: sentinel_(sentinel) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
	// Include the previous suffix so a marker split across chunks can match.
	std::string combined = pending_;
	combined += chunk;

	const std::size_t sentinel_position = combined.find(sentinel_);
	if (sentinel_position != std::string::npos) {
		// Once found, discard the marker and all text after it from the stream.
		pending_.clear();
		return {combined.substr(0, sentinel_position), true};
	}

	// Only a suffix shorter than the marker can still become a future match.
	const std::size_t max_pending = sentinel_.empty() ? 0 : sentinel_.size() - 1;
	const std::size_t max_candidate = std::min(max_pending, combined.size());
	std::size_t pending_size = 0;
	for (std::size_t candidate = max_candidate; candidate > 0; --candidate) {
		// Prefer the longest suffix that is also a prefix of the marker.
		if (combined.compare(combined.size() - candidate, candidate,
		                     sentinel_, 0, candidate) == 0) {
			pending_size = candidate;
			break;
		}
	}

	// Emit everything that cannot participate in a marker match yet.
	const std::size_t safe_size = combined.size() - pending_size;
	pending_ = combined.substr(safe_size);
	return {combined.substr(0, safe_size), false};
}

SentinelScanner::Out SentinelScanner::flush() {
	// End-of-stream makes the retained suffix ordinary text rather than a match.
	Out result{pending_, false};
	pending_.clear();
	return result;
}
