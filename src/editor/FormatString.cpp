#include "StringUtils.h"
#include <cstdio>
#include <cstdarg>

namespace sg {
	std::string FormatString(const char* msg, ...) {
		char buf[1024];

		va_list args1, args2;
		va_start (args1, msg);
		va_copy (args1, args2);

		const int required_count = std::vsnprintf(buf, sizeof(buf), msg, args1);
		va_end (args1);

		if (required_count+1 < sizeof(buf)) {
			va_end(args2);
			return buf;
		}

		std::string result;
		result.resize(required_count+1);

		std::vsnprintf(result.data(), required_count+1, msg, args2);
		va_end (args2)

		return result;
	}
}
