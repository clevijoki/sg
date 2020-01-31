#pragma once

#include "Result.h"
#include <string>

namespace sg {

	Result<> GenerateComponentFiles(const class Transaction& t, const std::string& header_path, const std::string& source_path);	
}
