#include "kindle/core.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace kindle {
static std::string key(const std::string& s) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    std::ostringstream out; out << std::hex << h; return out.str();
}
static void write(const fs::path& dest, const std::string& data) {
    auto temp = dest; temp += ".tmp";
    { std::ofstream out(temp, std::ios::binary | std::ios::trunc); out << data; out.close();
      if (!out) throw std::runtime_error("State write failed"); }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("State replace failed");
#else
    fs::rename(temp, dest);
#endif
}
StateStore::StateStore(fs::path root) : root_(std::move(root)) { fs::create_directories(root_); }
ReadingState StateStore::load(const std::string& book) const {
    ReadingState s; std::string path;
    std::ifstream in(root_ / (key(book) + ".state"));
    if (!(in >> std::quoted(path) >> s.offset >> s.bookmark >> s.has_bookmark) || path != book) return {};
    return s;
}
void StateStore::save(const std::string& book, const ReadingState& s) const {
    std::ostringstream out; out << std::quoted(book) << ' ' << s.offset << ' ' << s.bookmark << ' ' << s.has_bookmark << '\n';
    write(root_ / (key(book) + ".state"), out.str());
}
Settings StateStore::settings() const {
    Settings s; std::ifstream in(root_ / "settings");
    if (!(in >> s.font >> s.line_spacing >> s.wifi_demo >> s.light_demo)) return {};
    if (s.font < 16 || s.font > 32 || s.line_spacing > 16 || s.light_demo > 24) return {};
    return s;
}
void StateStore::save_settings(const Settings& s) const {
    std::ostringstream out; out << s.font << ' ' << s.line_spacing << ' ' << s.wifi_demo << ' ' << s.light_demo << '\n';
    write(root_ / "settings", out.str());
}
std::string StateStore::last_book() const { std::ifstream in(root_ / "last"); std::string b; in >> std::quoted(b); return b; }
void StateStore::set_last_book(const std::string& b) const { std::ostringstream o; o << std::quoted(b); write(root_ / "last", o.str()); }
}
