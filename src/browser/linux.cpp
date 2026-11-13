#include "kindle/web.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
namespace kindle {
namespace {
std::string read_file(const std::string& file, std::size_t limit) {
    if (std::filesystem::file_size(file) > limit) throw std::runtime_error("Page exceeds size limit");
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read page");
    return {std::istreambuf_iterator<char>(in), {}};
}
void unblock() { sigset_t set; sigemptyset(&set); sigprocmask(SIG_SETMASK, &set, nullptr); }
}
int fetch_web_worker(const std::string& url, const std::string& prefix) {
    try {
        if (!https_url(url)) throw std::runtime_error("Use an https:// address");
        auto curl = fork();
        if (curl < 0) throw std::runtime_error("Cannot launch HTTPS client");
        if (!curl) {
            auto output = prefix + ".meta", body = prefix + ".body", headers = prefix + ".headers", errors = prefix + ".errors";
            int fd = open(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600); if (fd < 0) _exit(126); dup2(fd, 1); close(fd);
            fd = open(errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600); if (fd < 0) _exit(126); dup2(fd, 2); close(fd);
            execl("/usr/bin/curl", "curl", "--silent", "--show-error", "--fail", "--location", "--max-redirs", "5", "--proto", "=https", "--proto-redir", "=https", "--connect-timeout", "5", "--max-time", "25", "--max-filesize", "2097152", "--user-agent", "KindleLite/0.2", "--dump-header", headers.c_str(), "--output", body.c_str(), "--write-out", "%{url_effective}\n%{content_type}\n", "--", url.c_str(), nullptr);
            _exit(127);
        }
        int status = 0; while (waitpid(curl, &status, 0) < 0) { if (errno != EINTR) throw std::runtime_error("HTTPS client wait failed"); }
        if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            auto error = read_file(prefix + ".errors", 8192);
            throw std::runtime_error(error.empty() ? "HTTPS request failed" : error.substr(0, 240));
        }
        auto meta = read_file(prefix + ".meta", 8192);
        auto type_start = meta.find('\n');
        std::string type = type_start == std::string::npos ? "" : meta.substr(type_start + 1);
        std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (type.find("text/html") == std::string::npos && type.find("text/plain") == std::string::npos && type.find("application/xhtml+xml") == std::string::npos)
            throw std::runtime_error("This page is not HTML or plain text");
        auto charset = type.find("charset=");
        if (charset != std::string::npos && type.find("utf-8", charset) == std::string::npos && type.find("us-ascii", charset) == std::string::npos)
            throw std::runtime_error("Page encoding must be UTF-8");
        read_file(prefix + ".body", 2 * 1024 * 1024);
        return 0;
    } catch (const std::exception& e) { std::cout << e.what(); return 1; }
}
WebRequest::~WebRequest() { cancel(); }
void WebRequest::cancel() {
    if (child_ > 0) { kill(-child_, SIGTERM); kill(child_, SIGTERM); while (waitpid(child_, nullptr, 0) < 0 && errno == EINTR) {} child_ = -1; }
    if (descriptor_ >= 0) { close(descriptor_); descriptor_ = -1; }
}
void WebRequest::start(const std::string& url, const std::string& prefix) {
    cancel();
    if (!https_url(url)) throw std::runtime_error("Use an https:// address");
    std::filesystem::create_directories(std::filesystem::path(prefix).parent_path());
    int channel[2]; if (pipe2(channel, O_CLOEXEC | O_NONBLOCK)) throw std::runtime_error("Cannot create browser channel");
    auto child = fork();
    if (child < 0) { close(channel[0]); close(channel[1]); throw std::runtime_error("Cannot launch browser request"); }
    if (!child) {
        setsid(); unblock(); dup2(channel[1], 1); close(channel[0]); close(channel[1]);
        execl("/proc/self/exe", "kindle_ui", "--fetch", url.c_str(), prefix.c_str(), nullptr);
        _exit(127);
    }
    close(channel[1]); child_ = child; descriptor_ = channel[0]; prefix_ = prefix;
}
bool WebRequest::finish(WebPage& result, std::string& error) {
    if (child_ < 0) return false;
    int status = 0;
    if (waitpid(child_, &status, WNOHANG) != child_) return false;
    child_ = -1;
    char buffer[1024]; auto count = ::read(descriptor_, buffer, sizeof(buffer)); close(descriptor_); descriptor_ = -1;
    try {
        if (!WIFEXITED(status) || WEXITSTATUS(status)) throw std::runtime_error(count > 0 ? std::string(buffer, static_cast<std::size_t>(count)) : "Page load failed");
        auto meta = read_file(prefix_ + ".meta", 8192);
        auto end = meta.find('\n'); auto url = meta.substr(0, end);
        if (!https_url(url)) throw std::runtime_error("Invalid redirect address");
        auto body = read_file(prefix_ + ".body", 2 * 1024 * 1024);
        if (meta.find("text/plain", end) != std::string::npos) result = {url, body, {}};
        else result = parse_web_page(body, url);
        if (result.text.empty()) throw std::runtime_error("No readable text; site may require JavaScript");
        error.clear();
    } catch (const std::exception& e) { error = e.what(); }
    return true;
}
}
