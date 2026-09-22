#include "core/sentinel_scanner.h"

#include <algorithm>

SentinelScanner::SentinelScanner(std::string sentinel)
	: sentinel_(sentinel) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
	std::string combined = pending_;
	combined += chunk;

	const std::size_t sentinel_position = combined.find(sentinel_);
	if (sentinel_position != std::string::npos) {
		pending_.clear();
		return {combined.substr(0, sentinel_position), true};
	}

	const std::size_t max_pending = sentinel_.empty() ? 0 : sentinel_.size() - 1;
	const std::size_t max_candidate = std::min(max_pending, combined.size());
	std::size_t pending_size = 0;
	for (std::size_t candidate = max_candidate; candidate > 0; --candidate) {
		if (combined.compare(combined.size() - candidate, candidate,
		                     sentinel_, 0, candidate) == 0) {
			pending_size = candidate;
			break;
		}
	}

	const std::size_t safe_size = combined.size() - pending_size;
	pending_ = combined.substr(safe_size);
	return {combined.substr(0, safe_size), false};
}

SentinelScanner::Out SentinelScanner::flush() {
	Out result{pending_, false};
	pending_.clear();
	return result;
}
