#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace kindle {
namespace fs = std::filesystem;
struct Book {
    std::string path, title, format;
    std::uintmax_t bytes = 0;
};
struct CatalogPage { std::vector<Book> books; bool more = false; };
class Catalog {
    fs::path root_, index_;
public:
    Catalog(fs::path root, fs::path index);
    std::size_t rebuild(); // Explicit, streaming scan. No background indexing.
    CatalogPage page(const std::string& query, const std::string& after = {}, bool descending = false) const;
    const fs::path& root() const { return root_; }
};
struct TextPage { std::string text; std::uint64_t start = 0, next = 0; bool end = false; };
TextPage read_page(const fs::path& file, std::uint64_t offset, unsigned columns, unsigned lines);
struct ReadingState { std::uint64_t offset = 0, bookmark = 0; bool has_bookmark = false; };
struct Settings { unsigned font = 22, line_spacing = 6; bool wifi_demo = false; unsigned light_demo = 12; };
class StateStore {
    fs::path root_;
public:
    explicit StateStore(fs::path root);
    ReadingState load(const std::string& book) const;
    void save(const std::string& book, const ReadingState& state) const;
    Settings settings() const;
    void save_settings(const Settings& settings) const;
    std::string last_book() const;
    void set_last_book(const std::string& book) const;
};
struct Rect { int x, y, width, height; };
enum class Refresh { none, partial, full };
class RefreshPolicy {
    unsigned partials_ = 0;
public:
    Refresh request(Rect area, bool scene_change = false);
};
// Simulation policy only: no real radio or suspend calls.
class IdlePolicy {
    std::uint64_t last_ = 0;
public:
    void activity(std::uint64_t now) { last_ = now; }
    bool wifi_due(std::uint64_t now, bool busy) const { return !busy && now >= last_ && now - last_ >= 300000; }
};
}
