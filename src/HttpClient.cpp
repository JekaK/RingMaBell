#include "HttpClient.h"

#include "Utils.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace rmb {

#ifdef _WIN32
namespace {

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

bool parseUrl(const std::string& url, ParsedUrl& out, std::string& error) {
    const std::wstring wide = utf8ToWide(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &parts) == 0) {
        error = "cannot parse URL: " + std::to_string(GetLastError());
        return false;
    }

    out.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    out.path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength > 0) {
        out.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    if (out.path.empty()) {
        out.path = L"/";
    }
    out.port = parts.nPort;
    out.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

std::wstring headersToWide(const std::map<std::string, std::string>& headers) {
    std::string raw;
    for (const auto& header : headers) {
        raw += header.first + ": " + header.second + "\r\n";
    }
    return utf8ToWide(raw);
}

} // namespace
#endif

HttpResponse HttpClient::get(const HttpRequest& request) const {
#ifdef _WIN32
    ParsedUrl url;
    HttpResponse response;
    if (!parseUrl(request.url, url, response.error)) {
        return response;
    }

    HINTERNET session = WinHttpOpen(
        L"RingMaBell/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session) {
        response.error = "WinHttpOpen failed: " + std::to_string(GetLastError());
        return response;
    }

    const int timeoutMs = request.timeoutSeconds * 1000;
    WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET connect = WinHttpConnect(session, url.host.c_str(), url.port, 0);
    if (!connect) {
        response.error = "WinHttpConnect failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(session);
        return response;
    }

    HINTERNET httpRequest = WinHttpOpenRequest(
        connect,
        L"GET",
        url.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        url.secure ? WINHTTP_FLAG_SECURE : 0);
    if (!httpRequest) {
        response.error = "WinHttpOpenRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return response;
    }

    const std::wstring headerText = headersToWide(request.headers);
    BOOL ok = WinHttpSendRequest(
        httpRequest,
        headerText.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerText.c_str(),
        headerText.empty() ? 0 : static_cast<DWORD>(headerText.size()),
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0);
    if (ok) {
        ok = WinHttpReceiveResponse(httpRequest, nullptr);
    }

    if (!ok) {
        response.error = "WinHTTP request failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(httpRequest);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return response;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(
        httpRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX);
    response.statusCode = static_cast<int>(status);

    std::string body;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(httpRequest, &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(httpRequest, chunk.data(), available, &read)) {
            response.error = "WinHttpReadData failed: " + std::to_string(GetLastError());
            break;
        }
        chunk.resize(read);
        body += chunk;
    }

    WinHttpCloseHandle(httpRequest);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    response.body = body;
    response.success = response.error.empty() && response.statusCode >= 200 && response.statusCode < 300;
    if (!response.success && response.error.empty()) {
        response.error = "HTTP " + std::to_string(response.statusCode);
    }
    return response;
#else
    HttpResponse response;
    fs::path temp = fs::temp_directory_path();
    temp /= "ringmabell-http-" + std::to_string(static_cast<long long>(::getpid())) + "-" + std::to_string(std::time(nullptr)) + ".tmp";

    std::ostringstream command;
    command << "curl -fsSL --max-time " << request.timeoutSeconds;
    for (const auto& header : request.headers) {
        command << " -H " << shellQuote(header.first + ": " + header.second);
    }
    command << " " << shellQuote(request.url) << " -o " << shellQuote(temp.string());

    const int code = std::system(command.str().c_str());
    if (code != 0) {
        response.error = "curl failed with code " + std::to_string(code);
        std::error_code ec;
        fs::remove(temp, ec);
        return response;
    }

    std::ifstream input(temp, std::ios::binary);
    if (!input) {
        response.error = "cannot read HTTP response file";
        std::error_code ec;
        fs::remove(temp, ec);
        return response;
    }

    std::ostringstream body;
    body << input.rdbuf();
    response.statusCode = 200;
    response.body = body.str();
    response.success = true;

    std::error_code ec;
    fs::remove(temp, ec);
    return response;
#endif
}

} // namespace rmb
