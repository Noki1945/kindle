#include "kindle/native_reader.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <chrono>
#include <stdexcept>
namespace kindle {
bool NativeReader::available() { return fs::exists("engines/koreader/luajit") && fs::exists("reader-worker.lua"); }
NativeReader::NativeReader(const fs::path& book, unsigned font, std::function<bool()> cancelled) : cancelled_(std::move(cancelled)) {
    signal(SIGPIPE, SIG_IGN);
    fs::create_directories("runtime/cre-cache");
    int to_worker[2], from_worker[2];
    if (pipe2(to_worker, O_CLOEXEC)) throw std::runtime_error("Cannot create reader input");
    if (pipe2(from_worker, O_CLOEXEC | O_NONBLOCK)) { close(to_worker[0]); close(to_worker[1]); throw std::runtime_error("Cannot create reader output"); }
    auto file = fs::absolute(book).u8string(), size = std::to_string(font);
    process_ = fork();
    if (!process_) {
        sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, nullptr);
        dup2(to_worker[0], 0); dup2(from_worker[1], 1);
        for (int fd : {to_worker[0], to_worker[1], from_worker[0], from_worker[1]}) close(fd);
        int log = open("runtime/reader-engine.log", O_WRONLY | O_CREAT | O_APPEND, 0600); if (log >= 0) { dup2(log, 2); close(log); }
        if (chdir("engines/koreader")) _exit(126);
        setenv("LD_LIBRARY_PATH", "libs:/usr/lib", 1);
        execl("./luajit", "luajit", "../../reader-worker.lua", file.c_str(), size.c_str(), "/tmp/kindle-lite-page.png", nullptr);
        _exit(127);
    }
    close(to_worker[0]); close(from_worker[1]); input_ = to_worker[1]; output_ = from_worker[0];
    if (process_ < 0) { close(input_); close(output_); input_ = output_ = -1; throw std::runtime_error("Cannot launch reader"); }
    try { current_ = response(); }
    catch (...) { kill(process_, SIGTERM); while (waitpid(process_, nullptr, 0) < 0 && errno == EINTR) {} close(input_); close(output_); process_ = input_ = output_ = -1; throw; }
}
NativeReader::~NativeReader() {
    if (process_ > 0) { kill(process_, SIGTERM); while (waitpid(process_, nullptr, 0) < 0 && errno == EINTR) {} }
    if (input_ >= 0) close(input_); if (output_ >= 0) close(output_);
}
RenderedPage NativeReader::response() {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
    for (;;) {
        std::size_t end;
        while ((end = pending_.find('\n')) != std::string::npos) {
            auto line = pending_.substr(0, end); pending_.erase(0, end + 1);
            if (line.rfind("KINDLE_READY\t", 0) == 0) {
                std::istringstream row(line.substr(13)); RenderedPage page;
                if (!(row >> page.page >> page.count) || !page.page || page.page > page.count) throw std::runtime_error("Invalid reader response");
                row.get(); std::getline(row, page.position);
                --page.page;
                return page;
            }
        }
        if (cancelled_() || std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Reader cancelled or timed out");
        pollfd fd{output_, POLLIN, 0};
        int ready = poll(&fd, 1, 100);
        if (ready < 0 && errno != EINTR) throw std::runtime_error("Reader channel failed");
        if (ready > 0) {
            char bytes[4096]; auto count = read(output_, bytes, sizeof(bytes));
            if (!count) throw std::runtime_error("Layout engine stopped; see reader-engine.log");
            if (count > 0) { pending_.append(bytes, static_cast<std::size_t>(count)); if (pending_.size() > 65536) throw std::runtime_error("Reader response too long"); }
        }
    }
}
RenderedPage NativeReader::seek(std::uint64_t page, const std::string& position) {
    if (position.find_first_of("\r\n\t") != std::string::npos || position.size() > 8192) throw std::runtime_error("Invalid reading position");
    if ((!position.empty() && position == current_.position) || (position.empty() && page == current_.page)) return current_;
    auto command = position.empty() ? "page\t" + std::to_string(page + 1) + "\n" : "position\t" + position + "\n";
    std::size_t sent = 0;
    while (sent < command.size()) { auto count = write(input_, command.data() + sent, command.size() - sent); if (count < 0) { if (errno == EINTR) continue; throw std::runtime_error("Reader command failed"); } sent += count; }
    current_ = response(); return current_;
}
}
