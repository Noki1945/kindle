#include <windows.h>
#include <winhttp.h>
#include "kindle/browser.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace kindle {
struct Internet {
    HINTERNET h;
    explicit Internet(HINTERNET value) : h(value) { if (!h) throw std::runtime_error("HTTPS operation failed"); }
    ~Internet() { WinHttpCloseHandle(h); }
    Internet(const Internet&) = delete;
};
std::string html_text(const std::string& html) {
    std::string result, tag; bool inside = false; std::string skip;
    for (char c : html) {
        if (c == '<') { inside = true; tag.clear(); continue; }
        if (inside) {
            if (c == '>') {
                inside = false;
                std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char v) { return static_cast<char>(tolower(v)); });
                if (tag == "/script" || tag == "/style") skip.clear();
                else if (tag == "script" || tag.rfind("script ", 0) == 0) skip = "script";
                else if (tag == "style" || tag.rfind("style ", 0) == 0) skip = "style";
                else if (skip.empty() && (tag == "br" || tag == "br/" || tag == "p" || tag == "/p" || tag == "/div" || tag == "/h1" || tag == "/h2" || tag == "/title" || tag == "/li")) result += "\r\n";
            } else if (tag.size() < 512) tag += c;
        } else if (skip.empty()) result += c;
    }
    const std::pair<const char*, const char*> entities[] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&nbsp;", " "}, {"&#39;", "'"}};
    for (auto e : entities) { std::size_t p = 0; while ((p = result.find(e.first, p)) != std::string::npos) { result.replace(p, strlen(e.first), e.second); p += strlen(e.second); } }
    return result;
}
std::string fetch_article(const std::wstring& url) {
    if (url.size() > 4096) throw std::runtime_error("URL too long");
    URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        throw std::runtime_error("Use an https:// URL");
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    Internet session(WinHttpOpen(L"KindleLitePrototype/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0));
    WinHttpSetTimeouts(session.h, 5000, 5000, 5000, 5000);
    Internet connection(WinHttpConnect(session.h, host.c_str(), parts.nPort, 0));
    Internet request(WinHttpOpenRequest(connection.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    if (!WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request.h, nullptr))
        throw std::runtime_error("HTTPS request failed (network, TLS or timeout)");
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr) || status != 200)
        throw std::runtime_error("Server returned HTTP " + std::to_string(status));
    wchar_t type[256]{}; size = sizeof(type);
    if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_TYPE, nullptr, type, &size, nullptr)) throw std::runtime_error("Missing content type");
    std::wstring mime(type);
    if (mime.find(L"text/html") == std::wstring::npos && mime.find(L"text/plain") == std::wstring::npos) throw std::runtime_error("Article mode supports text/html and text/plain only");
    std::string body; char buffer[8192]; DWORD count = 0;
    const ULONGLONG deadline = GetTickCount64() + 20000;
    do {
        if (GetTickCount64() > deadline) throw std::runtime_error("Article time limit exceeded");
        if (!WinHttpReadData(request.h, buffer, sizeof(buffer), &count)) throw std::runtime_error("Read failed");
        if (body.size() + count > 2 * 1024 * 1024) throw std::runtime_error("Article exceeds 2 MiB limit");
        body.append(buffer, count);
    } while (count);
    return mime.find(L"text/html") != std::wstring::npos ? html_text(body) : body;
}
}
