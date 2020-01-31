#pragma once
#include "FormatString.h"

namespace sg {

	void FatalError(const char* msg, ...);

	class VoidType {};
	class ResultError;
	template<typename T = VoidType> class Result;

	template<typename U> Result<U> Ok(U value);
	Result<> Ok();

	ResultError Error(StringBuilder message, StringBuilder info);

	class ResultError {
		template<typename U> friend class Result;

		friend ResultError Error(StringBuilder, StringBuilder);
		std::string message;
		std::string info;
	};

	template<typename T>
	class Result
	{
		template<typename U>
		friend Result<U> Ok(U value);

		friend Result<VoidType> Ok();

		union {
			T mData;
			ResultError mError;
		};

		enum class Status {
			Success,
			Failure
		};

		const Status mStatus;

		Result() : mStatus(Status::Success) {}

	public:

		Result(Result&& other)
		: mStatus(other.mStatus) {
			if (mStatus == Status::Failure) {
				new (&mError) ResultError(std::move(other.mError));
			} else {
				new (&mData) T(std::move(other.mData));
			}
		}

		Result(const Result& other)
		: mStatus(other.mStatus) {
			if (mStatus == Status::Failure) {
				new (&mError) ResultError(other.mError);
			} else {
				new (&mData) T(other.mData);
			}
		}

		Result(ResultError other) 
		: mStatus(Status::Failure) {
			new (&mError) ResultError(std::move(other));
		}

		~Result() {
			if (mStatus == Status::Failure) {
				mError.~ResultError();
			} else {
				mData.~T();
			}
		}

		const Result& operator=(const Result& other) {
			this->~Result();
			new (this) Result(other);
			return *this;
		}

		const Result& operator=(Result&& other) {
			this->~Result();
			new (this) Result(other);
			return *this;
		}

		const Result& operator=(ResultError other) {
			this->~Result();
			new (this) Result(other);
			return *this;
		}

		bool failed() const {
			return mStatus == Status::Failure;
		};

		ResultError error() {
			if (mStatus == Status::Success) {
				FatalError("Result succeeded");
			}
			return std::move(mError);
		}

		ResultError errorCopy() {
			if (mStatus == Status::Success) {
				FatalError("Result succeeded");
			}
			return mError;
		}

		const std::string& errorMessage() {
			if (mStatus == Status::Success) {
				FatalError("Result succeeded");
			}
			return mError.message;
		}

		const std::string& errorInfo() {
			if (mStatus == Status::Success) {
				FatalError("Result succeeded");
			}
			return mError.info;			
		}

		void verify() {
			if (failed()) {
				FatalError("Result failed: %s", mError.message.c_str());
			}
		}

		T* operator->() {
			verify();
			return &mData;
		}

		const T* operator->() const {
			verify();
			return &mData;			
		}

		T& operator*() {
			verify();
			return mData;
		}

		const T& operator*() const {
			verify();
			return mData;
		}
	};

	template<typename T>
	inline Result<T> Ok(T value) {
		Result<T> res;
		new (&res.mData) T(std::move(value));
		return res;
	}

	inline Result<> Ok() {
		Result<> res;
		return res;
	}

	inline ResultError Error(StringBuilder error_message, StringBuilder error_info = StringBuilder()) {
		ResultError res;
		res.message = error_message.take();
		res.info = error_info.take();
		return res;
	}
}

// for google testing
#define EXPECT_OK(value) { \
	auto __test_res = (value); \
	EXPECT_FALSE(__test_res.failed()) << __test_res.errorMessage().toStdString().c_str(); \
}
