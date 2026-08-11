#pragma once

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rmb {

class InstanceLock {
public:
    explicit InstanceLock(std::filesystem::path path);
    ~InstanceLock();

    InstanceLock(const InstanceLock&) = delete;
    InstanceLock& operator=(const InstanceLock&) = delete;

    bool acquire(std::string& error);
    bool acquired() const { return acquired_; }

private:
    bool removeIfStale();
    bool writePid(std::string& error);

    std::filesystem::path path_;
    bool acquired_ = false;

#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
};

} // namespace rmb
