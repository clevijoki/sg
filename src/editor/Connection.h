#pragma once

#include <QSqlDatabase>
#include "Result.h"

namespace sg {

	Result<QSqlDatabase> CreateConnection();
}