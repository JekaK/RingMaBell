#include "Utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rmb {

std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> splitAny(const std::string& value, const std::string& separators) {
    std::vector<std::string> result;
    std::string current;
    for (const char c : value) {
        if (separators.find(c) != std::string::npos) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

bool parseBool(const std::string& value, bool& out) {
    const auto normalized = toLower(trim(value));
    if (normalized == "true" || normalized == "yes" || normalized == "y" || normalized == "1" || normalized == "on") {
        out = true;
        return true;
    }
    if (normalized == "false" || normalized == "no" || normalized == "n" || normalized == "0" || normalized == "off") {
        out = false;
        return true;
    }
    return false;
}

bool parseSeconds(const std::string& value, int& seconds) {
    const auto parts = splitAny(trim(value), ":");
    if (parts.size() != 2 && parts.size() != 3) {
        return false;
    }

    try {
        const int hour = std::stoi(parts[0]);
        const int minute = std::stoi(parts[1]);
        const int second = parts.size() == 3 ? std::stoi(parts[2]) : 0;
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
            return false;
        }
        seconds = hour * 3600 + minute * 60 + second;
        return true;
    } catch (...) {
        return false;
    }
}

std::tm localTime(std::time_t time) {
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &time);
#else
    localtime_r(&time, &out);
#endif
    return out;
}

std::string timestampNow() {
    const auto now = std::time(nullptr);
    const auto local = localTime(now);
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string dateKey(const std::tm& local) {
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d");
    return out.str();
}

int secondsOfDay(const std::tm& local) {
    return local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
}

bool isWeekend(const std::tm& local) {
    return local.tm_wday == 0 || local.tm_wday == 6;
}

std::string formatSeconds(int seconds) {
    const int hour = seconds / 3600;
    const int minute = (seconds % 3600) / 60;
    const int second = seconds % 60;

    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << hour << ':'
        << std::setw(2) << minute << ':'
        << std::setw(2) << second;
    return out.str();
}

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string out = "\"";
    for (const char c : value) {
        if (c == '"') {
            out += "\\\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
#else
    std::string out = "'";
    for (const char c : value) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
#endif
}

bool commandExists(const std::string& command) {
#ifdef _WIN32
    (void)command;
    return false;
#else
    const std::string check = "command -v " + shellQuote(command) + " >/dev/null 2>&1";
    return std::system(check.c_str()) == 0;
#endif
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}
#endif

} // namespace rmb
