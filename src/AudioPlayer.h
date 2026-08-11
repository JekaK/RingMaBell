#pragma once

#include "Config.h"
#include "Logger.h"

#include <filesystem>
#include <string>

namespace rmb {

struct PlayResult {
    bool success = false;
    int exitCode = -1;
    std::string message;
};

class AudioPlayer {
public:
    AudioPlayer(const Settings& settings, Logger& logger);

    PlayResult play(const std::filesystem::path& file) const;

private:
    PlayResult playWithBackend(const std::string& backend, const std::filesystem::path& file) const;
    PlayResult playWithCommand(const std::filesystem::path& file) const;
    PlayResult runCommand(const std::string& command) const;

#ifdef _WIN32
    PlayResult playWithMci(const std::filesystem::path& file) const;
#endif

    const Settings& settings_;
    Logger& logger_;
};

} // namespace rmb
