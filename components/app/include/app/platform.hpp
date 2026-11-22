#pragma once
// Panel services that differ between the board and the PC: Wi-Fi, finding the server, receiving
// the token from a phone, backlight, system info. The board's implementation arrives with the
// board package (Phase 2b); the simulator uses a fake. Callbacks must run on the core task.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace app {

struct WifiNetwork {
    std::string ssid;
    int rssi = 0;  // dBm
    bool secure = true;
};

enum class WifiResult : uint8_t { Ok, WrongPassword, NotFound, Failed };

struct WifiStatus {
    bool connected = false;
    std::string ssid, ip;
    int rssi = 0;
};

struct ServerCandidate {
    std::string name, address;  // address: "host:port"
};

struct SystemInfo {
    std::string firmware;
    uint32_t uptime_s = 0;
    uint32_t free_internal_kb = 0, free_psram_kb = 0;
};

class Platform {
public:
    virtual ~Platform() = default;
    virtual void wifi_scan(std::function<void(std::vector<WifiNetwork>)> done) = 0;
    virtual void wifi_join(const std::string& ssid, const std::string& password, std::function<void(WifiResult)> done) = 0;
    virtual WifiStatus wifi_status() = 0;
    // Music Assistant servers on the local network (mDNS on the board).
    virtual void discover_servers(std::function<void(std::vector<ServerCandidate>)> done) = 0;
    // Serves a small page where a phone can paste the token. Returns the URL to show (and QR).
    virtual std::string pairing_start(std::function<void(std::string token)> on_token) = 0;
    virtual void pairing_stop() = 0;
    virtual void set_brightness(int percent) = 0;
    virtual SystemInfo system_info() = 0;
    virtual int local_minutes() = 0;  // minutes since local midnight, for the night schedule
    virtual void restart() = 0;
};

}  // namespace app
