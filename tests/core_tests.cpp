#include "kindle/core.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace kindle;
static unsigned checks = 0;
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
int main() {
    fs::path root = fs::temp_directory_path() / ("kindle-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root / "books");
        for (int i = 0; i < 25; ++i) { std::ofstream f(root / "books" / ("Book " + std::to_string(i) + ".txt")); f << "hello\n"; }
        std::ofstream(root / "books" / "ignore.exe") << "ignored";
        Catalog c(root / "books", root / "catalog");
        check(c.rebuild() == 25, "format filter");
        auto page = c.page(""); check(page.books.size() == 8 && page.more, "bounded first page");
        std::size_t count = page.books.size(); std::string last = page.books.back().path;
        while (page.more) { page = c.page("", last); check(!page.books.empty() && page.books.front().path > last, "cursor ordering"); count += page.books.size(); last = page.books.back().path; }
        check(count == 25, "no duplicate or lost catalog items");
        check(c.page("bOoK 24").books.size() == 1, "ASCII case insensitive search");
        check(c.page("absent").books.empty(), "empty query result");
        auto reverse = c.page("", "", true); check(reverse.books.front().path > reverse.books.back().path, "reverse order");
        StateStore store(root / "state"); check(store.load("missing").offset == 0, "missing state default");
        store.save("A", {100, 25, true}); store.save("A", {200, 25, true});
        check(store.load("A").offset == 200 && store.load("A").has_bookmark, "state replacement");
        check(store.load("B").offset == 0, "state isolation");
        store.save("layout", {2, 1, true, "/body/section[2]/p[1].0", "/body/section[1]/p[3].0"});
        auto layout = store.load("layout");
        check(layout.position == "/body/section[2]/p[1].0" && layout.bookmark_position == "/body/section[1]/p[3].0", "Stable layout positions survive state persistence");
        store.set_last_book("a path with spaces.txt"); check(store.last_book() == "a path with spaces.txt", "last book");
        Settings s; s.font = 28; s.line_spacing = 10; store.save_settings(s); check(store.settings().font == 28, "settings round trip");
        auto book = root / "unicode.txt";
        std::string content = "Один два три четыре пять.\nHello world!\n";
        { std::ofstream f(book, std::ios::binary); f << content; }
        std::string restored; std::uint64_t offset = 0;
        for (unsigned i = 0; i < 100; ++i) { auto p = read_page(book, offset, 8, 1); check(p.next > offset || p.end, "reader advances");
            // Wrapped lines are visual only; check original bytes by reading intervals.
            std::ifstream f(book, std::ios::binary); f.seekg(static_cast<std::streamoff>(offset)); std::string part(static_cast<std::size_t>(p.next - offset), '\0'); f.read(part.data(), part.size()); restored += part;
            offset = p.next; if (p.end) break;
        }
        check(restored == content, "UTF-8 pagination covers all source bytes");
        auto wrap_book = root / "wrap.txt";
        auto write_text = [&](const std::string& value) { std::ofstream f(wrap_book, std::ios::binary); f << value; };
        write_text("one two three four");
        auto wrapped = read_page(wrap_book, 0, 8, 1);
        check(wrapped.text == "one two\n" && wrapped.next == 8, "wrap at word boundary with exact resume offset");
        check(read_page(wrap_book, wrapped.next, 8, 2).text == "three\nfour", "resume wrapped words");
        write_text("abcdefgh\nnext");
        wrapped = read_page(wrap_book, 0, 8, 1);
        check(wrapped.text == "abcdefgh\n" && wrapped.next == 9, "exact-width paragraph avoids extra blank line");
        write_text("abcdefghijklmnopq");
        wrapped = read_page(wrap_book, 0, 8, 1);
        check(wrapped.text == "abcdefgh\n" && wrapped.next == 8, "long word fallback advances");
        check(read_page(wrap_book, 16, 8, 1).text == "q", "long word tail");
        write_text("Один два три четыре пять");
        wrapped = read_page(wrap_book, 0, 8, 1);
        check(wrapped.text == "Один два\n", "Cyrillic word boundary");
        auto visible = [](std::string value) {
            std::string result;
            for (char c : value) if (c != ' ' && c != '\t' && c != '\r' && c != '\n') result += c;
            return result;
        };
        const std::string mixed = "Один два три четыре пять.\r\n\r\n  alpha\tbeta  gamma delta\nabcdefghijklmnopqrstuvw xyz 🙂🙂🙂🙂🙂🙂🙂🙂🙂 конец\n";
        write_text(mixed);
        for (unsigned width : {8u, 9u, 16u}) for (unsigned height : {1u, 2u, 3u}) {
            std::string displayed;
            std::uint64_t resume = 0;
            bool ended = false;
            for (unsigned i = 0; i < mixed.size(); ++i) {
                auto part = read_page(wrap_book, resume, width, height);
                if (part.next <= resume && !part.end) throw std::runtime_error("Wrapped reader stopped advancing");
                displayed += part.text;
                resume = part.next;
                if (part.end) { ended = true; break; }
            }
            check(ended && resume == mixed.size() && visible(displayed) == visible(mixed), "wrapped pages preserve all displayed UTF-8 characters");
        }
        check(read_page(book, 99999, 8, 1).start == 0, "stale position reset");
        { std::ofstream f(root / "empty.txt"); }
        check(read_page(root / "empty.txt", 0, 8, 1).end, "empty book");
        { std::ofstream f(root / "invalid.txt", std::ios::binary); f << "\xed\xa0\x80"; }
        bool invalid = false; try { read_page(root / "invalid.txt", 0, 8, 1); } catch (const std::runtime_error&) { invalid = true; }
        check(invalid, "reject UTF-8 surrogate");
        bool threw = false; try { read_page(book, 0, 0, 0); } catch (const std::invalid_argument&) { threw = true; } check(threw, "geometry validation");
        RefreshPolicy refresh; check(refresh.request({0,0,0,0}) == Refresh::none, "no redraw on idle");
        check(refresh.request({0,0,100,100}, true) == Refresh::full, "scene refresh");
        for (int i=0; i<5; ++i) check(refresh.request({0,0,10,10}) == Refresh::partial, "partial refresh");
        check(refresh.request({0,0,10,10}) == Refresh::full, "periodic ghosting reset");
        IdlePolicy idle; idle.activity(500); check(!idle.wifi_due(300499, false), "idle deadline"); check(idle.wifi_due(300500, false), "idle expiry"); check(!idle.wifi_due(400000, true), "active transfer prevents radio off");
        fs::remove_all(root); std::cout << checks << " checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << " (fixtures: " << root << ")\n"; return 1; }
}
