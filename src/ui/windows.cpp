#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include "kindle/core.hpp"
#include "kindle/browser.hpp"
#include "kindle/document.hpp"
#include <fstream>
#include <memory>
#include <thread>
#include <atomic>
#include <stdexcept>

using namespace kindle;
namespace {
constexpr UINT fetched = WM_APP + 1;
HWND window, search_box, list_box, article_box, url_box;
HFONT ui_font;
fs::path base = fs::current_path(), books = base / "books", runtime = base / "runtime";
fs::path reading_file;
std::unique_ptr<Catalog> catalog;
std::unique_ptr<StateStore> store;
Settings settings;
CatalogPage shown;
std::string cursor, query, current, article;
std::vector<std::string> cursors;
std::vector<std::uint64_t> history;
ReadingState progress;
TextPage text;
bool descending = false, reading = false;
int scene = 0;
std::thread network;
std::atomic<bool> busy{false};
std::wstring wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n && !s.empty()) return L"[Текст не в UTF-8]";
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n); return w;
}
std::string utf8(const std::wstring& s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string v(n, 0); WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), v.data(), n, nullptr, nullptr); return v;
}
std::wstring value(HWND h) { int n = GetWindowTextLengthW(h); std::wstring s(n + 1, 0); GetWindowTextW(h, s.data(), n + 1); s.resize(n); return s; }
void error(const std::exception& e) { MessageBoxW(window, wide(e.what()).c_str(), L"Kindle Lite OS", MB_OK | MB_ICONWARNING); }
void refresh_list() {
    shown = catalog->page(query, cursor, descending);
    SendMessageW(list_box, LB_RESETCONTENT, 0, 0);
    for (const auto& b : shown.books) {
        auto label = wide(b.title + "   [" + b.format + "]");
        SendMessageW(list_box, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
}
void save() {
    if (!current.empty()) store->save(current, progress);
    store->save_settings(settings);
}
void load_text() { text = read_page(reading_file, progress.offset, static_cast<unsigned>(520 / (settings.font * 0.61)), 480 / (settings.font + settings.line_spacing)); progress.offset = text.start; }
void open_book(const std::string& path) {
    auto candidate = fs::weakly_canonical(books / fs::u8path(path));
    auto relative = candidate.lexically_relative(fs::weakly_canonical(books));
    if (relative.empty() || *relative.begin() == ".." || relative.is_absolute()) throw std::runtime_error("Book path outside library");
    auto prepared = prepare_document(candidate, runtime / "imported");
    save(); current = path; reading_file = prepared; progress = store->load(current); history.clear(); load_text();
    store->set_last_book(current); reading = true; scene = 0;
}
void visibility() {
    ShowWindow(search_box, scene == 0 && !reading ? SW_SHOW : SW_HIDE);
    ShowWindow(list_box, scene == 0 && !reading ? SW_SHOW : SW_HIDE);
    ShowWindow(url_box, scene == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(article_box, scene == 1 ? SW_SHOW : SW_HIDE);
    InvalidateRect(window, nullptr, TRUE);
}
void label(HDC dc, int x, int y, const std::wstring& s) { TextOutW(dc, x, y, s.c_str(), static_cast<int>(s.size())); }
void button(HDC dc, int x, int y, int width, const std::wstring& s) { Rectangle(dc, x, y, x + width, y + 48); RECT r{x + 6, y, x + width - 6, y + 48}; DrawTextW(dc, s.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE); }
void paint(HDC dc) {
    SelectObject(dc, ui_font); SetBkMode(dc, TRANSPARENT);
    label(dc, 24, 18, L"KINDLE LITE / СИМУЛЯТОР");
    label(dc, 24, 48, L"Батарея: —   Wi-Fi: имитация   1072 × 1448 → масштаб");
    button(dc, 24, 86, 168, L"Библиотека"); button(dc, 204, 86, 168, L"Браузер"); button(dc, 384, 86, 168, L"Настройки");
    if (scene == 0 && !reading) {
        button(dc, 432, 150, 120, L"Поиск");
        button(dc, 24, 514, 168, L"← Назад"); button(dc, 204, 514, 168, L"Открыть"); button(dc, 384, 514, 168, L"Далее →");
        button(dc, 24, 580, 250, descending ? L"Сортировка: Я → А" : L"Сортировка: А → Я"); button(dc, 290, 580, 262, L"Обновить каталог");
        button(dc, 24, 646, 528, L"Продолжить последнюю книгу");
        label(dc, 24, 716, L"Название из имени файла; автор и обложка пока недоступны.");
    } else if (scene == 0) {
        label(dc, 24, 148, wide(current));
        HFONT font = CreateFontW(-static_cast<int>(settings.font), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, FIXED_PITCH, L"Consolas");
        SelectObject(dc, font);
        auto w = wide(text.text); std::size_t start = 0; int y = 192;
        while (start < w.size()) { auto end = w.find(L'\n', start); if (end == std::wstring::npos) end = w.size(); label(dc, 24, y, w.substr(start, end - start)); y += settings.font + settings.line_spacing; start = end + 1; }
        SelectObject(dc, ui_font); DeleteObject(font);
        button(dc, 24, 690, 168, L"← Назад"); button(dc, 204, 690, 168, L"Закладка"); button(dc, 384, 690, 168, L"Далее →");
        label(dc, 24, 750, L"TXT / UTF-8 · позиция " + std::to_wstring(progress.offset));
    } else if (scene == 1) {
        button(dc, 432, 150, 120, busy ? L"Загрузка…" : L"Открыть");
        label(dc, 24, 690, L"HTTPS · режим статьи · UTF-8 · до 2 МиБ");
        label(dc, 24, 722, L"Без JavaScript, CSS, изображений и переходов по ссылкам.");
        label(dc, 24, 754, L"Закройте окно после завершения сетевого запроса.");
    } else {
        label(dc, 24, 166, L"Параметры чтения сохраняются локально");
        button(dc, 24, 216, 250, L"Шрифт −"); button(dc, 290, 216, 262, L"Шрифт +");
        label(dc, 24, 278, L"Размер: " + std::to_wstring(settings.font));
        button(dc, 24, 318, 250, L"Интервал −"); button(dc, 290, 318, 262, L"Интервал +");
        label(dc, 24, 380, L"Интервал: " + std::to_wstring(settings.line_spacing));
        button(dc, 24, 422, 528, settings.wifi_demo ? L"Wi-Fi: включён (демо)" : L"Wi-Fi: выключен (демо)");
        button(dc, 24, 490, 528, L"Подсветка (демо): " + std::to_wstring(settings.light_demo));
        label(dc, 24, 566, L"Kindle PW4 / FW 5.16.7 — целевая конфигурация");
        label(dc, 24, 606, L"Сон, сеть, USB и питание Kindle не подключены.");
        label(dc, 24, 644, L"Приложение не меняет системные настройки компьютера.");
        button(dc, 24, 710, 528, L"Закрыть приложение");
    }
}
void click(int x, int y) {
    if (y >= 86 && y < 134) { save(); reading = false; scene = x < 198 ? 0 : (x < 378 ? 1 : 2); visibility(); return; }
    if (scene == 0 && !reading) {
        if (y >= 150 && y < 198 && x >= 432) { query = utf8(value(search_box)); cursor.clear(); cursors.clear(); refresh_list(); }
        if (y >= 514 && y < 562) {
            if (x < 198 && !cursors.empty()) { cursor = cursors.back(); cursors.pop_back(); refresh_list(); }
            else if (x >= 378 && shown.more && !shown.books.empty()) { cursors.push_back(cursor); cursor = shown.books.back().path; refresh_list(); }
            else if (x >= 198 && x < 378) { auto i = SendMessageW(list_box, LB_GETCURSEL, 0, 0); if (i != LB_ERR) open_book(shown.books.at(static_cast<std::size_t>(i)).path); }
        }
        if (y >= 580 && y < 628) { if (x < 282) descending = !descending; else catalog->rebuild(); cursor.clear(); cursors.clear(); refresh_list(); }
        if (y >= 646 && y < 694) { auto b = store->last_book(); if (!b.empty()) open_book(b); }
    } else if (scene == 0) {
        if (y >= 690 && y < 738) {
            if (x < 198 && !history.empty()) { progress.offset = history.back(); history.pop_back(); load_text(); }
            else if (x >= 378 && !text.end) { history.push_back(progress.offset); progress.offset = text.next; load_text(); }
            else if (x >= 198 && x < 378) { progress.bookmark = progress.offset; progress.has_bookmark = true; MessageBoxW(window, L"Закладка сохранена. Клавиша B — перейти к закладке.", L"Закладка", MB_OK); }
            save();
        }
    } else if (scene == 1) {
        if (y >= 150 && y < 198 && x >= 432 && !busy) {
            if (network.joinable()) network.join();
            auto url = value(url_box); busy = true;
            network = std::thread([url] { try { article = fetch_article(url); } catch (const std::exception& e) { article = std::string("Ошибка: ") + e.what(); } PostMessageW(window, fetched, 0, 0); });
        }
    } else {
        if (y >= 216 && y < 264) { if (x < 282 && settings.font > 16) settings.font -= 2; else if (x >= 282 && settings.font < 32) settings.font += 2; }
        if (y >= 318 && y < 366) { if (x < 282 && settings.line_spacing > 0) settings.line_spacing -= 2; else if (x >= 282 && settings.line_spacing < 16) settings.line_spacing += 2; }
        if (y >= 422 && y < 470) settings.wifi_demo = !settings.wifi_demo;
        if (y >= 490 && y < 538) settings.light_demo = (settings.light_demo + 1) % 25;
        if (y >= 710 && y < 758) SendMessageW(window, WM_CLOSE, 0, 0);
        store->save_settings(settings);
    }
    visibility();
}
LRESULT CALLBACK procedure(HWND h, UINT message, WPARAM w, LPARAM l) {
    try {
        switch (message) {
        case WM_CREATE:
            window = h;
            search_box = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 24, 152, 394, 42, h, nullptr, nullptr, nullptr);
            list_box = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED, 24, 216, 528, 276, h, reinterpret_cast<HMENU>(101), nullptr, nullptr);
            url_box = CreateWindowW(L"EDIT", L"https://example.com", WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 24, 152, 394, 42, h, nullptr, nullptr, nullptr);
            article_box = CreateWindowW(L"EDIT", L"Введите HTTPS-адрес и нажмите «Открыть».", WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 24, 216, 528, 450, h, nullptr, nullptr, nullptr);
            for (auto control : {search_box, list_box, url_box, article_box}) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
            SendMessageW(article_box, EM_SETLIMITTEXT, 2 * 1024 * 1024, 0);
            refresh_list(); visibility(); return 0;
        case WM_MEASUREITEM: reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight = 34; return TRUE;
        case WM_DRAWITEM: {
            auto d = reinterpret_cast<DRAWITEMSTRUCT*>(l); if (d->itemID == static_cast<UINT>(-1)) return TRUE;
            FillRect(d->hDC, &d->rcItem, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            SelectObject(d->hDC, ui_font); SetTextColor(d->hDC, RGB(0, 0, 0)); SetBkColor(d->hDC, RGB(255,255,255));
            wchar_t buffer[8192]{}; SendMessageW(list_box, LB_GETTEXT, d->itemID, reinterpret_cast<LPARAM>(buffer));
            RECT r = d->rcItem; r.left += 10; DrawTextW(d->hDC, buffer, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            if (d->itemState & ODS_SELECTED) DrawFocusRect(d->hDC, &d->rcItem); return TRUE;
        }
        case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); paint(dc); EndPaint(h, &ps); return 0; }
        case WM_LBUTTONUP: click(GET_X_LPARAM(l), GET_Y_LPARAM(l)); return 0;
        case WM_COMMAND: if (LOWORD(w) == 101 && HIWORD(w) == LBN_DBLCLK) { auto i = SendMessageW(list_box, LB_GETCURSEL, 0, 0); if (i != LB_ERR) { open_book(shown.books.at(static_cast<std::size_t>(i)).path); visibility(); } } return 0;
        case WM_KEYDOWN: if (w == 'B' && reading && progress.has_bookmark) { progress.offset = progress.bookmark; history.clear(); load_text(); save(); visibility(); } return 0;
        case fetched: if (network.joinable()) network.join(); busy = false; SetWindowTextW(article_box, wide(article).c_str()); article.clear(); visibility(); return 0;
        case WM_CLOSE: if (busy) { MessageBoxW(h, L"Дождитесь завершения HTTPS-запроса (тайм-аут ограничен).", L"Загрузка", MB_OK); return 0; } save(); DestroyWindow(h); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
    } catch (const std::exception& e) { error(e); return 0; }
    return DefWindowProcW(h, message, w, l);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    try {
        // GUI launchers may choose System32 as CWD. Resolve bundled books from the executable.
        wchar_t exe[32768]{};
        if (!GetModuleFileNameW(nullptr, exe, 32768)) throw std::runtime_error("Cannot locate executable");
        auto location = fs::path(exe).parent_path();
        base = location;
        for (unsigned depth = 0; depth < 4; ++depth) {
            if (fs::is_directory(location / "books")) { base = location; break; }
            location = location.parent_path();
        }
        books = base / "books"; runtime = base / "runtime";
        fs::create_directories(books); store = std::make_unique<StateStore>(runtime / "state"); settings = store->settings();
        catalog = std::make_unique<Catalog>(books, runtime / "catalog"); if (!fs::exists(runtime / "catalog")) catalog->rebuild();
        ui_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
        WNDCLASSW wc{}; wc.lpfnWndProc = procedure; wc.hInstance = instance; wc.lpszClassName = L"KindleLite"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)); RegisterClassW(&wc);
        RECT r{0, 0, 576, 808}; AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
        HWND h = CreateWindowW(wc.lpszClassName, L"Kindle Lite OS — Windows prototype", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, r.right-r.left, r.bottom-r.top, nullptr, nullptr, instance, nullptr);
        if (!h) throw std::runtime_error("Window creation failed");
        ShowWindow(h, show); MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        if (network.joinable()) network.join(); DeleteObject(ui_font); return 0;
    } catch (const std::exception& e) { error(e); return 1; }
}
