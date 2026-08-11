#include "Config.h"

#include "Utils.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace rmb {

namespace {

fs::path resolvePath(const fs::path& baseDir, const std::string& value) {
    fs::path path = trim(value);
    if (path.is_absolute()) {
        return path.lexically_normal();
    }
    return (baseDir / path).lexically_normal();
}

bool parseIntField(const std::string& value, int& out) {
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(trim(value), &consumed);
        if (consumed != trim(value).size()) {
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

ConfigResult loadSettings(const fs::path& configPath) {
    ConfigResult result;
    result.settings.baseDir = fs::current_path();

    std::ifstream input(configPath);
    if (!input) {
        result.errors.push_back("cannot open settings file: " + configPath.string());
        return result;
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            result.errors.push_back("settings line " + std::to_string(lineNumber) + " must be key=value");
            continue;
        }

        const auto key = toLower(trim(line.substr(0, eq)));
        const auto value = trim(line.substr(eq + 1));
        values[key] = value;
    }

    if (auto it = values.find("base_dir"); it != values.end()) {
        fs::path base = trim(it->second);
        if (!base.is_absolute()) {
            base = (configPath.parent_path() / base).lexically_normal();
        }
        result.settings.baseDir = base;
    }

    const auto& base = result.settings.baseDir;

    if (auto it = values.find("schedule_file"); it != values.end()) {
        result.settings.scheduleFile = resolvePath(base, it->second);
    } else {
        result.settings.scheduleFile = resolvePath(base, result.settings.scheduleFile.string());
    }

    if (auto it = values.find("state_file"); it != values.end()) {
        result.settings.stateFile = resolvePath(base, it->second);
    } else {
        result.settings.stateFile = resolvePath(base, result.settings.stateFile.string());
    }

    if (auto it = values.find("air_alerts_state_file"); it != values.end()) {
        result.settings.airAlertStateFile = resolvePath(base, it->second);
    } else {
        result.settings.airAlertStateFile = resolvePath(base, result.settings.airAlertStateFile.string());
    }

    if (auto it = values.find("log_file"); it != values.end()) {
        result.settings.logFile = resolvePath(base, it->second);
    } else {
        result.settings.logFile = resolvePath(base, result.settings.logFile.string());
    }

    if (auto it = values.find("lock_file"); it != values.end()) {
        result.settings.lockFile = resolvePath(base, it->second);
    } else {
        result.settings.lockFile = resolvePath(base, result.settings.lockFile.string());
    }

    if (auto it = values.find("audio_backend"); it != values.end()) {
        result.settings.audioBackend = toLower(trim(it->second));
    }

    if (auto it = values.find("audio_command"); it != values.end()) {
        result.settings.audioCommand = trim(it->second);
    }

    if (auto it = values.find("air_alerts_endpoint"); it != values.end()) {
        result.settings.airAlertsEndpoint = trim(it->second);
    }

    if (auto it = values.find("air_alert_start_sound"); it != values.end()) {
        result.settings.airAlertStartSound = resolvePath(base, it->second);
    } else {
        result.settings.airAlertStartSound = resolvePath(base, result.settings.airAlertStartSound.string());
    }

    if (auto it = values.find("air_alert_end_sound"); it != values.end()) {
        result.settings.airAlertEndSound = resolvePath(base, it->second);
    } else {
        result.settings.airAlertEndSound = resolvePath(base, result.settings.airAlertEndSound.string());
    }

    const std::vector<std::pair<std::string, int Settings::*>> intFields = {
        {"poll_interval_ms", &Settings::pollIntervalMs},
        {"missed_grace_seconds", &Settings::missedGraceSeconds},
        {"playback_retries", &Settings::playbackRetries},
        {"retry_delay_seconds", &Settings::retryDelaySeconds},
        {"air_alerts_poll_seconds", &Settings::airAlertsPollSeconds},
        {"air_alerts_http_timeout_seconds", &Settings::airAlertsHttpTimeoutSeconds},
    };

    for (const auto& field : intFields) {
        if (auto it = values.find(field.first); it != values.end()) {
            int parsed = 0;
            if (!parseIntField(it->second, parsed) || parsed < 0) {
                result.errors.push_back("invalid integer for " + field.first + ": " + it->second);
            } else {
                result.settings.*(field.second) = parsed;
            }
        }
    }

    const std::vector<std::pair<std::string, bool Settings::*>> boolFields = {
        {"catch_up_missed_bells", &Settings::catchUpMissedBells},
        {"mark_expired_as_skipped", &Settings::markExpiredAsSkipped},
        {"require_existing_sound", &Settings::requireExistingSound},
        {"bell_weekends_enabled", &Settings::bellWeekendsEnabled},
        {"air_alerts_enabled", &Settings::airAlertsEnabled},
        {"air_alerts_notify_active_on_startup", &Settings::airAlertsNotifyActiveOnStartup},
    };

    for (const auto& field : boolFields) {
        if (auto it = values.find(field.first); it != values.end()) {
            bool parsed = false;
            if (!parseBool(it->second, parsed)) {
                result.errors.push_back("invalid boolean for " + field.first + ": " + it->second);
            } else {
                result.settings.*(field.second) = parsed;
            }
        }
    }

    if (result.settings.pollIntervalMs < 100) {
        result.warnings.push_back("poll_interval_ms is very low; using 100 ms");
        result.settings.pollIntervalMs = 100;
    }

    if (result.settings.airAlertsPollSeconds < 3) {
        result.warnings.push_back("air_alerts_poll_seconds is below the Ubilling cache window; using 3 seconds");
        result.settings.airAlertsPollSeconds = 3;
    }

    if (result.settings.airAlertsHttpTimeoutSeconds < 1) {
        result.warnings.push_back("air_alerts_http_timeout_seconds is too low; using 1 second");
        result.settings.airAlertsHttpTimeoutSeconds = 1;
    }

    return result;
}

} // namespace rmb
