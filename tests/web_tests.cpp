#include "kindle/web.hpp"
#include <iostream>
#include <stdexcept>
using namespace kindle;
int main() {
    unsigned checks = 0;
    auto check = [&](bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); };
    try {
        check(https_url("https://example.com/a?x=1"), "HTTPS accepted");
        check(!https_url("http://example.com") && !https_url("https://u:p@example.com") && !https_url("https://example.com/\n-x"), "Unsafe addresses rejected");
        check(resolve_url("https://example.com/a/b", "../c?q=1&amp;") == "https://example.com/c?q=1&amp;", "Relative path resolution");
        check(resolve_url("https://example.com/a/b?old", "?new") == "https://example.com/a/b?new", "Relative query");
        check(resolve_url("https://example.com", "/page") == "https://example.com/page", "Root relative");
        check(resolve_url("https://example.com/a", "//other.example/b") == "https://other.example/b", "Scheme relative");
        check(resolve_url("https://example.com/a", "javascript:alert(1)").empty(), "Script link rejected");
        auto p = parse_web_page("<head><title>hidden</title></head><h1>Заголовок</h1><p>Привет &#169; &#x1F642; &amp; мир <a href='../next?q=1&amp;x=2' title='a > b'>ссылка</a></p><script>bad</script><style>hidden</style><!-- hidden -->", "https://example.com/a/b");
        check(p.text.find("Заголовок") != std::string::npos && p.text.find("Привет © 🙂 & мир") != std::string::npos, "UTF-8 and numeric entities");
        check(p.text.find("hidden") == std::string::npos && p.text.find("bad") == std::string::npos, "Non-content tags excluded");
        check(p.links.size() == 1 && p.links[0].label == "ссылка" && p.links[0].url == "https://example.com/next?q=1&x=2", "Links preserve target and label");
        check(p.text.find("[1]") != std::string::npos, "Link marker displayed");
        check(parse_web_page("<p>x<script>not closed", "https://example.com").text.find("not closed") == std::string::npos, "Unclosed script remains hidden");
        std::string many; for (int i = 0; i < 100; ++i) many += "<a href='/x'>x</a>";
        check(parse_web_page(many, "https://example.com").links.size() == 64, "Bounded link list");
        std::cout << checks << " web checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
