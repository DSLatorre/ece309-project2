#ifndef CORE_SENTINEL_SCANNER_H
#define CORE_SENTINEL_SCANNER_H

#include <string>
#include <string_view>

class SentinelScanner {
public:
	struct Out {
		std::string safe_text;
		bool sentinel_found;
	};

	SentinelScanner(std::string sentinel);

	Out feed(std::string_view chunk);
	Out flush();

private:
	std::string sentinel_;
	std::string pending_;
};

#endif
