#pragma once

#include <mongocxx/database.hpp>

namespace mongo_schema {
void initialize(mongocxx::database& db);
}
