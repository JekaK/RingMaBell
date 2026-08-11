#include "AudioPlayer.h"

#include "Utils.h"

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace fs = std::filesystem;

namespace rmb {

namespace {

std::mutex gAudioMutex;

} // namespace

AudioPlayer::AudioPlayer(const Settings& settings, Logger& logger)
    : settings_(settings), logger_(logger) {}

PlayResult AudioPlayer::play(const fs::path& file) const {
    std::lock_guard<std::mutex> lock(gAudioMutex);

    if (settings_.requireExistingSound && !fs::exists(file)) {
        return {false, -1, "sound file does not exist: " + file.string()};
    }

    const auto backend = toLower(settings_.audioBackend);
    if (backend == "auto") {
#ifdef _WIN32
        return playWithMci(file);
#elif defined(__APPLE__)
        return playWithBackend("afplay", file);
#else
        if (commandExists("mpg123")) {
            return playWithBackend("mpg123", file);
        }
        if (commandExists("ffplay")) {
            return playWithBackend("ffplay", file);
        }
        if (commandExists("cvlc")) {
            return playWithBackend("cvlc", file);
        }
        return {false, -1, "no supported audio player found; install mpg123/ffplay/cvlc or set audio_command"};
#endif
    }

    return playWithBackend(backend, file);
}

PlayResult AudioPlayer::playWithBackend(const std::string& backend, const fs::path& file) const {
    if (backend == "dry-run" || backend == "dryrun") {
        logger_.info("dry-run audio backend: would play " + file.string());
        return {true, 0, "dry-run"};
    }

    if (backend == "command") {
        return playWithCommand(file);
    }

#ifdef _WIN32
    if (backend == "mci") {
        return playWithMci(file);
    }
#else
    if (backend == "mci") {
        return {false, -1, "mci backend is available only on Windows"};
    }
#endif

    if (backend == "afplay") {
        return runCommand("/usr/bin/afplay " + shellQuote(file.string()));
    }

    if (backend == "ffplay") {
        return runCommand("ffplay -nodisp -autoexit -loglevel quiet " + shellQuote(file.string()));
    }

    if (backend == "mpg123") {
        return runCommand("mpg123 -q " + shellQuote(file.string()));
    }

    if (backend == "cvlc") {
        return runCommand("cvlc --play-and-exit --intf dummy " + shellQuote(file.string()));
    }

    return {false, -1, "unknown audio backend: " + backend};
}

PlayResult AudioPlayer::playWithCommand(const fs::path& file) const {
    if (settings_.audioCommand.empty()) {
        return {false, -1, "audio_backend=command requires audio_command in settings"};
    }

    std::string command = settings_.audioCommand;
    const auto quoted = shellQuote(file.string());
    const auto pos = command.find("{file}");
    if (pos == std::string::npos) {
        command += " " + quoted;
    } else {
        command.replace(pos, 6, quoted);
    }

    return runCommand(command);
}

PlayResult AudioPlayer::runCommand(const std::string& command) const {
    logger_.info("starting audio command: " + command);
    const int code = std::system(command.c_str());
    if (code == 0) {
        return {true, 0, "command completed"};
    }

    std::ostringstream message;
    message << "audio command failed with code " << code;
    return {false, code, message.str()};
}

#ifdef _WIN32
PlayResult AudioPlayer::playWithMci(const fs::path& file) const {
    const std::wstring alias = L"ringmabell_audio";
    const std::wstring path = file.wstring();

    mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);

    const std::wstring openCommand = L"open \"" + path + L"\" type mpegvideo alias " + alias;
    MCIERROR error = mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr);
    if (error != 0) {
        wchar_t details[256] = {};
        mciGetErrorStringW(error, details, 256);
        return {false, static_cast<int>(error), wideToUtf8(details)};
    }

    const std::wstring playCommand = L"play " + alias + L" wait";
    error = mciSendStringW(playCommand.c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);

    if (error != 0) {
        wchar_t details[256] = {};
        mciGetErrorStringW(error, details, 256);
        return {false, static_cast<int>(error), wideToUtf8(details)};
    }

    return {true, 0, "mci playback completed"};
}
#endif

} // namespace rmb
