#pragma once

#include <ctime>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace rmb {

struct DayRule {
    bool all = false;
    std::set<int> weekdays;
    std::set<std::string> includeDates;
    std::set<std::string> excludeDates;

    bool matches(const std::tm& local) const;
};

struct BellEvent {
    std::string id;
    DayRule days;
    int secondsAfterMidnight = 0;
    std::filesystem::path soundFile;
    bool enabled = true;
    std::string description;
    int sourceLine = 0;
};

struct ScheduleResult {
    std::vector<BellEvent> events;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

ScheduleResult loadSchedule(const std::filesystem::path& scheduleFile, const std::filesystem::path& baseDir);
std::vector<BellEvent> eventsForDate(const std::vector<BellEvent>& events, const std::tm& local);

} // namespace rmb
