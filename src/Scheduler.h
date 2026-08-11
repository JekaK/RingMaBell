#pragma once

#include "AirAlertMonitor.h"
#include "AudioPlayer.h"
#include "Config.h"
#include "Logger.h"
#include "Schedule.h"
#include "StateStore.h"

#include <atomic>
#include <filesystem>
#include <vector>

namespace rmb {

class Scheduler {
public:
    Scheduler(Settings settings, Logger& logger, std::atomic_bool& stopRequested);

    bool load();
    int validate(bool checkSounds) const;
    void printToday() const;
    int checkAirAlertNow();
    void run();
    void tick(std::time_t now);

private:
    bool reloadIfChanged();
    bool playEvent(const BellEvent& event, const std::string& date);

    Settings settings_;
    Logger& logger_;
    std::atomic_bool& stopRequested_;
    AudioPlayer player_;
    StateStore state_;
    AirAlertMonitor airAlerts_;
    std::vector<BellEvent> events_;
    std::filesystem::file_time_type scheduleMtime_{};
};

} // namespace rmb
