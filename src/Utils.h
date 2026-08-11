#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace rmb {

std::string trim(const std::string& value);
std::string toLower(std::string value);
std::vector<std::string> splitAny(const std::string& value, const std::string& separators);
bool parseBool(const std::string& value, bool& out);
bool parseSeconds(const std::string& value, int& seconds);

std::tm localTime(std::time_t time);
std::string timestampNow();
std::string dateKey(const std::tm& local);
int secondsOfDay(const std::tm& local);
bool isWeekend(const std::tm& local);
std::string formatSeconds(int seconds);

std::string shellQuote(const std::string& value);
bool commandExists(const std::string& command);

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value);
std::string wideToUtf8(const std::wstring& value);
#endif

} // namespace rmb
