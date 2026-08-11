#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace rmb {

class Logger {
public:
    explicit Logger(const std::filesystem::path& path, bool console = true);
    ~Logger();

    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    void log(const char* level, const std::string& message);

    std::ofstream file_;
    std::mutex mutex_;
    bool console_ = true;
};

} // namespace rmb
