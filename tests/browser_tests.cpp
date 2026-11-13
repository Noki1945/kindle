#include "kindle/browser.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc, char**) {
    try {
        auto text = kindle::html_text("<h1>Title</h1><script>alert(1)</script><style>body{}</style><p>Hello &amp; world</p>");
        if (text.find("alert") != std::string::npos || text.find("body{") != std::string::npos || text.find("Hello & world") == std::string::npos) throw std::runtime_error("HTML conversion failed");
        bool rejected = false;
        try { kindle::fetch_article(L"http://example.com"); } catch (const std::exception&) { rejected = true; }
        if (!rejected) throw std::runtime_error("HTTP was not rejected");
        if (argc > 1) { auto article = kindle::fetch_article(L"https://example.com"); if (article.find("Example Domain") == std::string::npos) throw std::runtime_error("HTTPS smoke response unexpected"); std::cout << "HTTPS example.com smoke passed\n"; }
        std::cout << "Browser checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
