#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rmb {

struct Settings {
    std::filesystem::path baseDir = ".";
    std::filesystem::path scheduleFile = "config/schedule.csv";
    std::filesystem::path stateFile = "data/state.csv";
    std::filesystem::path airAlertStateFile = "data/air_alert_state.conf";
    std::filesystem::path logFile = "logs/ringmabell.log";
    std::filesystem::path lockFile = "data/ringmabell.lock";

    std::string audioBackend = "auto";
    std::string audioCommand;

    int pollIntervalMs = 500;
    int missedGraceSeconds = 300;
    int playbackRetries = 2;
    int retryDelaySeconds = 5;
    bool bellWeekendsEnabled = false;

    bool airAlertsEnabled = true;
    std::string airAlertsEndpoint = "https://ubilling.net.ua/aerialalerts/";
    int airAlertsPollSeconds = 3;
    int airAlertsHttpTimeoutSeconds = 2;
    bool airAlertsNotifyActiveOnStartup = true;
    std::filesystem::path airAlertStartSound = "sounds/air-alert-start.mp3";
    std::filesystem::path airAlertEndSound = "sounds/air-alert-end.mp3";

    bool catchUpMissedBells = true;
    bool markExpiredAsSkipped = true;
    bool requireExistingSound = true;
};

struct ConfigResult {
    Settings settings;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

ConfigResult loadSettings(const std::filesystem::path& configPath);

} // namespace rmb
