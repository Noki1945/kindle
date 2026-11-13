#include "kindle/document.hpp"
#include "tinyxml2.h"
#include "miniz.h"
#include <fstream>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace kindle {
namespace {
using namespace tinyxml2;
constexpr std::size_t limit = 16 * 1024 * 1024;
std::string local(const char* name) {
    std::string value = name ? name : "";
    auto colon = value.find(':');
    return colon == std::string::npos ? value : value.substr(colon + 1);
}
XMLElement* child(XMLElement* element, const std::string& name) {
    if (element) for (auto e = element->FirstChildElement(); e; e = e->NextSiblingElement()) if (local(e->Name()) == name) return e;
    return nullptr;
}
void parse(XMLDocument& document, std::string data) {
    auto declaration_end = data.find("?>");
    auto declaration_text = data.substr(0, std::min(declaration_end, std::size_t(512)));
    std::transform(declaration_text.begin(), declaration_text.end(), declaration_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto cp1251 = declaration_text.find("windows-1251");
    if (declaration_text.find("<?xml") != std::string::npos && cp1251 != std::string::npos) {
        static constexpr unsigned table[] = {
            0x0402,0x0403,0x201a,0x0453,0x201e,0x2026,0x2020,0x2021,0x20ac,0x2030,0x0409,0x2039,0x040a,0x040c,0x040b,0x040f,
            0x0452,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x0098,0x2122,0x0459,0x203a,0x045a,0x045c,0x045b,0x045f,
            0x00a0,0x040e,0x045e,0x0408,0x00a4,0x0490,0x00a6,0x00a7,0x0401,0x00a9,0x0404,0x00ab,0x00ac,0x00ad,0x00ae,0x0407,
            0x00b0,0x00b1,0x0406,0x0456,0x0491,0x00b5,0x00b6,0x00b7,0x0451,0x2116,0x0454,0x00bb,0x0458,0x0405,0x0455,0x0457,
            0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,0x0418,0x0419,0x041a,0x041b,0x041c,0x041d,0x041e,0x041f,
            0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,0x0428,0x0429,0x042a,0x042b,0x042c,0x042d,0x042e,0x042f,
            0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,0x0438,0x0439,0x043a,0x043b,0x043c,0x043d,0x043e,0x043f,
            0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,0x0448,0x0449,0x044a,0x044b,0x044c,0x044d,0x044e,0x044f};
        data.replace(cp1251, 12, "UTF-8");
        std::string converted;
        for (unsigned char c : data) {
            unsigned cp = c < 128 ? c : table[c - 128];
            if (cp < 128) converted += static_cast<char>(cp);
            else if (cp < 2048) { converted += static_cast<char>(0xc0 | (cp >> 6)); converted += static_cast<char>(0x80 | (cp & 63)); }
            else { converted += static_cast<char>(0xe0 | (cp >> 12)); converted += static_cast<char>(0x80 | ((cp >> 6) & 63)); converted += static_cast<char>(0x80 | (cp & 63)); }
            if (converted.size() > limit) throw std::runtime_error("Converted book XML exceeds 16 MiB");
        }
        data = std::move(converted);
    }
    // TinyXML2 doesn't fetch external entities or run code.
    if (document.Parse(data.data(), data.size()) != XML_SUCCESS) throw std::runtime_error("Invalid book XML");
    auto declaration = document.FirstChild() ? document.FirstChild()->ToDeclaration() : nullptr;
    if (declaration) {
        std::string d = declaration->Value();
        std::transform(d.begin(), d.end(), d.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        auto enc = d.find("encoding");
        if (enc != std::string::npos && d.find("utf-8", enc) == std::string::npos && d.find("utf8", enc) == std::string::npos)
            throw std::runtime_error("Supported book encodings: UTF-8 and Windows-1251");
    }
}
void newline(std::string& out) { if (!out.empty() && out.back() != '\n') out += '\n'; }
void flatten(XMLNode* node, std::string& out, unsigned depth = 0) {
    if (depth > 100) throw std::runtime_error("Book nesting too deep");
    if (auto t = node->ToText()) {
        for (unsigned char c : std::string(t->Value())) {
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { if (!out.empty() && out.back() != ' ' && out.back() != '\n') out += ' '; }
            else out += static_cast<char>(c);
        }
    } else {
        auto element = node->ToElement();
        auto tag = element ? local(element->Name()) : "";
        if (tag == "script" || tag == "style" || tag == "binary" || tag == "head") return;
        bool block = tag == "p" || tag == "section" || tag == "title" || tag == "subtitle" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "div" || tag == "li" || tag == "v" || tag == "br" || tag == "empty-line";
        if (block) newline(out);
        for (auto c = node->FirstChild(); c; c = c->NextSibling()) flatten(c, out, depth + 1);
        if (block) newline(out);
    }
    if (out.size() > limit) throw std::runtime_error("Book text exceeds 16 MiB");
}
std::string read(const fs::path& file) {
    if (fs::file_size(file) > limit) throw std::runtime_error("Book XML exceeds 16 MiB");
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open book");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::string member_path(const std::string& base, std::string relative) {
    auto hash = relative.find('#'); if (hash != std::string::npos) relative.resize(hash);
    std::string decoded;
    auto hex = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
    for (std::size_t i = 0; i < relative.size(); ++i) {
        if (relative[i] == '%' && i + 2 < relative.size() && hex(relative[i+1]) >= 0 && hex(relative[i+2]) >= 0) { decoded += static_cast<char>(hex(relative[i+1]) * 16 + hex(relative[i+2])); i += 2; }
        else decoded += relative[i];
    }
    if (decoded.empty() || decoded.find('\0') != std::string::npos || decoded.find(':') != std::string::npos || decoded.find('\\') != std::string::npos)
        throw std::runtime_error("Invalid EPUB member path");
    auto path = (fs::u8path(base) / fs::u8path(decoded)).lexically_normal();
    if (path.is_absolute() || *path.begin() == "..") throw std::runtime_error("EPUB path escapes archive");
    return path.generic_u8string();
}
struct Archive {
    mz_zip_archive zip{};
    explicit Archive(const fs::path& file) {
#ifdef _WIN32
        auto stream = _wfopen(file.c_str(), L"rb");
#else
        auto stream = fopen(file.c_str(), "rb");
#endif
        if (!stream) throw std::runtime_error("Cannot open EPUB");
        handle = stream;
        if (!mz_zip_reader_init_cfile(&zip, handle, 0, 0)) { fclose(handle); handle = nullptr; throw std::runtime_error("Invalid EPUB ZIP"); }
        if (mz_zip_reader_get_num_files(&zip) > 10000) { mz_zip_reader_end(&zip); fclose(handle); handle = nullptr; throw std::runtime_error("Too many EPUB members"); }
    }
    FILE* handle = nullptr;
    ~Archive() { mz_zip_reader_end(&zip); if (handle) fclose(handle); }
    std::size_t total = 0;
    std::string get(const std::string& name) {
        int index = mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
        mz_zip_archive_file_stat stat{};
        if (index < 0 || !mz_zip_reader_file_stat(&zip, index, &stat)) throw std::runtime_error("Missing EPUB member");
        if (stat.m_uncomp_size > limit || total + stat.m_uncomp_size > 2 * limit) throw std::runtime_error("EPUB text exceeds import limit");
        total += static_cast<std::size_t>(stat.m_uncomp_size);
        std::string data(static_cast<std::size_t>(stat.m_uncomp_size), '\0');
        if (!mz_zip_reader_extract_to_mem(&zip, index, data.data(), data.size(), 0)) throw std::runtime_error("Cannot decompress EPUB member");
        return data;
    }
};
std::string epub(const fs::path& source) {
    Archive archive(source);
    if (archive.get("mimetype") != "application/epub+zip") throw std::runtime_error("Not an EPUB book");
    XMLDocument container; parse(container, archive.get("META-INF/container.xml"));
    auto rootfile = child(child(container.RootElement(), "rootfiles"), "rootfile");
    if (!rootfile || !rootfile->Attribute("full-path")) throw std::runtime_error("Missing EPUB package");
    auto package = member_path("", rootfile->Attribute("full-path"));
    XMLDocument opf; parse(opf, archive.get(package));
    auto manifest = child(opf.RootElement(), "manifest");
    auto spine = child(opf.RootElement(), "spine");
    if (!manifest || !spine) throw std::runtime_error("Missing EPUB reading order");
    std::map<std::string, std::string> items;
    for (auto e = manifest->FirstChildElement(); e; e = e->NextSiblingElement()) {
        auto id = e->Attribute("id"), href = e->Attribute("href"), mime = e->Attribute("media-type");
        if (id && href && mime && std::string(mime) == "application/xhtml+xml") items[id] = href;
    }
    std::string out;
    for (auto e = spine->FirstChildElement(); e; e = e->NextSiblingElement()) {
        if (e->Attribute("linear", "no")) continue;
        auto id = e->Attribute("idref");
        if (!id || !items.count(id)) throw std::runtime_error("Unsupported EPUB spine item");
        XMLDocument chapter; parse(chapter, archive.get(member_path(fs::u8path(package).parent_path().generic_u8string(), items.at(id))));
        auto body = child(chapter.RootElement(), "body");
        if (!body) throw std::runtime_error("EPUB chapter has no body");
        flatten(body, out); newline(out);
    }
    return out;
}
}
fs::path prepare_document(const fs::path& source, const fs::path& cache) {
    auto extension = source.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".txt") return source;
    if (extension != ".fb2" && extension != ".epub") throw std::runtime_error("Unsupported book format");
    std::string fingerprint = fs::weakly_canonical(source).u8string() + ':' + std::to_string(fs::file_size(source)) + ':' + std::to_string(fs::last_write_time(source).time_since_epoch().count());
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : fingerprint) { hash ^= c; hash *= 1099511628211ull; }
    std::ostringstream name; name << "v1-" << std::hex << hash << ".txt";
    fs::create_directories(cache);
    auto dest = cache / name.str();
    if (fs::exists(dest)) return dest;
    std::string out;
    if (extension == ".epub") out = epub(source);
    else {
        XMLDocument document; parse(document, read(source));
        auto root = document.RootElement();
        if (!root || local(root->Name()) != "FictionBook") throw std::runtime_error("Not a FictionBook");
        for (auto e = root->FirstChildElement(); e; e = e->NextSiblingElement()) if (local(e->Name()) == "body") { flatten(e, out); newline(out); }
    }
    if (out.empty()) throw std::runtime_error("Book has no readable text");
    auto temp = dest; temp += ".tmp";
    { std::ofstream file(temp, std::ios::binary); file << out; file.close(); if (!file) throw std::runtime_error("Cannot cache book text"); }
    fs::rename(temp, dest);
    return dest;
}
}
