#include "kindle/device_status.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using namespace kindle;
    const auto root = fs::temp_directory_path() / ("kindle-status-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto write = [&](const char* path, const char* content) {
        const auto file = root / path;
        fs::create_directories(file.parent_path());
        std::ofstream(file) << content;
    };
    auto check = [](bool condition, const char* message) { if (!condition) throw std::runtime_error(message); };
    try {
        check(read_device_status(root) == DeviceStatus{}, "Missing device data must remain unknown");
        write("sys/class/power_supply/bd71827_bat/capacity", "80\n");
        write("sys/class/power_supply/bd71827_bat/status", "Charging\n");
        write("sys/class/net/wlan0/flags", "0x1003\n");
        write("sys/class/net/wlan0/carrier", "1\n");
        write("proc/net/wireless", "Inter-| quality\n wlan0: 0000 55. -55. -256 0 0\n");
        auto status = read_device_status(root);
        check(status.battery_percent == 80 && status.charging, "Actual capacity and charging state");
        check(status.wifi == WifiState::connected && status.signal_bars == 4, "Associated Wi-Fi and RSSI");
        write("proc/net/wireless", " wlan0: 0000 20. -80. -256 0 0\n");
        check(read_device_status(root).signal_bars == 1, "Weak signal");
        write("sys/class/net/wlan0/carrier", "0\n");
        status = read_device_status(root);
        check(status.wifi == WifiState::disconnected && status.signal_bars == 0, "Unassociated Wi-Fi");
        write("sys/class/net/wlan0/flags", "0x1002\n");
        check(read_device_status(root).wifi == WifiState::off, "Administratively disabled Wi-Fi");
        write("sys/class/power_supply/bd71827_bat/capacity", "150\n");
        check(!read_device_status(root).battery_percent, "Out-of-range capacity");
        write("sys/class/power_supply/bd71827_bat/capacity", "unavailable\n");
        check(!read_device_status(root).battery_percent, "Unreadable capacity");
        write("sys/class/power_supply/bd71827_bat/capacity", "0\n");
        write("sys/class/power_supply/bd71827_bat/status", "Discharging\n");
        status = read_device_status(root);
        check(status.battery_percent == 0 && !status.charging, "Zero capacity is valid");
        fs::remove_all(root / "sys/class/net/wlan0");
        check(read_device_status(root).wifi == WifiState::off, "PW4 removes wlan0 while Wi-Fi is off");
        fs::remove_all(root);
        std::cout << "PASS 10 device status checks\n";
        return 0;
    } catch (const std::exception& error) {
        fs::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
