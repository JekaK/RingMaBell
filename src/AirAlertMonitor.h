#pragma once

#include "AudioPlayer.h"
#include "Config.h"
#include "HttpClient.h"
#include "Logger.h"

#include <atomic>
#include <ctime>
#include <string>

namespace rmb {

class AirAlertMonitor {
public:
    AirAlertMonitor(const Settings& settings, Logger& logger);

    bool load();
    bool enabled() const;
    int validate(bool checkSounds) const;
    void printStatus() const;
    int checkNow();
    void run(std::atomic_bool& stopRequested);

private:
    struct RemoteStatus {
        bool ok = false;
        char code = '?';
        bool active = false;
        std::string error;
    };

    RemoteStatus fetchStatus() const;
    void handleStatus(const RemoteStatus& status, bool printOnly);
    bool notify(const std::string& eventName, const std::filesystem::path& sound);
    bool saveState(const std::string& status, char rawCode);

    const Settings& settings_;
    Logger& logger_;
    AudioPlayer player_;
    HttpClient http_;

    bool stateLoaded_ = false;
    std::string lastStatus_ = "unknown";
    char lastRawCode_ = '?';
    std::time_t lastUpdatedAt_ = 0;
};

} // namespace rmb
