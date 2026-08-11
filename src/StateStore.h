#pragma once

#include <ctime>
#include <filesystem>
#include <map>
#include <string>

namespace rmb {

struct StateRecord {
    std::string status;
    std::time_t updatedAt = 0;
};

class StateStore {
public:
    explicit StateStore(std::filesystem::path path);

    bool load();
    bool has(const std::string& date, const std::string& id, int seconds) const;
    std::string status(const std::string& date, const std::string& id, int seconds) const;
    bool mark(const std::string& date, const std::string& id, int seconds, const std::string& status);

private:
    bool save() const;
    static std::string key(const std::string& date, const std::string& id, int seconds);

    std::filesystem::path path_;
    std::map<std::string, StateRecord> records_;
};

} // namespace rmb
