#include "kindle/core.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <cctype>

namespace kindle {
static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
Catalog::Catalog(fs::path root, fs::path index) : root_(fs::absolute(root)), index_(std::move(index)) {}
std::size_t Catalog::rebuild() {
    if (!fs::is_directory(root_)) throw std::runtime_error("Book directory does not exist");
    if (!index_.parent_path().empty()) fs::create_directories(index_.parent_path());
    auto temp = index_; temp += ".tmp";
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create index");
    out << "KLO1\n";
    std::size_t count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(root_, fs::directory_options::skip_permission_denied)) {
        if (entry.is_symlink() || !entry.is_regular_file()) continue;
        auto ext = lower(entry.path().extension().u8string());
        if (ext != ".txt" && ext != ".epub" && ext != ".fb2" && ext != ".pdf") continue;
        auto relative = fs::relative(entry.path(), root_).generic_u8string();
        // One bounded record per line; control characters are not valid catalog names.
        if (relative.size() > 4096 || relative.find_first_of("\r\n") != std::string::npos) continue;
        out << std::quoted(relative) << ' ' << std::quoted(entry.path().stem().u8string())
            << ' ' << std::quoted(ext.substr(1)) << ' ' << entry.file_size() << '\n';
        ++count;
    }
    out.close();
    if (!out) throw std::runtime_error("Index write failed");
#ifdef _WIN32
    // ReplaceFile semantics are implemented by the state writer; retain old index on scan failures.
    if (fs::exists(index_)) { auto old = index_; old += ".old"; fs::remove(old); fs::rename(index_, old);
        try { fs::rename(temp, index_); } catch (...) { fs::rename(old, index_); throw; }
        fs::remove(old);
    } else fs::rename(temp, index_);
#else
    fs::rename(temp, index_);
#endif
    return count;
}
CatalogPage Catalog::page(const std::string& query, const std::string& after, bool descending) const {
    std::ifstream in(index_, std::ios::binary);
    std::string header; std::getline(in, header);
    if (header != "KLO1") throw std::runtime_error("Index missing or invalid; rebuild library");
    CatalogPage result;
    const auto needle = lower(query);
    Book b;
    auto before = [descending](const Book& a, const Book& z) { return descending ? a.path > z.path : a.path < z.path; };
    while (in >> std::quoted(b.path) >> std::quoted(b.title) >> std::quoted(b.format) >> b.bytes) {
        if (!after.empty() && (descending ? b.path >= after : b.path <= after)) continue;
        if (lower(b.title).find(needle) == std::string::npos) continue;
        auto pos = std::lower_bound(result.books.begin(), result.books.end(), b, before);
        result.books.insert(pos, b);
        if (result.books.size() > 8) { result.books.pop_back(); result.more = true; }
    }
    if (!in.eof()) throw std::runtime_error("Corrupt catalog record");
    return result;
}
}
