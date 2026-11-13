#include "kindle/core.hpp"
#include <fstream>
#include <stdexcept>

namespace kindle {
namespace {
std::string read_character(std::istream& in) {
    unsigned char lead = static_cast<unsigned char>(in.get());
    unsigned n = lead < 0x80 ? 1 : (lead >= 0xc2 && lead <= 0xdf ? 2 : (lead <= 0xef && lead >= 0xe0 ? 3 : (lead >= 0xf0 && lead <= 0xf4 ? 4 : 0)));
    if (!n) throw std::runtime_error("TXT must use UTF-8 encoding");
    std::uint32_t codepoint = lead & (n == 1 ? 0x7f : (0x7f >> n));
    std::string character(1, static_cast<char>(lead));
    for (unsigned i = 1; i < n; ++i) {
        int c = in.get();
        if (c == EOF || (c & 0xc0) != 0x80) throw std::runtime_error("Invalid UTF-8 text");
        codepoint = (codepoint << 6) | (c & 0x3f);
        character += static_cast<char>(c);
    }
    if ((n == 2 && codepoint < 0x80) || (n == 3 && codepoint < 0x800) ||
        (n == 4 && codepoint < 0x10000) || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) throw std::runtime_error("Invalid UTF-8 codepoint");
    return character;
}
}
TextPage read_page(const fs::path& file, std::uint64_t offset, unsigned columns, unsigned lines) {
    if (columns < 8 || columns > 200 || !lines || lines > 100) throw std::invalid_argument("Invalid page geometry");
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open text book");
    in.seekg(0, std::ios::end);
    const auto size = static_cast<std::uint64_t>(in.tellg());
    if (offset > size) offset = 0;
    in.seekg(static_cast<std::streamoff>(offset));
    if (!offset && size >= 3) {
        char bom[3]{}; in.read(bom, 3);
        if (std::string(bom, 3) != "\xef\xbb\xbf") in.seekg(0);
    }
    TextPage p; p.start = offset;
    for (unsigned row = 0; row < lines && in.peek() != EOF; ++row) {
        std::string line;
        unsigned width = 0;
        std::size_t break_index = std::string::npos;
        std::streampos break_position{};
        bool ended_line = false;
        while (in.peek() != EOF) {
            auto position = in.tellg();
            auto character = read_character(in);
            if (character == "\r") continue;
            if (character == "\n") { ended_line = true; break; }
            bool space = character == " " || character == "\t";
            if (width == columns) {
                if (!space) {
                    if (break_index != std::string::npos) {
                        line.resize(break_index);
                        in.seekg(break_position);
                    } else {
                        // A word longer than a whole line must still advance.
                        in.seekg(position);
                    }
                }
                ended_line = true;
                break;
            }
            if (space && width) { break_index = line.size(); break_position = in.tellg(); }
            line += space ? " " : character;
            ++width;
        }
        p.text += line;
        if (ended_line) p.text += '\n';
    }
    auto position = in.tellg();
    p.next = position < 0 ? size : static_cast<std::uint64_t>(position);
    p.end = p.next >= size;
    return p;
}
}
