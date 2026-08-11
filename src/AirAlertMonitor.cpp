#include "AirAlertMonitor.h"

#include "Utils.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace rmb {

namespace {

bool replaceFile(const fs::path& from, const fs::path& to) {
#ifdef _WIN32
    return MoveFileExW(from.wstring().c_str(), to.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    fs::rename(from, to, ec);
    return !ec;
#endif
}

std::string statusName(bool active) {
    return active ? "active" : "clear";
}

void skipJsonWs(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
}

bool hexValue(char c, unsigned int& out) {
    if (c >= '0' && c <= '9') {
        out = static_cast<unsigned int>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        out = static_cast<unsigned int>(10 + c - 'a');
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        out = static_cast<unsigned int>(10 + c - 'A');
        return true;
    }
    return false;
}

bool parseHex4(const std::string& json, size_t pos, unsigned int& out) {
    if (pos + 4 > json.size()) {
        return false;
    }

    out = 0;
    for (size_t i = 0; i < 4; ++i) {
        unsigned int digit = 0;
        if (!hexValue(json[pos + i], digit)) {
            return false;
        }
        out = (out << 4) | digit;
    }
    return true;
}

void appendUtf8(unsigned int codepoint, std::string& out) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

bool parseJsonString(const std::string& json, size_t& pos, std::string& out) {
    skipJsonWs(json, pos);
    if (pos >= json.size() || json[pos] != '"') {
        return false;
    }
    ++pos;
    out.clear();

    while (pos < json.size()) {
        const char c = json[pos++];
        if (c == '"') {
            return true;
        }
        if (c != '\\') {
            out.push_back(c);
            continue;
        }

        if (pos >= json.size()) {
            return false;
        }
        const char escaped = json[pos++];
        switch (escaped) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned int codepoint = 0;
                if (!parseHex4(json, pos, codepoint)) {
                    return false;
                }
                pos += 4;

                if (codepoint >= 0xD800 && codepoint <= 0xDBFF &&
                    pos + 6 <= json.size() && json[pos] == '\\' && json[pos + 1] == 'u') {
                    unsigned int low = 0;
                    if (parseHex4(json, pos + 2, low) && low >= 0xDC00 && low <= 0xDFFF) {
                        codepoint = 0x10000 + (((codepoint - 0xD800) << 10) | (low - 0xDC00));
                        pos += 6;
                    }
                }

                appendUtf8(codepoint, out);
                break;
            }
            default:
                return false;
        }
    }

    return false;
}

bool skipJsonValue(const std::string& json, size_t& pos) {
    skipJsonWs(json, pos);
    if (pos >= json.size()) {
        return false;
    }

    if (json[pos] == '"') {
        std::string ignored;
        return parseJsonString(json, pos, ignored);
    }

    if (json[pos] == '{' || json[pos] == '[') {
        const char open = json[pos];
        const char close = open == '{' ? '}' : ']';
        int depth = 0;
        while (pos < json.size()) {
            const char c = json[pos++];
            if (c == '"') {
                --pos;
                std::string ignored;
                if (!parseJsonString(json, pos, ignored)) {
                    return false;
                }
                continue;
            }
            if (c == open) {
                ++depth;
            } else if (c == close) {
                --depth;
                if (depth == 0) {
                    return true;
                }
            } else if ((c == '{' || c == '[') && open != c) {
                ++depth;
            } else if ((c == '}' || c == ']') && close != c) {
                --depth;
            }
        }
        return false;
    }

    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']') {
        ++pos;
    }
    return true;
}

std::string normalizeJsonName(const std::string& value) {
    std::string out;
    bool pendingSpace = false;
    for (const char c : trim(value)) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !out.empty()) {
            out.push_back(' ');
        }
        pendingSpace = false;
        out.push_back(c);
    }
    return out;
}

bool findObjectProperty(const std::string& json, size_t objectPos, const std::string& property, size_t& valuePos) {
    size_t pos = objectPos;
    skipJsonWs(json, pos);
    if (pos >= json.size() || json[pos] != '{') {
        return false;
    }
    ++pos;

    while (pos < json.size()) {
        skipJsonWs(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            return false;
        }

        std::string key;
        if (!parseJsonString(json, pos, key)) {
            return false;
        }
        skipJsonWs(json, pos);
        if (pos >= json.size() || json[pos] != ':') {
            return false;
        }
        ++pos;
        skipJsonWs(json, pos);

        if (key == property) {
            valuePos = pos;
            return true;
        }

        if (!skipJsonValue(json, pos)) {
            return false;
        }
        skipJsonWs(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
        }
    }

    return false;
}

bool parseJsonBoolAt(const std::string& json, size_t pos, bool& value) {
    skipJsonWs(json, pos);
    if (json.compare(pos, 4, "true") == 0) {
        value = true;
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        value = false;
        return true;
    }
    return false;
}

bool parseUbillingAlertNow(const std::string& json, const std::string& locationName, bool& active) {
    size_t statesPos = 0;
    if (!findObjectProperty(json, 0, "states", statesPos)) {
        return false;
    }

    size_t pos = statesPos;
    skipJsonWs(json, pos);
    if (pos >= json.size() || json[pos] != '{') {
        return false;
    }
    ++pos;

    const std::string target = normalizeJsonName(locationName);
    while (pos < json.size()) {
        skipJsonWs(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            return false;
        }

        std::string key;
        if (!parseJsonString(json, pos, key)) {
            return false;
        }
        skipJsonWs(json, pos);
        if (pos >= json.size() || json[pos] != ':') {
            return false;
        }
        ++pos;
        skipJsonWs(json, pos);

        if (normalizeJsonName(key) == target) {
            size_t alertNowPos = 0;
            if (!findObjectProperty(json, pos, "alertnow", alertNowPos)) {
                return false;
            }
            return parseJsonBoolAt(json, alertNowPos, active);
        }

        if (!skipJsonValue(json, pos)) {
            return false;
        }
        skipJsonWs(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
        }
    }

    return false;
}

const std::string& kyivLocationName() {
    static const std::string name = "м. Київ";
    return name;
}

} // namespace

AirAlertMonitor::AirAlertMonitor(const Settings& settings, Logger& logger)
    : settings_(settings), logger_(logger), player_(settings_, logger_) {}

bool AirAlertMonitor::load() {
    stateLoaded_ = false;
    lastStatus_ = "unknown";
    lastRawCode_ = '?';
    lastUpdatedAt_ = 0;

    if (!fs::exists(settings_.airAlertStateFile)) {
        return true;
    }

    std::ifstream input(settings_.airAlertStateFile);
    if (!input) {
        logger_.warn("cannot read air alert state file: " + settings_.airAlertStateFile.string());
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = toLower(trim(line.substr(0, eq)));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "status") {
            lastStatus_ = toLower(value);
            stateLoaded_ = lastStatus_ == "active" || lastStatus_ == "clear";
        } else if (key == "raw_code" && !value.empty()) {
            lastRawCode_ = value[0];
        } else if (key == "updated_epoch") {
            try {
                lastUpdatedAt_ = static_cast<std::time_t>(std::stoll(value));
            } catch (...) {
                lastUpdatedAt_ = 0;
            }
        }
    }

    return true;
}

bool AirAlertMonitor::enabled() const {
    return settings_.airAlertsEnabled;
}

int AirAlertMonitor::validate(bool checkSounds) const {
    if (!settings_.airAlertsEnabled) {
        return 0;
    }

    int errors = 0;
    if (settings_.airAlertsEndpoint.empty()) {
        std::cerr << "air alerts are enabled, but air_alerts_endpoint is empty\n";
        ++errors;
    }
    if (checkSounds && settings_.requireExistingSound && !fs::exists(settings_.airAlertStartSound)) {
        std::cerr << "missing air alert start sound: " << settings_.airAlertStartSound << '\n';
        ++errors;
    }
    if (checkSounds && settings_.requireExistingSound && !fs::exists(settings_.airAlertEndSound)) {
        std::cerr << "missing air alert end sound: " << settings_.airAlertEndSound << '\n';
        ++errors;
    }

    return errors;
}

void AirAlertMonitor::printStatus() const {
    std::cout << "Air alerts: " << (settings_.airAlertsEnabled ? "enabled" : "disabled") << '\n';
    if (!settings_.airAlertsEnabled) {
        return;
    }
    std::cout << "API: " << settings_.airAlertsEndpoint << '\n';
    std::cout << "Location: " << kyivLocationName() << '\n';
    std::cout << "Last known state: " << lastStatus_;
    if (lastRawCode_ != '?') {
        std::cout << " (" << lastRawCode_ << ")";
    }
    if (lastUpdatedAt_ > 0) {
        std::cout << " updated_epoch=" << lastUpdatedAt_;
    }
    std::cout << '\n';
}

int AirAlertMonitor::checkNow() {
    if (!settings_.airAlertsEnabled) {
        std::cout << "Air alerts are disabled in settings.\n";
        return 0;
    }

    const auto status = fetchStatus();
    handleStatus(status, true);
    return status.ok ? 0 : 2;
}

void AirAlertMonitor::run(std::atomic_bool& stopRequested) {
    if (!settings_.airAlertsEnabled) {
        logger_.info("air alert monitor disabled");
        return;
    }

    logger_.info("air alert monitor started for Kyiv through Ubilling API");
    std::time_t nextPoll = 0;
    while (!stopRequested.load()) {
        const auto now = std::time(nullptr);
        if (now >= nextPoll) {
            handleStatus(fetchStatus(), false);
            nextPoll = std::time(nullptr) + settings_.airAlertsPollSeconds;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    logger_.info("air alert monitor stopped");
}

AirAlertMonitor::RemoteStatus AirAlertMonitor::fetchStatus() const {
    RemoteStatus out;

    HttpRequest request;
    request.url = settings_.airAlertsEndpoint;
    request.timeoutSeconds = settings_.airAlertsHttpTimeoutSeconds;
    request.headers["Accept"] = "application/json";

    const auto response = http_.get(request);
    if (!response.success) {
        out.error = response.error.empty() ? "HTTP request failed" : response.error;
        return out;
    }

    bool active = false;
    if (!parseUbillingAlertNow(response.body, kyivLocationName(), active)) {
        out.error = "cannot find Ubilling alertnow for Kyiv";
        return out;
    }

    out.ok = true;
    out.code = active ? 'A' : 'N';
    out.active = active;
    return out;
}

void AirAlertMonitor::handleStatus(const RemoteStatus& status, bool printOnly) {
    if (!status.ok) {
        if (printOnly) {
            std::cerr << "Air alert status check failed: " << status.error << '\n';
        } else {
            logger_.warn("air alert status check failed: " + status.error);
        }
        return;
    }

    const std::string current = statusName(status.active);
    if (printOnly) {
        std::cout << "Air alert for Kyiv: " << current << " (" << status.code << ")\n";
        return;
    }

    if (!stateLoaded_) {
        if (status.active && settings_.airAlertsNotifyActiveOnStartup) {
            logger_.warn("air alert is already active on startup; notifying now");
            notify("air alert active", settings_.airAlertStartSound);
        }
        saveState(current, status.code);
        return;
    }

    if (lastStatus_ == current) {
        if (lastRawCode_ != status.code) {
            saveState(current, status.code);
        }
        return;
    }

    if (status.active) {
        logger_.warn("air alert started for Kyiv");
        notify("air alert start", settings_.airAlertStartSound);
    } else {
        logger_.info("air alert ended for Kyiv");
        notify("air alert end", settings_.airAlertEndSound);
    }

    saveState(current, status.code);
}

bool AirAlertMonitor::notify(const std::string& eventName, const fs::path& sound) {
    const int attempts = std::max(1, settings_.playbackRetries + 1);
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        const auto result = player_.play(sound);
        if (result.success) {
            logger_.info(eventName + " notification played");
            return true;
        }
        logger_.error(eventName + " notification attempt " + std::to_string(attempt) + " failed: " + result.message);
        if (attempt < attempts && settings_.retryDelaySeconds > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(settings_.retryDelaySeconds));
        }
    }
    return false;
}

bool AirAlertMonitor::saveState(const std::string& status, char rawCode) {
    std::error_code ec;
    fs::create_directories(settings_.airAlertStateFile.parent_path(), ec);
    if (ec) {
        logger_.error("cannot create air alert state directory: " + ec.message());
        return false;
    }

    fs::path temp = settings_.airAlertStateFile;
    temp += ".tmp";
    const auto now = std::time(nullptr);

    {
        std::ofstream output(temp, std::ios::trunc);
        if (!output) {
            logger_.error("cannot write air alert state file: " + temp.string());
            return false;
        }
        output << "status=" << status << '\n';
        output << "raw_code=" << rawCode << '\n';
        output << "updated_epoch=" << now << '\n';
    }

    if (!replaceFile(temp, settings_.airAlertStateFile)) {
        logger_.error("cannot replace air alert state file: " + settings_.airAlertStateFile.string());
        return false;
    }

    stateLoaded_ = true;
    lastStatus_ = status;
    lastRawCode_ = rawCode;
    lastUpdatedAt_ = now;
    return true;
}

} // namespace rmb
