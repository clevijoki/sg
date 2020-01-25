#pragma once

#include "Result.h"
class QString;

namespace sg {

	Result<> GenerateComponentFiles(const class Transaction& t, const QString& header_path, const QString& source_path);	
}
