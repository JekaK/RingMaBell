#include "Config.h"
#include "InstanceLock.h"
#include "Logger.h"
#include "Scheduler.h"
#include "Utils.h"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::atomic_bool gStopRequested{false};

void handleSignal(int) {
    gStopRequested.store(true);
}

void printHelp() {
    std::cout
        << "RingMaBell school bell server\n\n"
        << "Usage:\n"
        << "  ringmabell [--config config/settings.conf] run\n"
        << "  ringmabell [--config config/settings.conf] validate\n"
        << "  ringmabell [--config config/settings.conf] list\n"
        << "  ringmabell [--config config/settings.conf] alert-status\n"
        << "  ringmabell [--config config/settings.conf] play-test <mp3-file>\n\n"
        << "Commands:\n"
        << "  run        Start the long-running scheduler server.\n"
        << "  validate   Validate settings, schedule, and sound files.\n"
        << "  list       Print today's bells and their state.\n"
        << "  alert-status  Check Kyiv air alert status through the configured API.\n"
        << "  play-test  Play one sound through the configured backend.\n";
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path configPath = "config/settings.conf";
    std::string command = "run";
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
            continue;
        }
        positional.push_back(arg);
    }

    if (!positional.empty()) {
        command = positional[0];
    }

    auto config = rmb::loadSettings(configPath);
    for (const auto& warning : config.warnings) {
        std::cerr << "warning: " << warning << '\n';
    }
    for (const auto& error : config.errors) {
        std::cerr << "error: " << error << '\n';
    }
    if (!config.errors.empty()) {
        return 2;
    }

    rmb::Logger logger(config.settings.logFile);

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    rmb::Scheduler scheduler(config.settings, logger, gStopRequested);
    if (!scheduler.load()) {
        return 2;
    }

    if (command == "validate") {
        return scheduler.validate(true);
    }

    if (command == "list") {
        scheduler.printToday();
        return 0;
    }

    if (command == "alert-status") {
        return scheduler.checkAirAlertNow();
    }

    if (command == "play-test") {
        if (positional.size() < 2) {
            std::cerr << "play-test requires an mp3 file path\n";
            return 2;
        }
        rmb::AudioPlayer player(config.settings, logger);
        const auto result = player.play(std::filesystem::path(positional[1]));
        if (!result.success) {
            logger.error("play-test failed: " + result.message);
            return 2;
        }
        return 0;
    }

    if (command != "run") {
        std::cerr << "unknown command: " << command << "\n\n";
        printHelp();
        return 2;
    }

    rmb::InstanceLock lock(config.settings.lockFile);
    std::string lockError;
    if (!lock.acquire(lockError)) {
        logger.error(lockError);
        return 3;
    }

    scheduler.run();
    return 0;
}
