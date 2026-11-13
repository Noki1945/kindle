#pragma once
#include <string>
namespace kindle {
// Windows-only article fetcher. HTTPS only, bounded body, no scripts or subresources.
std::string fetch_article(const std::wstring& url);
std::string html_text(const std::string& html);
}
