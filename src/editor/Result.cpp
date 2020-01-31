#include "Result.h"
#include <gtest/gtest.h>
#include <cstdarg>
#include <cstdlib>

namespace sg {

	void FatalError(const char* msg, ...) {
		va_list args;
		va_start (args, msg);

		vprintf(msg, args);

		va_end(args);

		std::quick_exit(-1);
	}

	namespace test {

		Result<> ReturnSuccessVoid() {
			return Ok();
		}

		Result<std::string> ReturnSuccessValue() {
			return Ok<std::string>("Succeeded");
		}

		Result<> ReturnFailed() {
			return Error("Error Message");
		}

		Result<> PropagateError() {
			auto res = ReturnFailed();
			if (res.failed())
				return res.error();

			return Ok();
		}
	}

	TEST(Result, Usage) {
		using namespace test;

		EXPECT_FALSE(ReturnSuccessVoid().failed());
		EXPECT_FALSE(ReturnSuccessValue().failed());
		EXPECT_TRUE(ReturnFailed().failed());

		EXPECT_TRUE(PropagateError().failed());

		EXPECT_STREQ("Error Message", ReturnFailed().errorMessage().c_str());
		EXPECT_STREQ("Error Message", PropagateError().errorMessage().c_str());	

		EXPECT_STREQ("Succeeded", ReturnSuccessValue()->c_str());
	}
}