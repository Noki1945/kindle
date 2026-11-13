#include "kindle/web.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>

namespace kindle {
namespace {
std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string trim(std::string s) {
    auto white = [](unsigned char c) { return std::isspace(c); };
    while (!s.empty() && white(s.back())) s.pop_back();
    auto pos = std::find_if_not(s.begin(), s.end(), white); s.erase(s.begin(), pos); return s;
}
void utf8(std::string& out, unsigned cp) {
    if (!cp || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return;
    if (cp < 128) out += static_cast<char>(cp);
    else if (cp < 2048) { out += static_cast<char>(0xc0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 63)); }
    else if (cp < 65536) { out += static_cast<char>(0xe0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
    else { out += static_cast<char>(0xf0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 63)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
}
std::string decode(const std::string& input) {
    static const std::map<std::string, std::string> named = {{"amp","&"},{"lt","<"},{"gt",">"},{"quot","\""},{"apos","'"},{"nbsp"," "},{"copy","©"},{"mdash","—"},{"ndash","–"},{"hellip","…"}};
    std::string out;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '&') {
            auto end = input.find(';', i + 1);
            if (end != std::string::npos && end - i <= 16) {
                auto entity = input.substr(i + 1, end - i - 1);
                auto known = named.find(entity);
                if (known != named.end()) { out += known->second; i = end; continue; }
                if (entity.size() > 1 && entity[0] == '#') {
                    unsigned cp = 0; std::size_t k = 1; unsigned base = 10;
                    if (k < entity.size() && (entity[k] == 'x' || entity[k] == 'X')) { base = 16; ++k; }
                    bool valid = k < entity.size();
                    for (; k < entity.size(); ++k) {
                        char c = entity[k]; unsigned digit = c >= '0' && c <= '9' ? c-'0' : (c >= 'a' && c <= 'f' ? c-'a'+10 : (c >= 'A' && c <= 'F' ? c-'A'+10 : 99));
                        if (digit >= base || cp > 0x10ffff / base) { valid = false; break; }
                        cp = cp * base + digit;
                    }
                    if (valid && cp && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff)) { utf8(out, cp); i = end; continue; }
                }
            }
        }
        out += input[i];
    }
    return out;
}
std::map<std::string, std::string> attributes(const std::string& tag, std::size_t i) {
    std::map<std::string, std::string> values;
    while (i < tag.size()) {
        while (i < tag.size() && (std::isspace(static_cast<unsigned char>(tag[i])) || tag[i] == '/')) ++i;
        auto start = i;
        while (i < tag.size() && !std::isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '=' && tag[i] != '/') ++i;
        if (start == i) { ++i; continue; }
        auto name = lower(tag.substr(start, i - start));
        while (i < tag.size() && std::isspace(static_cast<unsigned char>(tag[i]))) ++i;
        if (i == tag.size() || tag[i] != '=') continue;
        ++i; while (i < tag.size() && std::isspace(static_cast<unsigned char>(tag[i]))) ++i;
        char quote = i < tag.size() && (tag[i] == '\'' || tag[i] == '"') ? tag[i++] : 0;
        start = i;
        while (i < tag.size() && (quote ? tag[i] != quote : !std::isspace(static_cast<unsigned char>(tag[i])))) ++i;
        values[name] = decode(tag.substr(start, i - start));
        if (quote && i < tag.size()) ++i;
    }
    return values;
}
}
bool https_url(const std::string& url) {
    if (url.rfind("https://", 0) || url.size() > 4096) return false;
    auto end = url.find_first_of("/?#", 8);
    auto authority = url.substr(8, end == std::string::npos ? end : end - 8);
    if (authority.empty() || authority.find('@') != std::string::npos || authority.find('\\') != std::string::npos) return false;
    for (unsigned char c : url) if (c <= 32 || c == 127 || c == '\\') return false;
    return true;
}
std::string resolve_url(const std::string& base, const std::string& reference) {
    auto ref = trim(reference);
    if (!https_url(base) || ref.empty()) return {};
    if (ref.rfind("//", 0) == 0) return https_url("https:" + ref) ? "https:" + ref : "";
    if (ref.find(':') < ref.find_first_of("/?#")) return https_url(ref) ? ref : "";
    auto authority_end = base.find_first_of("/?#", 8);
    auto origin = base.substr(0, authority_end);
    std::string result;
    if (ref[0] == '#') { result = base.substr(0, base.find('#')) + ref; }
    else if (ref[0] == '?') { result = base.substr(0, base.find_first_of("?#")) + ref; }
    else {
        auto path = base.substr(origin.size()); path.resize(path.find_first_of("?#") == std::string::npos ? path.size() : path.find_first_of("?#"));
        if (ref[0] == '/') path = ref;
        else path = (path.empty() ? "/" : path.substr(0, path.rfind('/') + 1)) + ref;
        auto suffix = path.find_first_of("?#");
        auto clean = std::filesystem::path(path.substr(0, suffix)).lexically_normal().generic_string();
        result = origin + clean + (suffix == std::string::npos ? "" : path.substr(suffix));
    }
    return https_url(result) ? result : "";
}
WebPage parse_web_page(const std::string& html, const std::string& url) {
    WebPage page; page.url = url;
    std::string base = url, skip, anchor;
    std::size_t anchor_start = 0;
    auto newline = [&] { if (!page.text.empty() && page.text.back() != '\n') page.text += '\n'; };
    auto append = [&](const std::string& value) {
        for (unsigned char c : decode(value)) {
            if (std::isspace(c)) { if (!page.text.empty() && page.text.back() != ' ' && page.text.back() != '\n') page.text += ' '; }
            else if (c >= 32) page.text += static_cast<char>(c);
        }
    };
    for (std::size_t i = 0; i < html.size();) {
        if (html[i] != '<') { auto end = html.find('<', i); if (skip.empty()) append(html.substr(i, end == std::string::npos ? end : end - i)); i = end == std::string::npos ? html.size() : end; continue; }
        if (html.compare(i, 4, "<!--") == 0) { auto end = html.find("-->", i + 4); i = end == std::string::npos ? html.size() : end + 3; continue; }
        std::size_t end = i + 1; char quote = 0;
        for (; end < html.size(); ++end) { char c = html[end]; if (quote) { if (c == quote) quote = 0; } else if (c == '\'' || c == '"') quote = c; else if (c == '>') break; }
        if (end == html.size()) break;
        auto tag = trim(html.substr(i + 1, end - i - 1)); i = end + 1;
        bool closing = !tag.empty() && tag[0] == '/';
        auto name_start = closing ? 1u : 0u;
        auto name_end = tag.find_first_of(" \t\r\n/", name_start);
        if (name_end == std::string::npos) name_end = tag.size();
        auto name = lower(tag.substr(name_start, name_end - name_start));
        if (!skip.empty()) { if (closing && name == skip) skip.clear(); continue; }
        if (!closing && (name == "script" || name == "style" || name == "head" || name == "svg")) { skip = name; continue; }
        if (!closing && name == "base") { auto attrs = attributes(tag, name_end); auto resolved = resolve_url(base, attrs["href"]); if (!resolved.empty()) base = resolved; }
        if (name == "p" || name == "div" || name == "br" || name == "h1" || name == "h2" || name == "h3" || name == "li" || name == "tr" || name == "section") newline();
        if (name == "a") {
            if (!closing) { auto attrs = attributes(tag, name_end); anchor = resolve_url(base, attrs["href"]); anchor_start = page.text.size(); }
            else if (!anchor.empty() && page.links.size() < 64) {
                auto label = trim(page.text.substr(anchor_start)); if (label.empty()) label = anchor;
                page.links.push_back({label.substr(0, 160), anchor}); page.text += " [" + std::to_string(page.links.size()) + "]"; anchor.clear();
            }
        }
    }
    return page;
}
}
