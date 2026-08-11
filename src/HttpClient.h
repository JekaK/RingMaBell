#pragma once

#include <map>
#include <string>

namespace rmb {

struct HttpResponse {
    bool success = false;
    int statusCode = 0;
    std::string body;
    std::string error;
};

struct HttpRequest {
    std::string url;
    std::map<std::string, std::string> headers;
    int timeoutSeconds = 8;
};

class HttpClient {
public:
    HttpResponse get(const HttpRequest& request) const;
};

} // namespace rmb
