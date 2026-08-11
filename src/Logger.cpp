#include "Logger.h"

#include "Utils.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace rmb {

Logger::Logger(const fs::path& path, bool console) : console_(console) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    file_.open(path, std::ios::app);
}

Logger::~Logger() = default;

void Logger::info(const std::string& message) {
    log("INFO", message);
}

void Logger::warn(const std::string& message) {
    log("WARN", message);
}

void Logger::error(const std::string& message) {
    log("ERROR", message);
}

void Logger::log(const char* level, const std::string& message) {
    const std::string line = timestampNow() + " [" + level + "] " + message;
    std::lock_guard<std::mutex> lock(mutex_);

    if (console_) {
        if (std::string(level) == "ERROR") {
            std::cerr << line << '\n';
        } else {
            std::cout << line << '\n';
        }
    }

    if (file_) {
        file_ << line << '\n';
        file_.flush();
    }
}

} // namespace rmb
