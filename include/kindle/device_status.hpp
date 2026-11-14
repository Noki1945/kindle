#pragma once
#include <filesystem>
#include <optional>

namespace kindle {
enum class WifiState { unknown, off, disconnected, connected };
struct DeviceStatus {
    std::optional<int> battery_percent;
    bool charging = false;
    WifiState wifi = WifiState::unknown;
    unsigned signal_bars = 0;
    bool operator==(const DeviceStatus& other) const {
        return battery_percent == other.battery_percent && charging == other.charging
            && wifi == other.wifi && signal_bars == other.signal_bars;
    }
};
DeviceStatus read_device_status(const std::filesystem::path& root = "/");
}
