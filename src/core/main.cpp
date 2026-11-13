#include "kindle/core.hpp"
#include <chrono>
#include <iostream>
int main(int argc, char** argv) {
    try {
        if (argc < 3) { std::cout << "kindle_cli index BOOKS [INDEX]\nkindle_cli list INDEX [QUERY]\nkindle_cli read TXT\n"; return 0; }
        auto begin = std::chrono::steady_clock::now();
        std::string command = argv[1];
        if (command == "index") { kindle::Catalog c(std::filesystem::u8path(argv[2]), argc > 3 ? std::filesystem::u8path(argv[3]) : "runtime/catalog"); std::cout << c.rebuild() << " books\n"; }
        else if (command == "list") { kindle::Catalog c(".", std::filesystem::u8path(argv[2])); for (const auto& b : c.page(argc > 3 ? argv[3] : "").books) std::cout << b.title << " [" << b.format << "]\n"; }
        else if (command == "read") std::cout << kindle::read_page(std::filesystem::u8path(argv[2]), 0, 48, 24).text;
        else return 2;
        std::cerr << "elapsed_us=" << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count() << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
