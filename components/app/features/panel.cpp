// This panel: screen & night mode (backlight), diagnostics, music services, preferred-source
// order, Wi-Fi/system info, restart and reset.
#include <algorithm>

#include "backend/ma_commands.hpp"
#include "features/features.hpp"

namespace app {
namespace {

constexpr uint32_t kRefreshMs = 30'000;
constexpr int kDimmedBrightness = 8;  // dimmed clock; the night brightness caps it further

bool in_window(int now, int from, int to) { return from <= to ? now >= from && now < to : now >= from || now < to; }

class Panel : public Feature {
public:
    explicit Panel(Context& ctx) : ctx_(ctx) { load(); }

    void on_connected() override { load_providers(); }

    void on_event(const backend::Event& ev) override {
        if (ev.name == "providers_updated") load_providers();
    }

    void tick() override {
        if (ctx_.now_ms - last_refresh_ < kRefreshMs && last_refresh_ != 0) return;
        last_refresh_ = ctx_.now_ms ? ctx_.now_ms : 1;
        apply_and_publish();
    }

    void on_command(const Command& c) override {
        auto& s = ctx_.settings;
        if (auto* d = std::get_if<cmd::SetDisplay>(&c)) {
            view_.display = d->display;
            const auto& x = view_.display;
            s.set("brightness", std::to_string(x.brightness));
            s.set("dim_after", std::to_string(x.dim_after_s));
            s.set("clock_dimmed", x.clock_when_dimmed ? "1" : "0");
            s.set("night", x.night ? "1" : "0");
            s.set("night_from", std::to_string(x.night_from));
            s.set("night_to", std::to_string(x.night_to));
            s.set("night_brightness", std::to_string(x.night_brightness));
            apply_and_publish();
        } else if (auto* d = std::get_if<cmd::SetDimmed>(&c)) {
            view_.dimmed = d->dimmed;
            apply_and_publish();
        } else if (auto* g = std::get_if<cmd::SetDiagnostics>(&c)) {
            view_.diagnostics = g->diagnostics;
            s.set("diag_fps", g->diagnostics.fps ? "1" : "0");
            s.set("diag_touches", g->diagnostics.touches ? "1" : "0");
            publish();
        } else if (auto* o = std::get_if<cmd::SetSourceOrder>(&c)) {
            view_.source_order = o->providers;
            ctx_.prefs.provider_order = o->providers;
            std::string joined;
            for (const auto& p : o->providers) joined += (joined.empty() ? "" : ",") + p;
            s.set("source_order", joined);
            publish();
        } else if (std::holds_alternative<cmd::Reconnect>(c)) {
            ctx_.backend.reconnect();
        } else if (std::holds_alternative<cmd::RestartPanel>(c)) {
            ctx_.platform.restart();
        } else if (std::holds_alternative<cmd::ResetPanel>(c)) {
            s.erase_all();
            ctx_.platform.restart();
        }
    }

private:
    int num(const char* key, int fallback) {
        std::string v = ctx_.settings.get(key);
        return v.empty() ? fallback : std::atoi(v.c_str());
    }

    void load() {
        DisplaySettings d;
        d.brightness = num("brightness", d.brightness);
        d.dim_after_s = num("dim_after", d.dim_after_s);
        d.clock_when_dimmed = num("clock_dimmed", 1) != 0;
        d.night = num("night", 1) != 0;
        d.night_from = num("night_from", d.night_from);
        d.night_to = num("night_to", d.night_to);
        d.night_brightness = num("night_brightness", d.night_brightness);
        view_.display = d;
        view_.diagnostics = {num("diag_fps", 0) != 0, num("diag_touches", 0) != 0};
        view_.source_order = ctx_.prefs.provider_order;  // App::start loaded these
    }

    void load_providers() {
        ctx_.backend.request(ma::cmd::music_providers(), [this](backend::Response&& r) {
            if (!r.ok) return;
            view_.providers.clear();
            const cJSON* p;
            cJSON_ArrayForEach(p, r.result.get()) {
                view_.providers.push_back({backend::json_str(p, "instance_id"), backend::json_str(p, "domain"),
                                           backend::json_str(p, "name"), backend::json_bool(p, "available", true),
                                           backend::json_bool(p, "is_streaming_provider")});
            }
            publish();
        });
    }

    void apply_and_publish() {
        const auto& d = view_.display;
        view_.night_now = d.night && in_window(ctx_.platform.local_minutes(), d.night_from, d.night_to);
        int level = view_.night_now ? d.night_brightness : d.brightness;
        if (view_.dimmed) level = std::min(level, kDimmedBrightness);
        ctx_.platform.set_brightness(level);
        view_.wifi = ctx_.platform.wifi_status();
        view_.system = ctx_.platform.system_info();
        std::string url = ctx_.settings.get("server_url");
        view_.server_address = url.size() > 8 ? url.substr(5, url.size() - 8) : url;  // ws://host:port/ws → host:port
        std::string token = ctx_.settings.get("token");
        view_.token_hint = token.size() > 4 ? token.substr(token.size() - 4) : "";
        publish();
    }

    void publish() { ctx_.publish(std::make_shared<const PanelView>(view_)); }

    Context& ctx_;
    PanelView view_;
    uint32_t last_refresh_ = 0;
};

}  // namespace

std::unique_ptr<Feature> make_panel(Context& ctx) { return std::make_unique<Panel>(ctx); }

}  // namespace app
