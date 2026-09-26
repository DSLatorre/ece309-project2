#ifndef CORE_SENTINEL_SCANNER_H
#define CORE_SENTINEL_SCANNER_H

#include <string>
#include <string_view>

// Detects a sentinel in streamed text without requiring complete chunks.
class SentinelScanner {
public:
	// Holds text that is safe to emit and whether the sentinel was found.
	struct Out {
		std::string safe_text;
		bool sentinel_found;
	};

	// Store the marker that terminates the streamed response.
	SentinelScanner(std::string sentinel);

	// Process one chunk, retaining only a possible partial sentinel suffix.
	Out feed(std::string_view chunk);
	// Emit any retained text when no more chunks will arrive.
	Out flush();

private:
	// Marker to search for in the incoming stream.
	std::string sentinel_;
	// Suffix that might become the beginning of the marker in a later chunk.
	std::string pending_;
};

#endif
