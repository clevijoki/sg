#pragma once
#include <string>
#include <vector>
#include <QString>

namespace sg {

	class StringBuilder {
		std::string mValue;

		friend StringBuilder operator+(StringBuilder, const std::string&);
		friend StringBuilder operator+(StringBuilder, const QString&);
		friend StringBuilder operator+(StringBuilder, const char*);
		friend StringBuilder operator "" _sb(const char* str, size_t len);

	public:
		const char* c_str() const {
			return mValue.c_str();
		}

		size_t length() const {
			return mValue.length();
		}

		StringBuilder() {
		}

		StringBuilder(std::string s)
		: mValue(std::move(s)) {
		}

		StringBuilder(const QString& s) 
		: mValue (s.toStdString()) {
		}

		StringBuilder(const char* s)
		: mValue (s) {
		}

		std::string take() {
			return std::move(mValue);
		} 
	};

	inline StringBuilder operator+(StringBuilder lhs, const std::string& rhs) {

		StringBuilder res = std::move(lhs);
		res.mValue += rhs;
		return res;
	}
	
	inline StringBuilder operator+(StringBuilder lhs, const QString& rhs) {
		StringBuilder res = std::move(lhs);
		res.mValue += rhs.toStdString();
		return res;
	}
	
	inline StringBuilder operator+(StringBuilder lhs, const char* rhs) {
		StringBuilder res = std::move(lhs);
		res.mValue += rhs;
		return res;
	}
	
	template<typename T>
	inline void operator+=(StringBuilder& lhs, const T& rhs) {

		lhs = lhs + rhs;
	}
	
	inline StringBuilder operator "" _sb(const char* str, size_t len) {
		StringBuilder res;
		res.mValue.assign(str, len);
		return res;
	}

	std::string StringJoin(const std::vector<std::string>& strings, const std::string& sep);
}