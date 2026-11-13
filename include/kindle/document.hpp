#pragma once
#include "kindle/core.hpp"
namespace kindle {
// Text-only import. EPUB spine order and FB2 bodies are retained.
// Images, CSS, DRM and embedded scripts are not rendered.
fs::path prepare_document(const fs::path& source, const fs::path& cache);
}
