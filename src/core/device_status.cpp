#include "kindle/device_status.hpp"
#include <fstream>
#include <sstream>
#include <string>

namespace kindle {
DeviceStatus read_device_status(const std::filesystem::path& root) {
    DeviceStatus result;
    const auto battery = root / "sys/class/power_supply/bd71827_bat";
    int capacity = -1;
    if (std::ifstream(battery / "capacity") >> capacity && capacity >= 0 && capacity <= 100)
        result.battery_percent = capacity;
    std::string state;
    std::ifstream(battery / "status") >> state;
    result.charging = state == "Charging";

    const auto wireless = root / "sys/class/net/wlan0";
    std::error_code error;
    const bool network_available = std::filesystem::is_directory(root / "sys/class/net", error);
    error.clear();
    if (network_available && !std::filesystem::exists(wireless, error) && !error) {
        result.wifi = WifiState::off;
        return result;
    }
    unsigned flags = 0;
    std::ifstream flag_file(wireless / "flags");
    const bool have_flags = static_cast<bool>(flag_file >> std::hex >> flags);
    int carrier = -1;
    std::ifstream(wireless / "carrier") >> carrier;
    if (have_flags && !(flags & 1u)) result.wifi = WifiState::off;
    else if (carrier == 1) result.wifi = WifiState::connected;
    else if (have_flags && carrier == 0) result.wifi = WifiState::disconnected;
    if (result.wifi != WifiState::connected) return result;

    std::ifstream wireless_file(root / "proc/net/wireless");
    std::string line;
    while (std::getline(wireless_file, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::istringstream interface_name(line.substr(0, colon));
        std::string name; interface_name >> name;
        if (name != "wlan0") continue;
        std::istringstream values(line.substr(colon + 1));
        std::string status;
        double quality = 0, level = 0;
        if (values >> status >> quality >> level && level >= -100 && level <= -10)
            result.signal_bars = level >= -55 ? 4 : level >= -65 ? 3 : level >= -75 ? 2 : 1;
        break;
    }
    return result;
}
}
