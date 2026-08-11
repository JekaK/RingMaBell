#pragma once

#include <string>
#include <vector>

namespace rmb {

std::vector<std::string> parseCsvLine(const std::string& line, bool& ok, std::string& error);
std::string csvEscape(const std::string& value);

} // namespace rmb
