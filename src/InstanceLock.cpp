#include "InstanceLock.h"

#include "Utils.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace rmb {

namespace {

int readStoredPid(const fs::path& path) {
    std::ifstream input(path);
    int pid = 0;
    input >> pid;
    return pid;
}

bool processAlive(int pid) {
    if (pid <= 0) {
        return false;
    }

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        return false;
    }
    DWORD exitCode = 0;
    const bool ok = GetExitCodeProcess(process, &exitCode) != 0;
    CloseHandle(process);
    return ok && exitCode == STILL_ACTIVE;
#else
    if (kill(pid, 0) == 0) {
        return true;
    }
    return errno == EPERM;
#endif
}

int currentPid() {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

} // namespace

InstanceLock::InstanceLock(fs::path path) : path_(std::move(path)) {}

InstanceLock::~InstanceLock() {
    if (!acquired_) {
        return;
    }

#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
#endif

    std::error_code ec;
    fs::remove(path_, ec);
}

bool InstanceLock::removeIfStale() {
    if (!fs::exists(path_)) {
        return false;
    }

    const int pid = readStoredPid(path_);
    if (processAlive(pid)) {
        return false;
    }

    std::error_code ec;
    fs::remove(path_, ec);
    return !ec;
}

bool InstanceLock::acquire(std::string& error) {
    std::error_code ec;
    fs::create_directories(path_.parent_path(), ec);
    if (ec) {
        error = "cannot create lock directory: " + ec.message();
        return false;
    }

    if (fs::exists(path_)) {
        if (!removeIfStale()) {
            error = "another ringmabell instance appears to be running; lock file: " + path_.string();
            return false;
        }
    }

#ifdef _WIN32
    handle_ = CreateFileW(
        path_.wstring().c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle_ == INVALID_HANDLE_VALUE) {
        error = "cannot acquire lock file: " + std::to_string(GetLastError());
        return false;
    }
#else
    fd_ = open(path_.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd_ < 0) {
        error = "cannot acquire lock file: " + std::string(std::strerror(errno));
        return false;
    }
#endif

    acquired_ = true;
    return writePid(error);
}

bool InstanceLock::writePid(std::string& error) {
    const std::string content = std::to_string(currentPid()) + "\n";

#ifdef _WIN32
    DWORD written = 0;
    if (WriteFile(handle_, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) == 0) {
        error = "cannot write lock file: " + std::to_string(GetLastError());
        return false;
    }
#else
    const ssize_t written = write(fd_, content.data(), content.size());
    if (written < 0 || static_cast<size_t>(written) != content.size()) {
        error = "cannot write lock file: " + std::string(std::strerror(errno));
        return false;
    }
#endif

    return true;
}

} // namespace rmb
