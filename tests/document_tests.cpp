#include "kindle/document.hpp"
#include "miniz.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>
using namespace kindle;
namespace {
unsigned checks = 0;
void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
void write(const fs::path& path, const std::string& data) { std::ofstream f(path, std::ios::binary); f << data; }
std::string read(const fs::path& path) { std::ifstream f(path, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
std::string illustration() {
    std::vector<unsigned char> pixels(240 * 120 * 3, 255);
    for (int y = 0; y < 120; ++y) for (int x = 0; x < 240; ++x) {
        unsigned char shade = ((x / 30 + y / 30) % 2) ? 40 : 215;
        for (int c = 0; c < 3; ++c) pixels[(y * 240 + x) * 3 + c] = shade;
    }
    std::size_t length = 0; void* data = tdefl_write_image_to_png_file_in_memory(pixels.data(), 240, 120, 3, &length);
    if (!data) throw std::runtime_error("Cannot create image fixture");
    std::string result(static_cast<char*>(data), length); mz_free(data); return result;
}
std::string base64(const std::string& value) {
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < value.size(); i += 3) {
        unsigned bits = static_cast<unsigned char>(value[i]) << 16;
        if (i + 1 < value.size()) bits |= static_cast<unsigned char>(value[i+1]) << 8;
        if (i + 2 < value.size()) bits |= static_cast<unsigned char>(value[i+2]);
        out += alphabet[(bits >> 18) & 63]; out += alphabet[(bits >> 12) & 63];
        out += i + 1 < value.size() ? alphabet[(bits >> 6) & 63] : '=';
        out += i + 2 < value.size() ? alphabet[bits & 63] : '=';
    }
    return out;
}
void make_epub(const fs::path& path, bool escape = false) {
    mz_zip_archive zip{};
#ifdef _WIN32
    auto file = _wfopen(path.c_str(), L"wb+");
#else
    auto file = fopen(path.c_str(), "wb+");
#endif
    if (!file || !mz_zip_writer_init_cfile(&zip, file, 0)) throw std::runtime_error("Fixture ZIP failed");
    auto add = [&](const char* name, const std::string& text) { if (!mz_zip_writer_add_mem(&zip, name, text.data(), text.size(), std::string(name) == "mimetype" ? MZ_NO_COMPRESSION : MZ_BEST_COMPRESSION)) throw std::runtime_error("Fixture ZIP member failed"); };
    add("mimetype", "application/epub+zip");
    add("META-INF/container.xml", "<?xml version='1.0'?><container version='1.0' xmlns='urn:oasis:names:tc:opendocument:xmlns:container'><rootfiles><rootfile full-path='OPS/book.opf' media-type='application/oebps-package+xml'/></rootfiles></container>");
    add("OPS/book.opf", std::string("<package xmlns='http://www.idpf.org/2007/opf' version='2.0' unique-identifier='book-id'><metadata xmlns:dc='http://purl.org/dc/elements/1.1/'><dc:identifier id='book-id'>urn:kindle-lite:sample</dc:identifier><dc:title>Проверка EPUB</dc:title><dc:language>ru</dc:language></metadata><manifest><item id='second' href='two.xhtml' media-type='application/xhtml+xml'/><item id='style' href='style.css' media-type='text/css'/><item id='image' href='illustration.png' media-type='image/png'/><item id='first' href='") + (escape ? "../../outside.xhtml" : "one%20chapter.xhtml") + "' media-type='application/xhtml+xml'/></manifest><spine><itemref idref='first'/><itemref idref='second'/></spine></package>");
    add("OPS/style.css", "h1 { text-align:center; font-size:1.6em; } .lead { border:2px solid black; padding:0.7em; background-color:#ddd; } img { display:block; margin:auto; width:420px; }");
    add("OPS/illustration.png", illustration());
    std::string paragraph;
    for (int i = 0; i < 40; ++i) paragraph += "<p>Это первая глава EPUB. Перелистывание сохраняет позицию. В этой книге проверяем русский текст, абзацы и порядок глав.</p>";
    add("OPS/one chapter.xhtml", "<html xmlns='http://www.w3.org/1999/xhtml'><head><title>Hidden metadata</title><link rel='stylesheet' type='text/css' href='style.css'/></head><body><h1>Первая глава</h1><img src='illustration.png' alt='Шахматная иллюстрация'/><p class='lead'>Этот абзац оформлен встроенным CSS книги.</p>" + paragraph + "<p>AT&amp;T &#169; <em>выделение</em></p></body></html>");
    add("OPS/two.xhtml", "<html xmlns='http://www.w3.org/1999/xhtml'><head><title>Глава 2</title></head><body><h1>Вторая глава</h1><p>Конец проверочной книги.</p></body></html>");
    if (!mz_zip_writer_finalize_archive(&zip)) throw std::runtime_error("Fixture ZIP finalization failed");
    mz_zip_writer_end(&zip); fclose(file);
}
void make_fb2(const fs::path& path) {
    std::string paragraphs;
    for (int i = 0; i < 40; ++i) paragraphs += "<p>Это текст FictionBook. Внутри абзаца есть <emphasis>выделенные слова</emphasis>, которые должны остаться в тексте. Позиция чтения и закладка сохраняются.</p>";
    write(path, "<?xml version='1.0' encoding='UTF-8'?><FictionBook xmlns='http://www.gribuser.ru/xml/fictionbook/2.0' xmlns:l='http://www.w3.org/1999/xlink'><description><title-info><book-title>Не текст книги</book-title></title-info></description><body><section><title><p>Проверка FB2</p></title><image l:href='#illustration'/>" + paragraphs + "</section></body><binary id='illustration' content-type='image/png'>" + base64(illustration()) + "</binary><binary>SHOULDNOTAPPEAR</binary></FictionBook>");
}
}
int main(int argc, char** argv) {
    auto root = fs::temp_directory_path() / ("kindle-documents-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root);
        make_epub(root / "sample.epub"); make_fb2(root / "sample.fb2");
        auto text = read(prepare_document(root / "sample.epub", root / "cache"));
        check(text.find("Первая глава") < text.find("Вторая глава"), "EPUB uses spine order, not ZIP/manifest order");
        check(text.find("AT&T © выделение") != std::string::npos, "EPUB entities and inline markup");
        check(text.find("Hidden metadata") == std::string::npos, "EPUB excludes head");
        auto cached = prepare_document(root / "sample.epub", root / "cache");
        check(read(cached) == text, "EPUB cache round trip");
        auto fb2 = read(prepare_document(root / "sample.fb2", root / "cache"));
        check(fb2.find("выделенные слова") != std::string::npos && fb2.find("Проверка FB2") != std::string::npos, "FB2 body and inline text");
        check(fb2.find("SHOULDNOTAPPEAR") == std::string::npos && fb2.find("Не текст книги") == std::string::npos, "FB2 excludes metadata and binary");
        check(read_page(cached, 0, 24, 8).next > 0 && !read_page(cached, 0, 24, 8).end, "Imported EPUB paginates");
        make_epub(root / "escape.epub", true);
        bool rejected = false; try { prepare_document(root / "escape.epub", root / "cache"); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Reject escaping EPUB path");
        write(root / "bad.fb2", "<FictionBook><body><p>broken");
        rejected = false; try { prepare_document(root / "bad.fb2", root / "cache"); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Malformed XML rejected");
        write(root / "cp1251.fb2", "<?xml version='1.0' encoding='windows-1251'?><FictionBook><body><p>\xcf\xf0\xe8\xe2\xe5\xf2</p></body></FictionBook>");
        check(read(prepare_document(root / "cp1251.fb2", root / "cache")).find("Привет") != std::string::npos, "FB2 Windows-1251 converted to UTF-8");
        write(root / "encoding.fb2", "<?xml version='1.0' encoding='iso-8859-1'?><FictionBook><body><p>text</p></body></FictionBook>");
        rejected = false; try { prepare_document(root / "encoding.fb2", root / "cache"); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Unsupported encoding fails explicitly");
        write(root / "plain.TXT", "plain");
        check(prepare_document(root / "plain.TXT", root / "cache") == root / "plain.TXT", "TXT remains direct, case insensitive extension");
        if (argc == 3 && std::string(argv[1]) == "--samples") {
            auto output = fs::u8path(argv[2]); fs::create_directories(output);
            fs::copy_file(root / "sample.epub", output / "Sample.epub", fs::copy_options::overwrite_existing);
            fs::copy_file(root / "sample.fb2", output / "Sample.fb2", fs::copy_options::overwrite_existing);
        }
        fs::remove_all(root);
        std::cout << checks << " document checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << " fixtures=" << root << '\n'; return 1; }
}
