#pragma once
#include <string>
#include <vector>
namespace kindle {
struct WebLink { std::string label, url; };
struct WebPage { std::string url, text; std::vector<WebLink> links; };
bool https_url(const std::string& url);
std::string resolve_url(const std::string& base, const std::string& reference);
WebPage parse_web_page(const std::string& html, const std::string& url);
#ifdef __linux__
int fetch_web_worker(const std::string& url, const std::string& prefix);
class WebRequest {
    int child_ = -1, descriptor_ = -1;
    std::string prefix_;
public:
    ~WebRequest();
    void cancel();
    void start(const std::string& url, const std::string& prefix);
    int descriptor() const { return descriptor_; }
    bool finish(WebPage& result, std::string& error);
};
#endif
}
