// Settings sub-pages: Screen & night mode, Wi-Fi, Music Assistant, Language, About & diagnostics.
#include <algorithm>

#include "../shell.hpp"

namespace ui {
namespace {

std::string language_label(i18n::Lang lang) {
    i18n::Lang current = i18n::language();
    i18n::set_language(lang);
    std::string name = tr(i18n::Str::language_name_self);
    i18n::set_language(current);
    return name;
}

std::string uptime(uint32_t s) {
    uint32_t d = s / 86400, h = s / 3600 % 24, m = s / 60 % 60;
    if (d) return std::to_string(d) + " d " + std::to_string(h) + " h";
    if (h) return std::to_string(h) + " h " + std::to_string(m) + " min";
    return std::to_string(m) + " min";
}

class SettingsPageScreen : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_pad_hor(root, theme::kPad, 0);
        lv_obj_set_style_pad_top(root, 20, 0);
        lv_obj_set_style_pad_row(root, 16, 0);
    }

    void refresh(Topic t) override {
        if (t == Topic::Progress || t == Topic::Search || t == Topic::Browse || t == Topic::Queue) return;
        // Don't rebuild under a finger dragging a slider.
        if (t == Topic::Panel && dragging_) return;
        lv_obj_clean(root_);
        kit::back_chip(root_, tr(i18n::Str::nav_settings), [] { back(); });
        lv_obj_t* body = kit::scroll_column(root_, 24);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_flex_grow(body, 1);
        lv_obj_set_style_pad_bottom(body, 40, 0);  // last item clear of the player bar edge
        switch (settings_page()) {
            case SettingsPage::Display: display(body); break;
            case SettingsPage::Wifi: wifi(body); break;
            case SettingsPage::Server: server(body); break;
            case SettingsPage::Language: language(body); break;
            case SettingsPage::About: about(body); break;
            case SettingsPage::Appearance: appearance(body); break;
        }
    }

    ScreenId tab() const override { return ScreenId::Settings; }
    std::string title() const override { return tr(i18n::Str::nav_settings); }

private:
    static void title(lv_obj_t* parent, i18n::Str s) { kit::text(parent, tr(s), theme::h1(), theme::text()); }

    static lv_obj_t* two_columns(lv_obj_t* parent) {
        lv_obj_t* cols = kit::row(parent, 24);
        lv_obj_set_width(cols, lv_pct(100));
        lv_obj_set_flex_align(cols, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        return cols;
    }

    static lv_obj_t* half(lv_obj_t* cols, const std::string& t) {
        lv_obj_t* c = card(cols, t);
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_width(c, 1);
        return c;
    }

    static lv_obj_t* toggle_row(lv_obj_t* parent, const std::string& t, const std::string& sub, bool on,
                                std::function<void(bool)> changed) {
        lv_obj_t* r = kit::row(parent, 16);
        lv_obj_set_size(r, lv_pct(100), 72);
        lv_obj_t* col = kit::column(r, 4);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        kit::text(col, t, theme::body(), theme::text());
        if (!sub.empty()) kit::text(col, sub, theme::meta(), theme::text2());
        kit::toggle(r, on, std::move(changed));
        return r;
    }

    lv_obj_t* level(lv_obj_t* parent, int value, std::function<void(int)> on_release) {
        lv_obj_t* r = kit::row(parent, 16);
        lv_obj_set_size(r, lv_pct(100), 56);
        kit::icon(r, icon::sun, 22, theme::text2());
        lv_obj_t* s = kit::slider(r, value, [this, on_release](int v) {
            dragging_ = false;
            on_release(v);
        });
        lv_obj_add_event_cb(s, [](lv_event_t* e) { static_cast<SettingsPageScreen*>(lv_event_get_user_data(e))->dragging_ = true; },
                            LV_EVENT_PRESSED, this);
        lv_obj_set_flex_grow(s, 1);
        kit::icon(r, icon::sun, 32, theme::text2());
        return r;
    }

    // Minutes stepper for the night schedule (−/+ 15 min).
    static lv_obj_t* time_stepper(lv_obj_t* parent, const std::string& label, int minutes, std::function<void(int)> set) {
        lv_obj_t* col = kit::column(parent, 8);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        kit::overline(col, label);
        lv_obj_t* r = kit::row(col, 8);
        lv_obj_set_width(r, lv_pct(100));
        lv_obj_set_style_pad_all(r, 4, 0);
        lv_obj_set_style_radius(r, theme::r_control(), 0);
        lv_obj_set_style_bg_color(r, theme::elevated(), 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        kit::icon_button(r, icon::minus, [minutes, set] { set((minutes + 24 * 60 - 15) % (24 * 60)); });
        lv_obj_t* t = kit::text(r, clock_text(minutes), theme::h2(), theme::text());
        lv_obj_set_flex_grow(t, 1);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
        kit::icon_button(r, icon::plus, [minutes, set] { set((minutes + 15) % (24 * 60)); });
        return col;
    }

    void display(lv_obj_t* body) {
        title(body, i18n::Str::page_display);
        const app::PanelView* pv = state().panel.get();
        if (!pv) return;
        app::DisplaySettings d = pv->display;
        auto set = [](app::DisplaySettings x) { send(app::cmd::SetDisplay{x}); };
        lv_obj_t* cols = two_columns(body);

        lv_obj_t* day = half(cols, tr(i18n::Str::display_day));
        kit::overline(day, tr(i18n::Str::display_brightness));
        level(day, d.brightness, [d, set](int v) { auto x = d; x.brightness = std::max(v, 5); set(x); });
        kit::overline(day, tr(i18n::Str::display_dim_after));
        lv_obj_t* seg = kit::row(day, 8);
        for (int s : {30, 120, 300, 0}) {
            std::string label = s == 0 ? std::string(tr(i18n::Str::dim_never)) : s < 60 ? std::to_string(s) + " s" : std::to_string(s / 60) + " min";
            kit::tab(seg, label, d.dim_after_s == s, [d, s, set] { auto x = d; x.dim_after_s = s; set(x); });
        }
        toggle_row(day, tr(i18n::Str::display_clock), tr(i18n::Str::display_clock_sub), d.clock_when_dimmed,
                   [d, set](bool on) { auto x = d; x.clock_when_dimmed = on; set(x); });
        kit::button(day, kit::Button::Ghost, icon::moon, tr(i18n::Str::display_preview), [] { show_ambient(); });

        lv_obj_t* night = half(cols, tr(i18n::Str::display_night));
        toggle_row(night, tr(i18n::Str::display_schedule), tr(i18n::Str::display_schedule_sub), d.night,
                   [d, set](bool on) { auto x = d; x.night = on; set(x); });
        lv_obj_t* times = kit::row(night, 16);
        lv_obj_set_width(times, lv_pct(100));
        time_stepper(times, tr(i18n::Str::display_from), d.night_from, [d, set](int m) { auto x = d; x.night_from = m; set(x); });
        time_stepper(times, tr(i18n::Str::display_until), d.night_to, [d, set](int m) { auto x = d; x.night_to = m; set(x); });
        kit::overline(night, tr(i18n::Str::display_night_brightness));
        level(night, d.night_brightness, [d, set](int v) { auto x = d; x.night_brightness = std::max(v, 1); set(x); });
        if (pv->night_now) kit::text(night, tr(i18n::Str::display_night_now), theme::meta(), theme::accent());
    }

    void wifi(lv_obj_t* body) {
        title(body, i18n::Str::page_wifi);
        const app::PanelView* pv = state().panel.get();
        lv_obj_t* cols = two_columns(body);
        lv_obj_t* now = half(cols, pv && pv->wifi.connected ? pv->wifi.ssid : tr(i18n::Str::wifi_not_connected));
        if (pv && pv->wifi.connected) {
            int rssi = pv->wifi.rssi;
            i18n::Str strength = rssi > -60 ? i18n::Str::wifi_strong : rssi > -72 ? i18n::Str::wifi_ok : i18n::Str::wifi_weak;
            kit::text(now, std::string(tr(i18n::Str::home_connected)) + " · " + tr(strength), theme::body(), theme::accent());
            kit::overline(now, tr(i18n::Str::wifi_address));
            kit::text(now, pv->wifi.ip, theme::body(), theme::text());
            kit::overline(now, tr(i18n::Str::wifi_signal));
            kit::text(now, std::to_string(rssi) + " dBm", theme::body(), theme::text());
        }
        kit::button(now, kit::Button::Secondary, icon::wifi, tr(i18n::Str::wifi_change), [] { send(app::cmd::SetupGo{app::SetupStep::Wifi}); });
        lv_obj_t* info = half(cols, tr(i18n::Str::wifi_drops));
        kit::text(info, tr(i18n::Str::wifi_drops_body), theme::body(), theme::text2(), true);
    }

    void server(lv_obj_t* body) {
        title(body, i18n::Str::settings_server);
        const app::PanelView* pv = state().panel.get();
        const auto& c = state().connection;
        lv_obj_t* cols = two_columns(body);
        lv_obj_t* conn = half(cols, tr(state().connected() ? i18n::Str::home_connected : i18n::Str::home_reconnecting));
        kit::overline(conn, tr(i18n::Str::server_server));
        kit::text(conn, pv ? pv->server_address : "", theme::body(), theme::text());
        kit::overline(conn, tr(i18n::Str::server_version));
        kit::text(conn, c.server.server_version.empty() ? "—" : c.server.server_version, theme::body(), theme::text());
        kit::overline(conn, tr(i18n::Str::server_signed_in));
        kit::text(conn, pv && !pv->token_hint.empty() ? i18n::tr(i18n::Str::server_token_ending, pv->token_hint) : "—",
                  theme::body(), theme::text());
        lv_obj_t* buttons = kit::row(conn, 12);
        kit::button(buttons, kit::Button::Secondary, nullptr, tr(i18n::Str::server_change), [] { send(app::cmd::SetupGo{app::SetupStep::Server}); });
        kit::button(buttons, kit::Button::Secondary, nullptr, tr(i18n::Str::server_sign_in_again), [] { send(app::cmd::SignInAgain{}); });

        lv_obj_t* right = kit::column(cols, 24);
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_width(right, 1);
        lv_obj_t* services = card(right, tr(i18n::Str::server_services));
        std::vector<std::string> streaming;
        if (pv) {
            for (const auto& p : pv->providers) {
                if (!p.streaming && p.domain != "builtin") continue;
                if (p.streaming && std::find(streaming.begin(), streaming.end(), p.domain) == streaming.end()) streaming.push_back(p.domain);
                lv_obj_t* r = kit::row(services, 12);
                lv_obj_set_size(r, lv_pct(100), 48);
                kit::badge(r, p.domain == "builtin" ? "library" : p.domain);
                lv_obj_t* n = kit::text(r, p.domain == "builtin" ? tr(i18n::Str::menu_your_library) : p.name, theme::body(), theme::text());
                lv_obj_set_flex_grow(n, 1);
                kit::text(r, tr(p.available ? i18n::Str::server_working : i18n::Str::server_not_responding), theme::meta(),
                          p.available ? theme::accent() : theme::warning());
            }
        }

        // Preferred source: library & favourites always first (core rule), then this order.
        lv_obj_t* order = card(right, tr(i18n::Str::server_order));
        kit::text(order, tr(i18n::Str::server_order_sub), theme::meta(), theme::text2(), true);
        std::vector<std::string> list = pv ? pv->source_order : std::vector<std::string>{};
        for (const auto& d : streaming)
            if (std::find(list.begin(), list.end(), d) == list.end()) list.push_back(d);
        lv_obj_t* lib = kit::row(order, 12);
        kit::text(lib, "1", theme::body(), theme::text3());
        kit::badge(lib, "library");
        kit::text(lib, tr(i18n::Str::server_library_first), theme::body(), theme::text());
        for (size_t i = 0; i < list.size(); ++i) {
            lv_obj_t* r = kit::row(order, 12);
            lv_obj_set_size(r, lv_pct(100), 56);
            kit::text(r, std::to_string(i + 2), theme::body(), theme::text3());
            kit::badge(r, list[i]);
            lv_obj_t* n = kit::text(r, kit::provider_name(list[i]), theme::body(), theme::text());
            lv_obj_set_flex_grow(n, 1);
            if (i > 0) {
                kit::icon_button(r, icon::chevron_up, [list, i] {
                    auto next = list;
                    std::swap(next[i], next[i - 1]);
                    send(app::cmd::SetSourceOrder{next});
                }, 56, 28, true);
            }
        }
    }

    void language(lv_obj_t* body) {
        title(body, i18n::Str::settings_language);
        lv_obj_t* c = card(body);
        lv_obj_set_width(c, 720);
        for (int i = 0; i < i18n::kLangCount; ++i) {
            auto lang = static_cast<i18n::Lang>(i);
            lv_obj_t* r = nav_row(c, nullptr, language_label(lang), "", [lang] { send(app::cmd::SetLanguage{lang}); }, false);
            if (lang == i18n::language()) kit::icon(r, icon::check, 28, theme::accent());
            else lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        }
        kit::text(c, tr(i18n::Str::language_hint), theme::meta(), theme::text3(), true);
    }

    // A selectable preview tile (theme / rounding): the selected one gets an accent ring.
    static lv_obj_t* option(lv_obj_t* parent, const std::string& label, bool selected, kit::Action action) {
        lv_obj_t* o = kit::column(parent, 10);
        lv_obj_set_style_pad_all(o, 10, 0);
        lv_obj_set_style_radius(o, theme::r_card(), 0);
        lv_obj_set_style_border_width(o, selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(o, selected ? theme::accent() : theme::border(), 0);
        lv_obj_set_style_bg_color(o, theme::surface(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_PRESSED);
        kit::press_scale(o);
        kit::on_click(o, std::move(action));
        lv_obj_t* l = kit::text(o, label, theme::meta(), selected ? theme::text() : theme::text2());
        lv_obj_move_to_index(l, -1);
        return o;
    }

    void appearance(lv_obj_t* body) {
        title(body, i18n::Str::page_appearance);
        const app::PanelView* pv = state().panel.get();
        app::Appearance a = pv ? pv->appearance : app::Appearance{};
        auto set = [](app::Appearance x) { send(app::cmd::SetAppearance{x}); };
        lv_obj_t* cols = two_columns(body);

        lv_obj_t* mode = half(cols, tr(i18n::Str::appearance_theme));
        lv_obj_t* modes = kit::row(mode, 16);
        for (auto m : {app::ThemeMode::Dark, app::ThemeMode::Light}) {
            bool dark = m == app::ThemeMode::Dark;
            lv_obj_t* o = option(modes, tr(dark ? i18n::Str::appearance_dark : i18n::Str::appearance_light), a.mode == m,
                                 [a, m, set] { auto x = a; x.mode = m; set(x); });
            // Mini preview of the theme: page, sidebar strip, a cover and two text lines.
            lv_obj_t* pv_box = lv_obj_create(o);
            lv_obj_remove_style_all(pv_box);
            lv_obj_set_size(pv_box, 168, 96);
            lv_obj_move_to_index(pv_box, 0);
            lv_obj_set_style_radius(pv_box, theme::r_control(), 0);
            lv_obj_set_style_clip_corner(pv_box, true, 0);
            lv_obj_set_style_bg_opa(pv_box, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(pv_box, lv_color_hex(dark ? 0x0A0A0A : 0xF6F6F7), 0);
            lv_obj_set_style_border_width(pv_box, 1, 0);
            lv_obj_set_style_border_color(pv_box, theme::hairline(), 0);
            auto block = [pv_box](int x, int y, int w, int h, uint32_t col, int r) {
                lv_obj_t* b = lv_obj_create(pv_box);
                lv_obj_remove_style_all(b);
                lv_obj_set_pos(b, x, y);
                lv_obj_set_size(b, w, h);
                lv_obj_set_style_radius(b, r, 0);
                lv_obj_set_style_bg_color(b, lv_color_hex(col), 0);
                lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
            };
            block(0, 0, 36, 96, dark ? 0x000000 : 0xFFFFFF, 0);
            block(48, 14, 40, 40, 0xC9A227, 3);
            block(98, 18, 56, 8, dark ? 0xFFFFFF : 0x111111, 2);
            block(98, 32, 40, 6, dark ? 0xA3A3A3 : 0x52525B, 2);
            block(48, 66, 106, 18, dark ? 0x161616 : 0xECECEE, 3);
        }

        lv_obj_t* accent = half(cols, tr(i18n::Str::appearance_accent));
        lv_obj_t* sw = kit::row(accent, 14);
        for (int i = 0; i < theme::accent_count(); ++i) {
            lv_obj_t* dot = lv_obj_create(sw);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 48, 48);
            lv_obj_set_ext_click_area(dot, 4);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(dot, theme::accent_swatch(i), 0);
            lv_obj_set_style_border_width(dot, i == 0 ? 1 : 0, 0);  // the neutral swatch needs an edge on its own background
            lv_obj_set_style_border_color(dot, theme::border(), 0);
            if (a.accent == i) {
                lv_obj_set_style_outline_width(dot, 3, 0);
                lv_obj_set_style_outline_pad(dot, 3, 0);
                lv_obj_set_style_outline_color(dot, theme::text(), 0);
            }
            kit::press_scale(dot);
            kit::on_click(dot, [a, i, set] { auto x = a; x.accent = static_cast<uint8_t>(i); set(x); });
        }
        kit::text(accent, tr(i18n::Str::appearance_accent_hint), theme::label(), theme::text3(), true);

        lv_obj_t* cols2 = two_columns(body);
        lv_obj_t* round = half(cols2, tr(i18n::Str::appearance_rounding));
        lv_obj_t* rounds = kit::row(round, 16);
        static const i18n::Str names[] = {i18n::Str::round_sharp, i18n::Str::round_normal, i18n::Str::round_round};
        static const int32_t sample[] = {3, 10, 22};
        for (int r = 0; r < 3; ++r) {
            auto rr = static_cast<app::Roundness>(r);
            lv_obj_t* o = option(rounds, tr(names[r]), a.roundness == rr, [a, rr, set] { auto x = a; x.roundness = rr; set(x); });
            lv_obj_t* shape = lv_obj_create(o);
            lv_obj_remove_style_all(shape);
            lv_obj_set_size(shape, 120, 72);
            lv_obj_move_to_index(shape, 0);
            lv_obj_set_style_radius(shape, sample[r], 0);
            lv_obj_set_style_bg_color(shape, theme::elevated(), 0);
            lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, 0);
            lv_obj_set_size(shape, 96, 64);
        }
        lv_obj_t* motion = half(cols2, tr(i18n::Str::appearance_motion));
        toggle_row(motion, tr(i18n::Str::appearance_reduce_motion), tr(i18n::Str::appearance_reduce_motion_sub), a.reduce_motion,
                   [a, set](bool on) { auto x = a; x.reduce_motion = on; set(x); });
    }

    void about(lv_obj_t* body) {
        title(body, i18n::Str::page_about);
        const app::PanelView* pv = state().panel.get();
        lv_obj_t* cols = two_columns(body);
        lv_obj_t* about = half(cols, tr(i18n::Str::about_title));
        if (pv) {
            kit::overline(about, tr(i18n::Str::about_software));
            kit::text(about, pv->system.firmware, theme::body(), theme::text());
            kit::overline(about, tr(i18n::Str::about_running));
            kit::text(about, uptime(pv->system.uptime_s), theme::body(), theme::text());
            kit::overline(about, tr(i18n::Str::about_memory));
            kit::text(about, i18n::tr(i18n::Str::about_memory_value, std::to_string(pv->system.free_internal_kb) + " KB · PSRAM " +
                                                                    std::to_string(pv->system.free_psram_kb / 1024) + " MB"),
                      theme::body(), theme::text());
        }
        lv_obj_t* buttons = kit::row(about, 12);
        kit::button(buttons, kit::Button::Secondary, icon::power, tr(i18n::Str::about_restart), [] {
            confirm(tr(i18n::Str::about_restart_q), tr(i18n::Str::about_restart_body), tr(i18n::Str::edit_delete_keep),
                    tr(i18n::Str::about_restart), [] { send(app::cmd::RestartPanel{}); });
        });
        kit::button(buttons, kit::Button::Ghost, icon::trash, tr(i18n::Str::about_reset), [] {
            confirm(tr(i18n::Str::about_reset_q), tr(i18n::Str::about_reset_body), tr(i18n::Str::edit_delete_keep),
                    tr(i18n::Str::about_reset), [] { send(app::cmd::ResetPanel{}); });
        });

        lv_obj_t* diag = half(cols, tr(i18n::Str::about_diagnostics));
        app::Diagnostics g = pv ? pv->diagnostics : app::Diagnostics{};
        toggle_row(diag, tr(i18n::Str::about_fps), tr(i18n::Str::about_fps_sub), g.fps,
                   [g](bool on) { auto x = g; x.fps = on; send(app::cmd::SetDiagnostics{x}); });
        toggle_row(diag, tr(i18n::Str::about_touches), tr(i18n::Str::about_touches_sub), g.touches,
                   [g](bool on) { auto x = g; x.touches = on; send(app::cmd::SetDiagnostics{x}); });
    }

    lv_obj_t* root_ = nullptr;
    bool dragging_ = false;
};

}  // namespace

std::unique_ptr<Screen> make_settings_page() { return std::make_unique<SettingsPageScreen>(); }

}  // namespace ui
