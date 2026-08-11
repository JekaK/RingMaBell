#include "Scheduler.h"

#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;

namespace rmb {

Scheduler::Scheduler(Settings settings, Logger& logger, std::atomic_bool& stopRequested)
    : settings_(std::move(settings)),
      logger_(logger),
      stopRequested_(stopRequested),
      player_(settings_, logger_),
      state_(settings_.stateFile),
      airAlerts_(settings_, logger_) {}

bool Scheduler::load() {
    if (!state_.load()) {
        logger_.warn("state file could not be loaded; continuing with empty state");
    }
    airAlerts_.load();

    auto schedule = loadSchedule(settings_.scheduleFile, settings_.baseDir);
    for (const auto& warning : schedule.warnings) {
        logger_.warn(warning);
    }
    for (const auto& error : schedule.errors) {
        logger_.error(error);
    }
    if (!schedule.errors.empty()) {
        return false;
    }

    events_ = std::move(schedule.events);

    std::error_code ec;
    scheduleMtime_ = fs::last_write_time(settings_.scheduleFile, ec);
    if (ec) {
        scheduleMtime_ = {};
    }

    logger_.info("loaded " + std::to_string(events_.size()) + " schedule event(s)");
    return true;
}

int Scheduler::validate(bool checkSounds) const {
    int errors = 0;
    for (const auto& event : events_) {
        if (event.enabled && checkSounds && settings_.requireExistingSound && !fs::exists(event.soundFile)) {
            std::cerr << "missing sound for " << event.id << " at "
                      << formatSeconds(event.secondsAfterMidnight) << ": "
                      << event.soundFile << '\n';
            ++errors;
        }
    }

    errors += airAlerts_.validate(checkSounds);

    if (errors == 0) {
        std::cout << "configuration is valid; events loaded: " << events_.size() << '\n';
    }
    return errors == 0 ? 0 : 2;
}

void Scheduler::printToday() const {
    const auto now = std::time(nullptr);
    const auto local = localTime(now);
    const std::string date = dateKey(local);
    const auto today = eventsForDate(events_, local);

    std::cout << "Schedule for " << date << '\n';
    for (const auto& event : today) {
        const std::string status = state_.has(date, event.id, event.secondsAfterMidnight)
            ? state_.status(date, event.id, event.secondsAfterMidnight)
            : "pending";
        std::cout << formatSeconds(event.secondsAfterMidnight) << "  "
                  << event.id << "  " << status << "  "
                  << event.soundFile.string();
        if (!event.description.empty()) {
            std::cout << "  " << event.description;
        }
        std::cout << '\n';
    }

    std::cout << '\n';
    airAlerts_.printStatus();
}

int Scheduler::checkAirAlertNow() {
    return airAlerts_.checkNow();
}

void Scheduler::run() {
    logger_.info("ringmabell server started");
    std::thread airAlertThread;
    if (airAlerts_.enabled()) {
        airAlertThread = std::thread([this]() {
            airAlerts_.run(stopRequested_);
        });
    }

    while (!stopRequested_.load()) {
        reloadIfChanged();
        tick(std::time(nullptr));
        std::this_thread::sleep_for(std::chrono::milliseconds(settings_.pollIntervalMs));
    }

    if (airAlertThread.joinable()) {
        airAlertThread.join();
    }
    logger_.info("ringmabell server stopped");
}

bool Scheduler::reloadIfChanged() {
    std::error_code ec;
    const auto mtime = fs::last_write_time(settings_.scheduleFile, ec);
    if (ec || mtime == scheduleMtime_) {
        return false;
    }

    logger_.info("schedule file changed; reloading");
    auto schedule = loadSchedule(settings_.scheduleFile, settings_.baseDir);
    for (const auto& warning : schedule.warnings) {
        logger_.warn(warning);
    }
    for (const auto& error : schedule.errors) {
        logger_.error(error);
    }
    if (!schedule.errors.empty()) {
        logger_.error("keeping previous schedule because reload failed");
        return false;
    }

    events_ = std::move(schedule.events);
    scheduleMtime_ = mtime;
    logger_.info("reloaded " + std::to_string(events_.size()) + " schedule event(s)");
    return true;
}

void Scheduler::tick(std::time_t now) {
    const auto local = localTime(now);
    if (!settings_.bellWeekendsEnabled && isWeekend(local)) {
        return;
    }

    const std::string date = dateKey(local);
    const int nowSeconds = secondsOfDay(local);
    const auto today = eventsForDate(events_, local);

    for (const auto& event : today) {
        if (state_.has(date, event.id, event.secondsAfterMidnight)) {
            continue;
        }

        const int delta = nowSeconds - event.secondsAfterMidnight;
        if (delta < 0) {
            continue;
        }

        if (delta <= settings_.missedGraceSeconds) {
            if (settings_.catchUpMissedBells || delta <= 1) {
                const bool ok = playEvent(event, date);
                state_.mark(date, event.id, event.secondsAfterMidnight, ok ? "played" : "failed");
            }
            continue;
        }

        if (settings_.markExpiredAsSkipped) {
            logger_.warn("skipping expired bell " + event.id + " scheduled at " + formatSeconds(event.secondsAfterMidnight));
            state_.mark(date, event.id, event.secondsAfterMidnight, "skipped");
        }
    }
}

bool Scheduler::playEvent(const BellEvent& event, const std::string& date) {
    logger_.info("playing bell " + event.id + " for " + date + " at " + formatSeconds(event.secondsAfterMidnight));

    const int attempts = std::max(1, settings_.playbackRetries + 1);
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        const auto result = player_.play(event.soundFile);
        if (result.success) {
            logger_.info("bell " + event.id + " played successfully");
            return true;
        }

        logger_.error("bell " + event.id + " playback attempt " + std::to_string(attempt) + " failed: " + result.message);
        if (attempt < attempts && settings_.retryDelaySeconds > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(settings_.retryDelaySeconds));
        }
    }

    return false;
}

} // namespace rmb
