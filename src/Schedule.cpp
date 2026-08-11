#include "Schedule.h"

#include "Csv.h"
#include "Utils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace rmb {

namespace {

bool parseDateToken(const std::string& token, std::string& normalized) {
    if (token.size() != 10 || token[4] != '-' || token[7] != '-') {
        return false;
    }
    for (size_t i = 0; i < token.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) {
            return false;
        }
    }
    normalized = token;
    return true;
}

void addWeekdayRange(DayRule& rule, int start, int end) {
    if (start <= end) {
        for (int day = start; day <= end; ++day) {
            rule.weekdays.insert(day);
        }
    } else {
        for (int day = start; day <= 6; ++day) {
            rule.weekdays.insert(day);
        }
        for (int day = 0; day <= end; ++day) {
            rule.weekdays.insert(day);
        }
    }
}

bool parseDayRule(const std::string& value, DayRule& rule, std::string& error) {
    const auto tokens = splitAny(value, " ,;|");
    if (tokens.empty()) {
        error = "days field is empty";
        return false;
    }

    const std::map<std::string, int> weekdays = {
        {"sun", 0}, {"sunday", 0}, {"nd", 0}, {"7", 0}, {"0", 0},
        {"mon", 1}, {"monday", 1}, {"pn", 1}, {"1", 1},
        {"tue", 2}, {"tuesday", 2}, {"vt", 2}, {"2", 2},
        {"wed", 3}, {"wednesday", 3}, {"sr", 3}, {"3", 3},
        {"thu", 4}, {"thursday", 4}, {"cht", 4}, {"4", 4},
        {"fri", 5}, {"friday", 5}, {"pt", 5}, {"5", 5},
        {"sat", 6}, {"saturday", 6}, {"sb", 6}, {"6", 6},
    };

    for (std::string token : tokens) {
        token = toLower(trim(token));
        if (token.empty()) {
            continue;
        }

        if (token == "*" || token == "all" || token == "daily" || token == "everyday") {
            rule.all = true;
            continue;
        }

        if (token == "weekdays" || token == "weekday" || token == "school" || token == "mon-fri" || token == "1-5") {
            addWeekdayRange(rule, 1, 5);
            continue;
        }

        if (token == "weekend" || token == "sat-sun" || token == "6-7" || token == "6-0") {
            rule.weekdays.insert(6);
            rule.weekdays.insert(0);
            continue;
        }

        bool excluded = false;
        if (token[0] == '!') {
            excluded = true;
            token = token.substr(1);
        }

        std::string date;
        if (parseDateToken(token, date)) {
            if (excluded) {
                rule.excludeDates.insert(date);
            } else {
                rule.includeDates.insert(date);
            }
            continue;
        }

        const auto weekday = weekdays.find(token);
        if (weekday != weekdays.end()) {
            if (excluded) {
                error = "weekday exclusions are not supported, use !YYYY-MM-DD instead: " + token;
                return false;
            }
            rule.weekdays.insert(weekday->second);
            continue;
        }

        error = "unknown day token: " + token;
        return false;
    }

    return rule.all || !rule.weekdays.empty() || !rule.includeDates.empty() || !rule.excludeDates.empty();
}

bool parseTimeField(const std::string& value, int& seconds) {
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

std::map<std::string, size_t> headerMap(const std::vector<std::string>& header) {
    std::map<std::string, size_t> result;
    for (size_t i = 0; i < header.size(); ++i) {
        result[toLower(trim(header[i]))] = i;
    }
    return result;
}

std::string cellAt(const std::vector<std::string>& row, const std::map<std::string, size_t>& header, const std::string& name) {
    const auto it = header.find(name);
    if (it == header.end() || it->second >= row.size()) {
        return {};
    }
    return trim(row[it->second]);
}

} // namespace

bool DayRule::matches(const std::tm& local) const {
    const std::string date = dateKey(local);
    if (excludeDates.count(date) > 0) {
        return false;
    }
    if (includeDates.count(date) > 0) {
        return true;
    }
    if (all) {
        return true;
    }
    return weekdays.count(local.tm_wday) > 0;
}

ScheduleResult loadSchedule(const fs::path& scheduleFile, const fs::path& baseDir) {
    ScheduleResult result;

    std::ifstream input(scheduleFile);
    if (!input) {
        result.errors.push_back("cannot open schedule file: " + scheduleFile.string());
        return result;
    }

    std::string line;
    int lineNumber = 0;
    std::vector<std::string> header;
    std::map<std::string, size_t> columns;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty() || trim(line)[0] == '#') {
            continue;
        }

        bool ok = true;
        std::string csvError;
        auto row = parseCsvLine(line, ok, csvError);
        if (!ok) {
            result.errors.push_back("schedule line " + std::to_string(lineNumber) + ": " + csvError);
            continue;
        }

        if (header.empty()) {
            header = row;
            columns = headerMap(header);
            for (const std::string& required : {"id", "days", "time", "sound"}) {
                if (columns.find(required) == columns.end()) {
                    result.errors.push_back("schedule header must include column: " + required);
                }
            }
            continue;
        }

        BellEvent event;
        event.sourceLine = lineNumber;
        event.id = cellAt(row, columns, "id");
        event.description = cellAt(row, columns, "description");

        if (event.id.empty()) {
            result.errors.push_back("schedule line " + std::to_string(lineNumber) + ": id is required");
        }

        const std::string dayText = cellAt(row, columns, "days");
        std::string dayError;
        if (!parseDayRule(dayText, event.days, dayError)) {
            result.errors.push_back("schedule line " + std::to_string(lineNumber) + ": " + dayError);
        }

        const std::string timeText = cellAt(row, columns, "time");
        if (!parseTimeField(timeText, event.secondsAfterMidnight)) {
            result.errors.push_back("schedule line " + std::to_string(lineNumber) + ": invalid time: " + timeText);
        }

        const std::string soundText = cellAt(row, columns, "sound");
        if (soundText.empty()) {
            result.errors.push_back("schedule line " + std::to_string(lineNumber) + ": sound is required");
        } else {
            fs::path sound = soundText;
            if (!sound.is_absolute()) {
                sound = (baseDir / sound).lexically_normal();
            }
            event.soundFile = sound;
        }

        const std::string enabledText = cellAt(row, columns, "enabled");
        if (!enabledText.empty() && !parseBool(enabledText, event.enabled)) {
            result.errors.push_back("schedule line " + std::to_string(lineNumber) + ": invalid enabled value: " + enabledText);
        }

        result.events.push_back(event);
    }

    if (header.empty()) {
        result.errors.push_back("schedule file is empty: " + scheduleFile.string());
    }

    std::sort(result.events.begin(), result.events.end(), [](const BellEvent& a, const BellEvent& b) {
        if (a.secondsAfterMidnight != b.secondsAfterMidnight) {
            return a.secondsAfterMidnight < b.secondsAfterMidnight;
        }
        return a.id < b.id;
    });

    return result;
}

std::vector<BellEvent> eventsForDate(const std::vector<BellEvent>& events, const std::tm& local) {
    std::vector<BellEvent> out;
    for (const auto& event : events) {
        if (event.enabled && event.days.matches(local)) {
            out.push_back(event);
        }
    }

    std::sort(out.begin(), out.end(), [](const BellEvent& a, const BellEvent& b) {
        if (a.secondsAfterMidnight != b.secondsAfterMidnight) {
            return a.secondsAfterMidnight < b.secondsAfterMidnight;
        }
        return a.id < b.id;
    });
    return out;
}

} // namespace rmb
