#pragma once
// The board's Platform: Wi-Fi through the ESP32-C6 (esp_wifi_remote), Music Assistant discovery
// over mDNS, the phone pairing page, the backlight and local time. Every callback is handed to
// `post` so it runs on the core task, as app::Platform requires.
#include <functional>
#include <memory>

#include "app/platform.hpp"

namespace board {

using Post = std::function<void(std::function<void()>)>;

// The backlight PWM exists only once the display has started. Until then brightness requests are
// remembered; backlight_ready() (called after bsp_display_start) applies the last one.
void backlight_ready();

class BoardPlatform : public app::Platform {
public:
    explicit BoardPlatform(Post post);
    ~BoardPlatform() override;

    void wifi_scan(std::function<void(std::vector<app::WifiNetwork>)> done) override;
    void wifi_join(const std::string& ssid, const std::string& password, std::function<void(app::WifiResult)> done) override;
    app::WifiStatus wifi_status() override;
    void discover_servers(std::function<void(std::vector<app::ServerCandidate>)> done) override;
    std::string pairing_start(const std::string& server, std::function<void(std::string token)> on_token) override;
    void pairing_stop() override;
    void set_brightness(int percent) override;
    app::SystemInfo system_info() override;
    int local_minutes() override;
    void restart() override;
    void update_check(std::function<void(std::string version, bool online)> done) override;
    void update_install(std::function<void(int percent)> progress, std::function<void(bool ok)> done) override;

    struct State;  // event-handler state (board.cpp)

private:
    std::unique_ptr<State> s_;
};

}  // namespace board
