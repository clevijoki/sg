#include "Result.h"
#include <gtest/gtest.h>

namespace sg {

	namespace test {

		Result<> ReturnSuccessVoid() {
			return Ok();
		}

		Result<QString> ReturnSuccessValue() {
			QString res = "Succeeded";
			return Ok(std::move(res));
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

		EXPECT_STREQ("Error Message", ReturnFailed().errorMessage().toStdString().c_str());
		EXPECT_STREQ("Error Message", PropagateError().errorMessage().toStdString().c_str());	

		EXPECT_STREQ("Succeeded", ReturnSuccessValue()->toStdString().c_str());
	}
}