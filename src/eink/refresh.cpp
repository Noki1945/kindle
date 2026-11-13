#include "kindle/core.hpp"
namespace kindle {
Refresh RefreshPolicy::request(Rect r, bool scene) {
    if (r.width <= 0 || r.height <= 0) return Refresh::none;
    if (scene || ++partials_ >= 6) { partials_ = 0; return Refresh::full; }
    return Refresh::partial;
}
}
