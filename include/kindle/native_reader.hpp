#pragma once
#include "kindle/core.hpp"
#include <functional>
namespace kindle {
struct RenderedPage { std::uint64_t page = 0, count = 0; std::string position; };
class NativeReader {
    int process_ = -1, input_ = -1, output_ = -1;
    std::string pending_;
    std::function<bool()> cancelled_;
    RenderedPage current_;
    RenderedPage response();
public:
    NativeReader(const fs::path& book, unsigned font, std::function<bool()> cancelled);
    ~NativeReader();
    NativeReader(const NativeReader&) = delete;
    RenderedPage seek(std::uint64_t page, const std::string& position);
    static bool available();
};
}
