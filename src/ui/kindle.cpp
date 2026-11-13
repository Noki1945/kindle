#include "kindle/core.hpp"
#include "kindle/document.hpp"
#include "kindle/web.hpp"
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/signalfd.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <csignal>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <limits>

namespace {
using namespace kindle;
volatile sig_atomic_t stopped = 0;
int signal_descriptor = -1;
void consume_signals() {
    signalfd_siginfo information{};
    while (read(signal_descriptor, &information, sizeof(information)) == sizeof(information)) {
        if (information.ssi_signo != SIGCHLD) stopped = 1;
    }
}
const char* renderer = "/mnt/us/extensions/MRInstaller/bin/KHF/fbink";
std::string shortened(const std::string& value, std::size_t characters) {
    std::size_t i = 0, count = 0;
    for (; i < value.size(); ++i) {
        if ((static_cast<unsigned char>(value[i]) & 0xc0) != 0x80 && count++ == characters) break;
    }
    return value.substr(0, i) + (i < value.size() ? "…" : "");
}

void draw(const std::vector<std::string>& arguments, const char* executable = renderer) {
    auto child = fork();
    if (child < 0) throw std::runtime_error("Cannot start renderer");
    if (child == 0) {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, nullptr);
        std::vector<char*> argv{const_cast<char*>(executable)};
        for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
        argv.push_back(nullptr);
        execv(executable, argv.data());
        _exit(127);
    }
    int status = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        int result = waitpid(child, &status, WNOHANG);
        if (result == child) break;
        if (result < 0 && errno != EINTR) throw std::runtime_error("Renderer wait failed");
        if (stopped || std::chrono::steady_clock::now() >= deadline) {
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            throw std::runtime_error(stopped ? "Session stopped" : "Renderer timeout");
        }
        pollfd wake{signal_descriptor, POLLIN, 0};
        poll(&wake, 1, 10000);
        consume_signals();
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status)) throw std::runtime_error("FBInk rendering failed");
}

struct PowerEvents {
    int descriptor = -1, process = -1;
    PowerEvents() {
        int channel[2];
        if (pipe2(channel, O_NONBLOCK | O_CLOEXEC)) throw std::runtime_error("Cannot observe power events");
        process = fork();
        if (process < 0) { close(channel[0]); close(channel[1]); throw std::runtime_error("Cannot launch power observer"); }
        if (!process) {
            setsid();
            sigset_t empty; sigemptyset(&empty); sigprocmask(SIG_SETMASK, &empty, nullptr);
            dup2(channel[1], 1); close(channel[0]); close(channel[1]);
            execl("/bin/sh", "sh", "-c", "while /usr/bin/lipc-wait-event com.lab126.powerd goingToScreenSaver,outOfScreenSaver; do :; done", nullptr);
            _exit(127);
        }
        close(channel[1]); descriptor = channel[0];
    }
    ~PowerEvents() {
        if (process > 0) { kill(-process, SIGTERM); kill(process, SIGTERM); while (waitpid(process, nullptr, 0) < 0 && errno == EINTR) {} }
        if (descriptor >= 0) close(descriptor);
    }
};

void text(int top, int bottom, const std::string& value, int size = 36, int left = 40, int right = 1032) {
    if (value.empty()) return;
    std::string options = "regular=/usr/java/lib/fonts/Bookerly-Regular.ttf,px=" + std::to_string(size)
        + ",top=" + std::to_string(top) + ",bottom=" + std::to_string(1448 - bottom)
        + ",left=" + std::to_string(left) + ",right=" + std::to_string(1072 - right);
    draw({"-q", "-b", "-t", options, "--", value});
}

class Shell {
    Catalog catalog{"books", "runtime/catalog"};
    StateStore store{"runtime/state"};
    Settings settings = store.settings();
    CatalogPage books;
    std::string cursor, current, message;
    fs::path reading_file;
    std::vector<std::string> cursors;
    std::vector<std::uint64_t> history;
    ReadingState progress;
    TextPage page;
    int scene = 0;
    bool reading = false;
    WebRequest request;
    WebPage website;
    TextPage web_page;
    std::vector<std::uint64_t> web_offsets;
    std::vector<std::string> web_history;
    std::string address = "https://example.com", web_message;
    int web_mode = 0; // article, address keyboard, link list
    bool upper_keys = false;
    std::size_t link_start = 0;
    const std::vector<std::string> keys = {"1234567890-.", "qwertyuiop/:", "asdfghjkl_?=", "zxcvbnm&%+#@"};
public:
    Shell() { if (!fs::exists("runtime/catalog")) catalog.rebuild(); books = catalog.page(""); }
    void load_page() {
        const unsigned pixels = settings.font + 10;
        const unsigned columns = 960 / pixels;
        const unsigned lines = 820 / (pixels * 2);
        page = read_page(reading_file, progress.offset, columns, lines);
        progress.offset = page.start;
        std::cout << "READ offset=" << page.start << " next=" << page.next << " end=" << page.end << std::endl;
    }
    void save() { if (!current.empty()) store.save(current, progress); store.save_settings(settings); }
    int browser_descriptor() const { return request.descriptor(); }
    void sleep() { save(); request.cancel(); }
    void begin_web(std::string url, bool record = true) {
        try {
            if (url.find("://") == std::string::npos) url = "https://" + url;
            if (!https_url(url)) throw std::runtime_error("Нужен адрес https:// без пробелов");
            if (record && !website.url.empty()) {
                if (web_history.size() == 32) web_history.erase(web_history.begin());
                web_history.push_back(website.url);
            }
            request.start(url, "runtime/browser/request");
            address = url; web_mode = 0; web_message = "Загрузка…";
        } catch (const std::exception& e) { web_message = e.what(); }
    }
    void browser_ready() {
        WebPage result; std::string error;
        if (!request.finish(result, error)) return;
        if (error.empty()) {
            try {
                std::ofstream output("runtime/browser/page.next.txt", std::ios::binary | std::ios::trunc);
                output << result.text; output.close(); if (!output) throw std::runtime_error("Cannot save page");
                auto first = read_page("runtime/browser/page.next.txt", 0, 30, 10);
                fs::rename("runtime/browser/page.next.txt", "runtime/browser/page.txt");
                website = std::move(result); if (web_mode != 1) address = website.url; web_page = std::move(first);
                web_offsets.clear(); link_start = 0; web_message.clear();
                std::cout << "WEB loaded bytes=" << website.text.size() << " links=" << website.links.size() << std::endl;
            } catch (const std::exception& e) { web_message = e.what(); }
        } else web_message = error;
        if (scene == 1 && !reading) paint();
    }
    void paint_browser() {
        if (web_mode == 1) {
            text(280, 360, "Введите адрес HTTPS", 32);
            auto shown = address.size() > 180 ? address.substr(address.size() - 180) : address;
            while (!shown.empty() && (static_cast<unsigned char>(shown[0]) & 0xc0) == 0x80) shown.erase(shown.begin());
            text(390, 750, shown, 28);
            text(740, 810, upper_keys ? "ABC" : "abc", 28, 40, 350);
            text(740, 810, ".com", 28, 397, 707); text(740, 810, ".ru", 28, 754, 1032);
            for (std::size_t row = 0; row < keys.size(); ++row)
                for (std::size_t col = 0; col < keys[row].size(); ++col)
                    text(820 + static_cast<int>(row) * 96, 900 + static_cast<int>(row) * 96, std::string(1, upper_keys ? static_cast<char>(std::toupper(static_cast<unsigned char>(keys[row][col]))) : keys[row][col]), 36, static_cast<int>(col) * 89 + 16, static_cast<int>(col) * 89 + 84);
            text(1240, 1330, "Стереть", 30, 40, 350); text(1240, 1330, "Очистить", 30, 397, 707); text(1240, 1330, "Открыть", 30, 754, 1032);
            text(1350, 1435, "Отмена", 28, 40, 350); text(1350, 1435, "example.com", 26, 397, 707); text(1350, 1435, "Wikipedia", 26, 754, 1032);
        } else if (web_mode == 2) {
            text(260, 325, "Ссылки страницы", 30);
            if (website.links.empty()) text(370, 520, "На странице нет доступных HTTPS-ссылок.", 30);
            for (std::size_t i = link_start; i < website.links.size() && i < link_start + 8; ++i)
                text(340 + static_cast<int>(i - link_start) * 105, 435 + static_cast<int>(i - link_start) * 105, std::to_string(i + 1) + ". " + shortened(website.links[i].label, 36), 28);
            text(1310, 1430, "Назад", 30, 40, 350); text(1310, 1430, "К статье", 30, 397, 707); text(1310, 1430, "Далее", 30, 754, 1032);
        } else {
            text(260, 340, shortened(address, 55), 26);
            text(375, 1140, web_page.text.empty() ? "Нажмите адрес выше, чтобы открыть сайт.\n\nЭто текстовый браузер: страницы, ссылки и ввод адреса. Сайты с JavaScript пока не поддерживаются." : web_page.text, 30);
            text(1150, 1240, shortened(web_message, 110), 24);
            text(1240, 1310, "Ссылки", 26, 40, 500); text(1240, 1310, "Предыдущий сайт", 26, 576, 1032);
            text(1340, 1440, "Назад", 30, 40, 350); text(1340, 1440, "Адрес", 30, 397, 707); text(1340, 1440, "Далее", 30, 754, 1032);
        }
    }
    void browser_tap(int x, int y) {
        if (web_mode == 1) {
            if (y >= 740 && y < 810) {
                if (x < 357) upper_keys = !upper_keys;
                else if (address.size() < 2040) address += x < 714 ? ".com" : ".ru";
            } else if (y >= 820 && y < 1204) {
                auto row = static_cast<std::size_t>((y - 820) / 96), col = static_cast<std::size_t>(x / 89);
                if (col < keys[row].size() && address.size() < 2048) address += upper_keys ? static_cast<char>(std::toupper(static_cast<unsigned char>(keys[row][col]))) : keys[row][col];
            } else if (y >= 1240 && y < 1330) {
                if (x < 357) {
                    if (!address.empty()) { auto i = address.size() - 1; while (i && (static_cast<unsigned char>(address[i]) & 0xc0) == 0x80) --i; address.resize(i); }
                }
                else if (x < 714) address.clear();
                else begin_web(address);
            } else if (y >= 1350) {
                if (x < 357) web_mode = 0;
                else begin_web(x < 714 ? "https://example.com" : "https://ru.wikipedia.org/wiki/Заглавная_страница");
            } else return;
        } else if (web_mode == 2) {
            if (y >= 340 && y < 1180) {
                auto index = link_start + static_cast<std::size_t>((y - 340) / 105);
                if (index < website.links.size()) begin_web(website.links[index].url);
            } else if (y > 1290) {
                if (x < 357 && link_start >= 8) link_start -= 8;
                else if (x > 714 && link_start + 8 < website.links.size()) link_start += 8;
                else if (x >= 357 && x <= 714) web_mode = 0;
            } else return;
        } else {
            if ((y >= 250 && y < 350) || (y >= 1330 && x >= 357 && x <= 714)) web_mode = 1;
            else if (y >= 1235 && y < 1320) {
                if (x < 536) web_mode = 2;
                else if (!web_history.empty()) { auto previous = web_history.back(); web_history.pop_back(); begin_web(previous, false); }
            } else if (y >= 1330 && !website.text.empty()) {
                auto offset = web_page.start;
                if (x < 357 && !web_offsets.empty()) { offset = web_offsets.back(); web_offsets.pop_back(); }
                else if (x > 714 && !web_page.end) { if (web_offsets.size() == 256) web_offsets.erase(web_offsets.begin()); web_offsets.push_back(offset); offset = web_page.next; }
                try { web_page = read_page("runtime/browser/page.txt", offset, 30, 10); }
                catch (const std::exception& e) { web_message = e.what(); }
            } else return;
        }
        paint();
    }
    void verify_reader_navigation() {
        if (!reading || page.end) throw std::runtime_error("Navigation smoke test needs a multi-page TXT");
        auto original = progress;
        auto first = progress.offset;
        auto next = page.next;
        tap(900, 1350);
        if (progress.offset != next || next <= first || store.load(current).offset != next)
            throw std::runtime_error("Next page persistence failed");
        tap(500, 1350);
        auto saved = store.load(current);
        if (!saved.has_bookmark || saved.bookmark != next) throw std::runtime_error("Bookmark persistence failed");
        tap(100, 1350);
        if (progress.offset != first) throw std::runtime_error("Previous page failed");
        tap(900, 170);
        tap(500, 1140);
        if (!reading || progress.offset != next) throw std::runtime_error("Bookmark reopen failed");
        progress = original; history.clear(); load_page(); save(); paint();
        std::cout << "PASS reader next/previous/persisted bookmark/reopen" << std::endl;
    }
    void verify_formats() {
        for (const auto& name : {"Sample.epub", "Sample.fb2"}) {
            open(name);
            if (current != name || !reading) throw std::runtime_error("Format did not open");
            verify_reader_navigation();
            std::cout << "PASS native format " << name << std::endl;
        }
    }
    void verify_browser() {
        reading = false; scene = 1;
        begin_web("https://www.iana.org/help/example-domains", false); paint();
        auto await_page = [&] {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(35);
            while (request.descriptor() >= 0) {
                if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Browser smoke deadline");
                pollfd channel{request.descriptor(), POLLIN, 0};
                if (poll(&channel, 1, 1000) > 0) browser_ready();
            }
            if (!web_message.empty()) throw std::runtime_error("Browser request failed: " + web_message);
        };
        await_page();
        if (website.text.find("Example Domains") == std::string::npos || website.links.empty())
            throw std::runtime_error("Browser smoke failed: " + web_message);
        web_mode = 1; auto original = address; address.clear(); paint();
        browser_tap(20, 930);
        if (address != "q") throw std::runtime_error("Address keyboard mapping failed");
        address = original; web_mode = 2; paint();
        browser_tap(100, 380); await_page();
        if (website.url == original) throw std::runtime_error("Link did not navigate");
        browser_tap(900, 1270); await_page();
        if (website.url != original) throw std::runtime_error("Site history did not return");
        web_mode = 0; paint();
        std::cout << "PASS native HTTPS page/link navigation/site history/address keyboard" << std::endl;
    }
    void paint() {
        auto started = std::chrono::steady_clock::now();
        draw({"-q", "-b", "-k"});
        text(25, 100, "Kindle Lite OS", 32, 40, 800);
        text(25, 100, "Amazon", 32, 870, 1040);
        text(130, 220, "Библиотека", 34, 40, 350);
        text(130, 220, "Браузер", 34, 397, 707);
        text(130, 220, "Настройки", 34, 754, 1032);
        if (reading) {
            text(245, 315, current, 30);
            text(330, 1250, page.text, settings.font + 10);
            text(1310, 1430, "Назад", 32, 40, 350);
            text(1310, 1430, "Закладка", 32, 397, 707);
            text(1310, 1430, "Далее", 32, 754, 1032);
        } else if (scene == 0) {
            if (books.books.empty()) text(300, 470, "Библиотека пуста. Добавьте книги в kindle-lite/books.");
            for (std::size_t i = 0; i < books.books.size(); ++i)
                text(300 + static_cast<int>(i) * 108, 390 + static_cast<int>(i) * 108, books.books[i].title + " [" + books.books[i].format + "]");
            text(1190, 1290, message, 28);
            text(1320, 1440, "Назад", 30, 40, 350);
            text(1320, 1440, "Обновить", 30, 397, 707);
            text(1320, 1440, "Далее", 30, 754, 1032);
        } else if (scene == 1) {
            paint_browser();
        } else {
            text(300, 360, "Размер шрифта: " + std::to_string(settings.font));
            text(390, 480, "Уменьшить", 36, 40, 500);
            text(390, 480, "Увеличить", 36, 576, 1032);
            text(560, 830, "Wi-Fi и подсветка управляются штатными службами.\n\nПроверка сна ещё предстоит.");
            text(900, 980, fs::exists("autostart.enabled") ? "Автозапуск: включён" : "Автозапуск: выключен", 32);
            text(1010, 1080, "Открыть последнюю книгу");
            text(1110, 1190, "Перейти к сохранённой закладке");
        }
        draw({"-q", "-s", "-w"});
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        std::cout << "PAINT scene=" << scene << " reader=" << reading << " elapsed_ms=" << milliseconds << std::endl;
    }
    void open(const std::string& path) {
        auto relative = fs::weakly_canonical(fs::u8path("books") / fs::u8path(path)).lexically_relative(fs::weakly_canonical("books"));
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") throw std::runtime_error("Book outside library");
        fs::path prepared;
        try { prepared = prepare_document(fs::u8path("books") / fs::u8path(path), "runtime/imported"); }
        catch (const std::exception& e) { message = std::string("Не удалось открыть: ") + e.what(); return; }
        save(); current = path; reading_file = prepared; progress = store.load(path); history.clear(); load_page();
        reading = true; store.set_last_book(path);
    }
    void tap(int x, int y) {
        std::cout << "TAP x=" << x << " y=" << y << std::endl;
        if (y < 110 && x > 850) { stopped = 1; return; }
        if (y >= 110 && y < 240) { save(); reading = false; scene = std::min(2, x / 357); paint(); return; }
        if (reading && y > 1290) {
            if (x < 357 && !history.empty()) { progress.offset = history.back(); history.pop_back(); load_page(); }
            else if (x > 714 && !page.end) { if (history.size() == 256) history.erase(history.begin()); history.push_back(progress.offset); progress.offset = page.next; load_page(); }
            else if (x >= 357 && x <= 714) { progress.bookmark = progress.offset; progress.has_bookmark = true; }
            save(); paint(); return;
        }
        if (reading) return;
        if (scene == 1) { browser_tap(x, y); return; }
        if (scene == 0) {
            if (y >= 300 && y < 1164) {
                auto index = static_cast<std::size_t>((y - 300) / 108);
                if (index < books.books.size()) open(books.books[index].path);
            } else if (y > 1290) {
                if (x < 357 && !cursors.empty()) { cursor = cursors.back(); cursors.pop_back(); }
                else if (x > 714 && books.more && !books.books.empty()) { cursors.push_back(cursor); cursor = books.books.back().path; }
                else if (x >= 357 && x <= 714) { catalog.rebuild(); cursor.clear(); cursors.clear(); }
                books = catalog.page("", cursor);
            } else return;
        } else if (scene == 2) {
            if (y >= 320 && y < 500) { settings.font = x < 536 ? std::max(16u, settings.font - 2) : std::min(32u, settings.font + 2); save(); }
            else if (y >= 900 && y < 980) {
                if (fs::exists("autostart.enabled")) fs::remove("autostart.enabled");
                else { std::ofstream flag("autostart.enabled"); if (!flag) throw std::runtime_error("Cannot enable autostart"); }
            }
            else if (y >= 1010 && y < 1190) {
                auto last = store.last_book();
                if (!last.empty()) { open(last); if (y >= 1090 && progress.has_bookmark) { progress.offset = progress.bookmark; load_page(); } }
            } else return;
        } else return;
        paint();
    }
};

void capture() {
    int fd = ::open("/dev/fb0", O_RDONLY);
    if (fd < 0) throw std::runtime_error("Cannot capture framebuffer");
    fb_var_screeninfo variable{}; fb_fix_screeninfo fixed{};
    if (ioctl(fd, FBIOGET_VSCREENINFO, &variable) || ioctl(fd, FBIOGET_FSCREENINFO, &fixed) || variable.bits_per_pixel != 8) {
        close(fd); throw std::runtime_error("Unsupported capture framebuffer");
    }
    std::ofstream out("runtime/screen.pgm", std::ios::binary);
    out << "P5\n" << variable.xres << ' ' << variable.yres << "\n255\n";
    std::vector<char> row(variable.xres);
    for (unsigned y = 0; y < variable.yres; ++y) {
        auto offset = static_cast<off_t>(y + variable.yoffset) * fixed.line_length + variable.xoffset;
        if (pread(fd, row.data(), row.size(), offset) != static_cast<ssize_t>(row.size())) { close(fd); throw std::runtime_error("Framebuffer read failed"); }
        out.write(row.data(), row.size());
    }
    close(fd);
    if (!out) throw std::runtime_error("Capture write failed");
}
}

int main(int argc, char** argv) {
    if (argc == 4 && std::string(argv[1]) == "--fetch") return fetch_web_worker(argv[2], argv[3]);
    try {
        sigset_t signals;
        sigemptyset(&signals);
        for (int number : {SIGTERM, SIGINT, SIGHUP, SIGCHLD}) sigaddset(&signals, number);
        if (sigprocmask(SIG_BLOCK, &signals, nullptr)) throw std::runtime_error("Cannot configure session signals");
        signal_descriptor = signalfd(-1, &signals, SFD_NONBLOCK | SFD_CLOEXEC);
        if (signal_descriptor < 0) throw std::runtime_error("Cannot create signal event source");
        fs::create_directories("runtime");
        int lock = ::open("/tmp/kindle-lite-ui.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) throw std::runtime_error("Another Kindle Lite UI is running");
        int touch = ::open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (touch < 0) throw std::runtime_error("Touch device unavailable");
        char name[128]{}; ioctl(touch, EVIOCGNAME(sizeof(name)), name);
        input_absinfo ax{}, ay{};
        if (std::string(name) != "goodix-ts" || ioctl(touch, EVIOCGABS(ABS_MT_POSITION_X), &ax) || ioctl(touch, EVIOCGABS(ABS_MT_POSITION_Y), &ay) || ax.maximum <= ax.minimum || ay.maximum <= ay.minimum)
            throw std::runtime_error("Unverified touchscreen capabilities");
        input_absinfo slots{};
        bool slotted = ioctl(touch, EVIOCGABS(ABS_MT_SLOT), &slots) == 0;
        std::cout << "TOUCH " << name << " x=" << ax.minimum << ':' << ax.maximum << " y=" << ay.minimum << ':' << ay.maximum << " slots=" << slotted << std::endl;
        if (argc > 1 && std::string(argv[1]) == "--probe") return 0;
        if (ioctl(touch, EVIOCGRAB, 1)) throw std::runtime_error("Cannot acquire touch input");
        Shell shell;
        PowerEvents power;
        shell.paint();
        if (argc > 1 && std::string(argv[1]) == "--formats-smoke") { shell.verify_formats(); capture(); shell.save(); return 0; }
        if (argc > 1 && std::string(argv[1]) == "--browser-smoke") { shell.verify_browser(); capture(); shell.save(); return 0; }
        if (argc > 1 && std::string(argv[1]) == "--smoke") {
            auto original_settings = StateStore("runtime/state").settings();
            shell.tap(500, 170); shell.tap(900, 170); shell.tap(900, 400); shell.tap(100, 170); shell.tap(100, 340);
            shell.verify_reader_navigation();
            capture(); shell.save(); StateStore("runtime/state").save_settings(original_settings); return 0;
        }
        bool timed = argc > 1 && std::string(argv[1]) == "--test-session";
        auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
        int slot = slotted ? slots.value : 0, primary_slot = -1, x = 0, y = 0;
        unsigned traced_events = 0;
        bool release = false, dropped = false, frame_contact = false, touching = false;
        bool sleeping = false;
        std::string power_buffer;
        auto last_activity = std::chrono::steady_clock::time_point{};
        while (!stopped) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (timed && remaining <= 0) break;
            pollfd descriptors[] = {{touch, POLLIN, 0}, {signal_descriptor, POLLIN, 0}, {shell.browser_descriptor(), POLLIN, 0}, {power.descriptor, POLLIN, 0}};
            int result = poll(descriptors, 4, timed ? static_cast<int>(remaining) : -1);
            if (result < 0) { if (errno == EINTR) continue; throw std::runtime_error("Input wait failed"); }
            if (!result) break;
            if (descriptors[1].revents & POLLIN) consume_signals();
            if (stopped) break;
            if (descriptors[3].revents & (POLLIN | POLLHUP)) {
                char bytes[512]; ssize_t count;
                while ((count = read(power.descriptor, bytes, sizeof(bytes))) > 0) power_buffer.append(bytes, static_cast<std::size_t>(count));
                std::size_t newline;
                while ((newline = power_buffer.find('\n')) != std::string::npos) {
                    auto event = power_buffer.substr(0, newline); power_buffer.erase(0, newline + 1);
                    std::cout << "POWER " << event << std::endl;
                    if (event.find("goingToScreenSaver") != std::string::npos) { sleeping = true; shell.sleep(); }
                    if (event.find("outOfScreenSaver") != std::string::npos) { sleeping = false; shell.paint(); }
                }
                if (count == 0) throw std::runtime_error("Power observer stopped");
            }
            if (!sleeping && descriptors[2].revents & (POLLIN | POLLHUP)) shell.browser_ready();
            if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) throw std::runtime_error("Touch disconnected");
            input_event event{};
            while (!stopped && read(touch, &event, sizeof(event)) == sizeof(event)) {
                if (traced_events < 128) {
                    std::cout << "INPUT type=" << event.type << " code=" << event.code << " value=" << event.value << std::endl;
                    ++traced_events;
                }
                if (event.type == EV_SYN && event.code == SYN_DROPPED) { dropped = true; release = false; touching = false; frame_contact = false; }
                if (dropped) { if (event.type == EV_SYN && event.code == SYN_REPORT) dropped = false; continue; }
                if (event.type == EV_ABS && event.code == ABS_MT_SLOT) slot = event.value;
                if (slotted && event.type == EV_ABS && event.code == ABS_MT_TRACKING_ID && event.value >= 0 && primary_slot < 0)
                    primary_slot = slot;
                if ((!slotted || slot == primary_slot) && event.type == EV_ABS) {
                    if (event.code == ABS_MT_POSITION_X) { x = event.value; frame_contact = true; }
                    if (event.code == ABS_MT_POSITION_Y) { y = event.value; frame_contact = true; }
                    if (event.code == ABS_MT_TRACKING_ID && event.value == -1) release = true;
                }
                if (event.type == EV_SYN && event.code == SYN_REPORT && !slotted) {
                    release = touching && !frame_contact;
                    touching = frame_contact;
                    frame_contact = false;
                }
                if (event.type == EV_SYN && event.code == SYN_REPORT && release) {
                    release = false;
                    primary_slot = -1;
                    if (!sleeping) {
                        auto now = std::chrono::steady_clock::now();
                        if (now - last_activity > std::chrono::seconds(30)) {
                            draw({"-i", "com.lab126.powerd", "touchScreenSaverTimeout", "1"}, "/usr/bin/lipc-set-prop"); last_activity = now;
                        }
                        shell.tap(std::clamp((x - ax.minimum) * 1072 / (ax.maximum - ax.minimum), 0, 1071), std::clamp((y - ay.minimum) * 1448 / (ay.maximum - ay.minimum), 0, 1447));
                    }
                }
            }
        }
        shell.save(); close(touch); close(lock);
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
}
