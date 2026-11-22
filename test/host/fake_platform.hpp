#pragma once
// PC stand-in for the board's Platform. Wi-Fi: the right password is "music" (anything else →
// wrong password). Discovery finds `server_address`. Pairing plays the phone: it delivers
// `token` 3 s after the sign-in step opens (empty token = never, so you type it).
#include <ctime>
#include <deque>
#include <functional>

#include "app/platform.hpp"

class FakePlatform : public app::Platform {
public:
    std::string server_address = "mock";
    std::string token = "mock-token";
    int brightness = -1;

    void tick(uint32_t now) {
        now_ = now;
        while (!jobs_.empty() && static_cast<int32_t>(now - jobs_.front().first) >= 0) {
            auto fn = std::move(jobs_.front().second);
            jobs_.pop_front();
            fn();
        }
    }

    void wifi_scan(std::function<void(std::vector<app::WifiNetwork>)> done) override {
        later(800, [done] { done({{"HomeNetwork", -48, true}, {"HomeNetwork-5G", -61, true}, {"Neighbours", -78, true}, {"Guest", -70, false}}); });
    }
    void wifi_join(const std::string& ssid, const std::string& pw, std::function<void(app::WifiResult)> done) override {
        later(900, [this, ssid, pw, done] {
            bool ok = pw == "music" || ssid == "Guest";
            if (ok) wifi_ = {true, ssid, "192.168.1.42", -48};
            done(ok ? app::WifiResult::Ok : app::WifiResult::WrongPassword);
        });
    }
    app::WifiStatus wifi_status() override { return wifi_; }
    void discover_servers(std::function<void(std::vector<app::ServerCandidate>)> done) override {
        std::string a = server_address;
        later(1500, [a, done] { done({{"Music Assistant", a}}); });
    }
    std::string pairing_start(std::function<void(std::string)> on_token) override {
        if (!token.empty()) {
            std::string t = token;
            later(3000, [t, on_token] { on_token(t); });
        }
        return "http://192.168.1.42";
    }
    void pairing_stop() override {}
    void set_brightness(int percent) override { brightness = percent; }
    app::SystemInfo system_info() override { return {"sim", now_ / 1000, 300, 28000}; }
    int local_minutes() override {
        std::time_t t = std::time(nullptr);
        std::tm* lt = std::localtime(&t);
        return lt->tm_hour * 60 + lt->tm_min;
    }
    void restart() override { restarted = true; }
    bool restarted = false;

private:
    void later(uint32_t ms, std::function<void()> fn) { jobs_.emplace_back(now_ + ms, std::move(fn)); }
    std::deque<std::pair<uint32_t, std::function<void()>>> jobs_;
    uint32_t now_ = 0;
    app::WifiStatus wifi_{true, "HomeNetwork", "192.168.1.42", -48};
};
