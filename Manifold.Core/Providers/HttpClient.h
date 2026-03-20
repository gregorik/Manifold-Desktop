#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace Manifold::Core {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

class HttpClient {
public:
    // Simple POST with JSON body, returns full response
    static HttpResponse Post(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers = {});

    // Streaming POST — calls onData for each chunk, returns status code
    static int StreamPost(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers,
        std::function<bool(const std::string& chunk)> onData);

    // Simple GET
    static HttpResponse Get(
        const std::string& url,
        const std::map<std::string, std::string>& headers = {});

private:
    struct UrlParts {
        std::wstring host;
        std::wstring path;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        bool useHttps = true;
    };
    static UrlParts ParseUrl(const std::string& url);
};

} // namespace Manifold::Core
