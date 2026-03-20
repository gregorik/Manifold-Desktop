#include "pch.h"
#include "HttpClient.h"
#include "../StringUtils.h"

namespace Manifold::Core {

HttpClient::UrlParts HttpClient::ParseUrl(const std::string& url) {
    UrlParts parts;
    auto wurl = Utf8ToWide(url);

    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostBuf[256]{}, pathBuf[2048]{};
    urlComp.lpszHostName = hostBuf;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = pathBuf;
    urlComp.dwUrlPathLength = 2048;

    if (WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &urlComp)) {
        parts.host = hostBuf;
        parts.path = pathBuf;
        parts.port = urlComp.nPort;
        parts.useHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    }
    return parts;
}

HttpResponse HttpClient::Post(
    const std::string& url,
    const std::string& body,
    const std::map<std::string, std::string>& headers) {

    HttpResponse response;
    auto parts = ParseUrl(url);

    HINTERNET hSession = WinHttpOpen(L"Manifold/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!hSession) return response;

    HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return response; }

    DWORD flags = parts.useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
        parts.path.c_str(), nullptr, nullptr, nullptr, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    // Add headers
    std::wstring headerStr;
    for (auto& [k, v] : headers) {
        headerStr += Utf8ToWide(k) + L": " + Utf8ToWide(v) + L"\r\n";
    }
    headerStr += L"Content-Type: application/json\r\n";

    WinHttpAddRequestHeaders(hRequest, headerStr.c_str(),
        (DWORD)headerStr.size(), WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(hRequest, nullptr, 0,
        (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    // Get status code
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        nullptr, &statusCode, &statusSize, nullptr);
    response.statusCode = (int)statusCode;

    // Read body
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buf(bytesAvailable);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
        response.body.append(buf.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

int HttpClient::StreamPost(
    const std::string& url,
    const std::string& body,
    const std::map<std::string, std::string>& headers,
    std::function<bool(const std::string& chunk)> onData) {

    auto parts = ParseUrl(url);

    HINTERNET hSession = WinHttpOpen(L"Manifold/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!hSession) return 0;

    HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return 0; }

    DWORD flags = parts.useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
        parts.path.c_str(), nullptr, nullptr, nullptr, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 0;
    }

    std::wstring headerStr;
    for (auto& [k, v] : headers) {
        headerStr += Utf8ToWide(k) + L": " + Utf8ToWide(v) + L"\r\n";
    }
    headerStr += L"Content-Type: application/json\r\n";

    WinHttpAddRequestHeaders(hRequest, headerStr.c_str(),
        (DWORD)headerStr.size(), WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(hRequest, nullptr, 0,
        (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 0;
    }

    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        nullptr, &statusCode, &statusSize, nullptr);

    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buf(bytesAvailable);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
        if (bytesRead > 0) {
            std::string chunk(buf.data(), bytesRead);
            if (!onData(chunk)) break;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return (int)statusCode;
}

HttpResponse HttpClient::Get(
    const std::string& url,
    const std::map<std::string, std::string>& headers) {

    HttpResponse response;
    auto parts = ParseUrl(url);

    HINTERNET hSession = WinHttpOpen(L"Manifold/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!hSession) return response;

    HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return response; }

    DWORD flags = parts.useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
        parts.path.c_str(), nullptr, nullptr, nullptr, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    std::wstring headerStr;
    for (auto& [k, v] : headers) {
        headerStr += Utf8ToWide(k) + L": " + Utf8ToWide(v) + L"\r\n";
    }
    if (!headerStr.empty()) {
        WinHttpAddRequestHeaders(hRequest, headerStr.c_str(),
            (DWORD)headerStr.size(), WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, nullptr, 0, nullptr, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    DWORD sc = 0, scSize = sizeof(sc);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        nullptr, &sc, &scSize, nullptr);
    response.statusCode = (int)sc;

    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buf(bytesAvailable);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
        response.body.append(buf.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

} // namespace Manifold::Core
