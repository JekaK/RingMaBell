#include "StateStore.h"

#include "Csv.h"
#include "Utils.h"

#include <filesystem>
#include <fstream>

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

} // namespace

StateStore::StateStore(fs::path path) : path_(std::move(path)) {}

bool StateStore::load() {
    records_.clear();
    if (!fs::exists(path_)) {
        return true;
    }

    std::ifstream input(path_);
    if (!input) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (trim(line).empty() || trim(line)[0] == '#') {
            continue;
        }

        bool ok = true;
        std::string error;
        auto cells = parseCsvLine(line, ok, error);
        if (!ok || cells.size() < 5 || toLower(trim(cells[0])) == "date") {
            continue;
        }

        int seconds = 0;
        if (!parseSeconds(trim(cells[2]), seconds)) {
            continue;
        }

        StateRecord record;
        record.status = trim(cells[3]);
        try {
            record.updatedAt = static_cast<std::time_t>(std::stoll(trim(cells[4])));
        } catch (...) {
            record.updatedAt = 0;
        }
        records_[key(trim(cells[0]), trim(cells[1]), seconds)] = record;
    }

    return true;
}

bool StateStore::has(const std::string& date, const std::string& id, int seconds) const {
    return records_.count(key(date, id, seconds)) > 0;
}

std::string StateStore::status(const std::string& date, const std::string& id, int seconds) const {
    const auto it = records_.find(key(date, id, seconds));
    if (it == records_.end()) {
        return {};
    }
    return it->second.status;
}

bool StateStore::mark(const std::string& date, const std::string& id, int seconds, const std::string& status) {
    records_[key(date, id, seconds)] = {status, std::time(nullptr)};
    return save();
}

bool StateStore::save() const {
    std::error_code ec;
    fs::create_directories(path_.parent_path(), ec);
    if (ec) {
        return false;
    }

    fs::path tmp = path_;
    tmp += ".tmp";
    {
        std::ofstream output(tmp, std::ios::trunc);
        if (!output) {
            return false;
        }

        output << "date,id,time,status,updated_epoch\n";
        for (const auto& item : records_) {
            const auto parts = splitAny(item.first, "|");
            if (parts.size() != 3) {
                continue;
            }
            output << csvEscape(parts[0]) << ','
                   << csvEscape(parts[1]) << ','
                   << csvEscape(parts[2]) << ','
                   << csvEscape(item.second.status) << ','
                   << item.second.updatedAt << '\n';
        }
    }

    return replaceFile(tmp, path_);
}

std::string StateStore::key(const std::string& date, const std::string& id, int seconds) {
    return date + "|" + id + "|" + formatSeconds(seconds);
}

} // namespace rmb
