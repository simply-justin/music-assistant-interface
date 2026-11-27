// Settings: categories on the left, the chosen page on the right (one column of grouped rows).
// Every page uses the same parts: group() = title + card, rows separated by hairlines.
#include <algorithm>

#include "../shell.hpp"

namespace ui {
namespace {

std::string language_label(i18n::Lang lang) {
    i18n::Lang current = i18n::language();
    i18n::set_language(lang);
    std::string name = tr(i18n::Str::language_name_self);  // each language names itself
    i18n::set_language(current);
    return name;
}

std::string uptime(uint32_t s) {
    uint32_t d = s / 86400, h = s / 3600 % 24, m = s / 60 % 60;
    if (d) return std::to_string(d) + " d " + std::to_string(h) + " h";
    if (h) return std::to_string(h) + " h " + std::to_string(m) + " min";
    return std::to_string(m) + " min";
}

std::string dim_label(int s) {
    return s == 0 ? std::string(tr(i18n::Str::dim_never)) : s < 60 ? std::to_string(s) + " s" : std::to_string(s / 60) + " min";
}

// ---- Parts shared by every page (group(), line(), … live in shell.cpp: setup uses them too)

// Choice on the right of a row (dim after: 30 s / 2 min / …).
void choice_row(lv_obj_t* card, const std::string& label, const std::vector<std::pair<std::string, bool>>& options,
                std::function<void(size_t)> pick) {
    lv_obj_t* r = line(card, 64);
    grow_text(r, label);
    lv_obj_t* seg = kit::row(r, 4);
    for (size_t i = 0; i < options.size(); ++i) kit::tab(seg, options[i].first, options[i].second, [pick, i] { pick(i); });
}

// Time of day with −/+ 15 min.
void time_row(lv_obj_t* card, const std::string& label, int minutes, std::function<void(int)> set) {
    lv_obj_t* r = line(card, 64);
    grow_text(r, label);
    kit::icon_button(r, icon::minus, [minutes, set] { set((minutes + 24 * 60 - 15) % (24 * 60)); }, 48, 22, true);
    lv_obj_t* t = kit::text(r, clock_text(minutes), theme::body(), theme::text());
    lv_obj_set_width(t, 72);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    kit::icon_button(r, icon::plus, [minutes, set] { set((minutes + 15) % (24 * 60)); }, 48, 22, true);
}

// ---- The screen

struct Category { SettingsPage page; const char* glyph; i18n::Str title; };

class Settings : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(root, 0, 0);
        lv_obj_set_style_pad_column(root, 0, 0);
        lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    }

    void refresh(Topic t) override {
        if (t == Topic::Progress || t == Topic::Search || t == Topic::Browse || t == Topic::Queue || t == Topic::Playlists ||
            t == Topic::Sleep)
            return;
        if (t == Topic::Panel && dragging_) return;  // don't rebuild under a finger dragging a slider
        lv_obj_clean(root_);
        nav();
        lv_obj_t* pane = kit::scroll_column(root_, 28);
        lv_obj_set_flex_grow(pane, 1);
        lv_obj_set_height(pane, lv_pct(100));
        lv_obj_set_style_pad_hor(pane, 40, 0);
        lv_obj_set_style_pad_top(pane, 28, 0);
        lv_obj_set_style_pad_bottom(pane, 40, 0);
        switch (settings_page()) {
            case SettingsPage::Appearance: appearance(pane); break;
            case SettingsPage::Display: display(pane); break;
            case SettingsPage::Wifi: wifi(pane); break;
            case SettingsPage::Server: server(pane); break;
            case SettingsPage::Language: language(pane); break;
            case SettingsPage::About: about(pane); break;
        }
    }

    ScreenId tab() const override { return ScreenId::Settings; }
    std::string title() const override { return tr(i18n::Str::nav_settings); }

private:
    void nav() {
        lv_obj_t* n = kit::column(root_, 4);
        lv_obj_set_size(n, 296, lv_pct(100));
        lv_obj_set_style_pad_hor(n, 16, 0);
        lv_obj_set_style_pad_top(n, 24, 0);
        lv_obj_set_style_border_side(n, LV_BORDER_SIDE_RIGHT, 0);
        lv_obj_set_style_border_width(n, 1, 0);
        lv_obj_set_style_border_color(n, theme::hairline(), 0);
        lv_obj_t* h = kit::text(n, tr(i18n::Str::nav_settings), theme::h1(), theme::text());
        lv_obj_set_style_pad_left(h, 12, 0);
        lv_obj_set_style_margin_bottom(h, 16, 0);
        static const Category cats[] = {
            {SettingsPage::Appearance, icon::eye, i18n::Str::page_appearance},
            {SettingsPage::Display, icon::sun, i18n::Str::page_display},
            {SettingsPage::Wifi, icon::wifi, i18n::Str::page_wifi},
            {SettingsPage::Server, icon::server, i18n::Str::settings_server},
            {SettingsPage::Language, icon::language, i18n::Str::settings_language},
            {SettingsPage::About, icon::info, i18n::Str::page_about},
        };
        for (const auto& c : cats) {
            bool on = settings_page() == c.page;
            lv_obj_t* r = kit::row(n, 14);
            lv_obj_set_size(r, lv_pct(100), 64);
            lv_obj_set_style_pad_hor(r, 12, 0);
            lv_obj_set_style_radius(r, theme::r_control(), 0);
            lv_obj_set_style_bg_color(r, theme::elevated(), 0);
            lv_obj_set_style_bg_opa(r, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(r, theme::surface(), LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
            kit::icon(r, c.glyph, 24, on ? theme::text() : theme::text2());
            lv_obj_t* col = kit::column(r, 2);
            lv_obj_set_flex_grow(col, 1);
            lv_obj_set_width(col, 1);
            kit::text(col, tr(c.title), theme::meta(), theme::text());
            kit::text(col, summary(c.page), theme::label(), theme::text3());
            SettingsPage page = c.page;
            kit::on_click(r, [page] { open_settings_page(page); });
        }
    }

    // One-line state under each category.
    static std::string summary(SettingsPage page) {
        const app::PanelView* pv = state().panel.get();
        switch (page) {
            case SettingsPage::Appearance: {
                if (!pv) return {};
                static const i18n::Str rounds[] = {i18n::Str::round_sharp, i18n::Str::round_normal, i18n::Str::round_round};
                const auto& a = pv->appearance;
                return std::string(tr(a.mode == app::ThemeMode::Light ? i18n::Str::appearance_light : i18n::Str::appearance_dark)) +
                       " · " + tr(rounds[static_cast<int>(a.roundness)]);
            }
            case SettingsPage::Display: {
                if (!pv) return {};
                const auto& d = pv->display;
                std::string s = d.dim_after_s ? i18n::tr(i18n::Str::settings_dims_after, dim_label(d.dim_after_s)) : tr(i18n::Str::dim_never);
                if (d.night) s += " · " + i18n::tr(i18n::Str::settings_night_range, clock_text(d.night_from) + "–" + clock_text(d.night_to));
                return s;
            }
            case SettingsPage::Wifi: return pv && pv->wifi.connected ? pv->wifi.ssid : tr(i18n::Str::wifi_not_connected);
            case SettingsPage::Server: return tr(state().connected() ? i18n::Str::home_connected : i18n::Str::home_reconnecting);
            case SettingsPage::Language: return language_label(i18n::language());
            case SettingsPage::About: return pv ? pv->system.firmware : std::string();
        }
        return {};
    }

    lv_obj_t* slider_row(lv_obj_t* card, const std::string& label, int value, std::function<void(int)> on_release) {
        lv_obj_t* r = line(card, 64);
        lv_obj_t* l = kit::text(r, label, theme::meta(), theme::text());
        lv_obj_set_width(l, 180);
        kit::icon(r, icon::sun, 18, theme::text3());
        lv_obj_t* s = kit::slider(r, value, [this, on_release](int v) {
            dragging_ = false;
            on_release(v);
        });
        lv_obj_add_event_cb(s, [](lv_event_t* e) { static_cast<Settings*>(lv_event_get_user_data(e))->dragging_ = true; },
                            LV_EVENT_PRESSED, this);
        lv_obj_set_flex_grow(s, 1);
        kit::icon(r, icon::sun, 26, theme::text3());
        return r;
    }

    // A selectable preview tile (theme / rounding): the selected one gets an accent ring.
    static lv_obj_t* option(lv_obj_t* parent, const std::string& label, bool selected, kit::Action action) {
        lv_obj_t* o = kit::column(parent, 10);
        lv_obj_set_style_pad_all(o, 8, 0);
        lv_obj_set_style_radius(o, theme::r_card(), 0);
        lv_obj_set_style_border_width(o, 2, 0);
        lv_obj_set_style_border_color(o, selected ? theme::accent() : theme::border(), 0);
        lv_obj_set_style_border_opa(o, selected ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_set_style_bg_color(o, theme::elevated(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_PRESSED);
        kit::press_scale(o);
        kit::on_click(o, std::move(action));
        lv_obj_t* l = kit::text(o, label, theme::meta(), selected ? theme::text() : theme::text2());
        lv_obj_set_style_pad_left(l, 4, 0);
        return o;
    }

    static lv_obj_t* shape(lv_obj_t* parent, int32_t w, int32_t h, int32_t radius, lv_color_t c) {
        lv_obj_t* b = lv_obj_create(parent);
        lv_obj_remove_style_all(b);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);  // the tile around it takes the tap
        lv_obj_set_size(b, w, h);
        lv_obj_set_style_radius(b, radius, 0);
        lv_obj_set_style_bg_color(b, c, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        return b;
    }

    void appearance(lv_obj_t* pane) {
        const app::PanelView* pv = state().panel.get();
        app::Appearance a = pv ? pv->appearance : app::Appearance{};
        auto set = [](app::Appearance x) { send(app::cmd::SetAppearance{x}); };

        lv_obj_t* g = group(pane, tr(i18n::Str::appearance_theme));
        lv_obj_t* r = line(g, 0);
        lv_obj_set_style_pad_ver(r, 20, 0);
        for (auto m : {app::ThemeMode::Dark, app::ThemeMode::Light}) {
            bool dark = m == app::ThemeMode::Dark;
            lv_obj_t* o = option(r, tr(dark ? i18n::Str::appearance_dark : i18n::Str::appearance_light), a.mode == m,
                                 [a, m, set] { auto x = a; x.mode = m; set(x); });
            // Mini preview: page, sidebar strip, a cover and two text lines.
            lv_obj_t* box = shape(o, 176, 100, theme::r_control(), lv_color_hex(dark ? 0x0A0A0A : 0xF6F6F7));
            lv_obj_move_to_index(box, 0);
            lv_obj_set_style_clip_corner(box, true, 0);
            auto block = [box](int x, int y, int w, int h, uint32_t col, int rad) {
                lv_obj_set_pos(shape(box, w, h, rad, lv_color_hex(col)), x, y);
            };
            block(0, 0, 38, 100, dark ? 0x000000 : 0xFFFFFF, 0);
            block(50, 14, 42, 42, 0xC9A227, 3);
            block(102, 20, 60, 8, dark ? 0xFFFFFF : 0x111111, 2);
            block(102, 34, 42, 6, dark ? 0xA3A3A3 : 0x52525B, 2);
            block(50, 70, 112, 18, dark ? 0x242424 : 0xE0E0E4, 3);
        }

        g = group(pane, tr(i18n::Str::appearance_accent));
        r = line(g, 0);
        lv_obj_set_style_pad_ver(r, 20, 0);
        lv_obj_set_style_pad_hor(r, 8, 0);  // room for the selection ring around the first swatch
        for (int i = 0; i < theme::accent_count(); ++i) {
            lv_obj_t* dot = shape(r, 44, 44, LV_RADIUS_CIRCLE, theme::accent_swatch(i));
            lv_obj_add_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(dot, 6);
            lv_obj_set_style_border_width(dot, i == 0 ? 1 : 0, 0);  // the neutral swatch needs an edge
            lv_obj_set_style_border_color(dot, theme::border(), 0);
            if (a.accent == i) {
                lv_obj_set_style_outline_width(dot, 2, 0);
                lv_obj_set_style_outline_pad(dot, 4, 0);
                lv_obj_set_style_outline_color(dot, theme::text(), 0);
            }
            kit::press_scale(dot);
            kit::on_click(dot, [a, i, set] { auto x = a; x.accent = static_cast<uint8_t>(i); set(x); });
        }
        footnote(g, tr(i18n::Str::appearance_accent_hint));

        g = group(pane, tr(i18n::Str::appearance_rounding));
        r = line(g, 0);
        lv_obj_set_style_pad_ver(r, 20, 0);
        static const i18n::Str names[] = {i18n::Str::round_sharp, i18n::Str::round_normal, i18n::Str::round_round};
        static const int32_t sample[] = {3, 10, 22};
        for (int i = 0; i < 3; ++i) {
            auto rr = static_cast<app::Roundness>(i);
            lv_obj_t* o = option(r, tr(names[i]), a.roundness == rr, [a, rr, set] { auto x = a; x.roundness = rr; set(x); });
            lv_obj_move_to_index(shape(o, 112, 64, sample[i], theme::elevated()), 0);
        }

        g = group(pane, tr(i18n::Str::appearance_motion));
        toggle_row(g, tr(i18n::Str::appearance_reduce_motion), tr(i18n::Str::appearance_reduce_motion_sub), a.reduce_motion,
                   [a, set](bool on) { auto x = a; x.reduce_motion = on; set(x); });
    }

    void display(lv_obj_t* pane) {
        const app::PanelView* pv = state().panel.get();
        if (!pv) return;
        app::DisplaySettings d = pv->display;
        auto set = [](app::DisplaySettings x) { send(app::cmd::SetDisplay{x}); };

        lv_obj_t* g = group(pane, tr(i18n::Str::display_day));
        slider_row(g, tr(i18n::Str::display_brightness), d.brightness, [d, set](int v) { auto x = d; x.brightness = std::max(v, 5); set(x); });
        static const int dims[] = {30, 120, 300, 0};
        std::vector<std::pair<std::string, bool>> opts;
        for (int s : dims) opts.push_back({dim_label(s), d.dim_after_s == s});
        choice_row(g, tr(i18n::Str::display_dim_after), opts, [d, set](size_t i) { auto x = d; x.dim_after_s = dims[i]; set(x); });
        toggle_row(g, tr(i18n::Str::display_clock), tr(i18n::Str::display_clock_sub), d.clock_when_dimmed,
                   [d, set](bool on) { auto x = d; x.clock_when_dimmed = on; set(x); });
        action_row(g, icon::moon, tr(i18n::Str::display_preview), [] { show_ambient(); });

        g = group(pane, tr(i18n::Str::display_night));
        toggle_row(g, tr(i18n::Str::display_schedule), tr(i18n::Str::display_schedule_sub), d.night,
                   [d, set](bool on) { auto x = d; x.night = on; set(x); });
        if (d.night) {
            time_row(g, tr(i18n::Str::display_from), d.night_from, [d, set](int m) { auto x = d; x.night_from = m; set(x); });
            time_row(g, tr(i18n::Str::display_until), d.night_to, [d, set](int m) { auto x = d; x.night_to = m; set(x); });
            slider_row(g, tr(i18n::Str::display_night_brightness), d.night_brightness,
                       [d, set](int v) { auto x = d; x.night_brightness = std::max(v, 1); set(x); });
            if (pv->night_now) footnote(g, tr(i18n::Str::display_night_now));
        }
    }

    void wifi(lv_obj_t* pane) {
        const app::PanelView* pv = state().panel.get();
        bool up = pv && pv->wifi.connected;
        lv_obj_t* g = group(pane, tr(i18n::Str::page_wifi));
        value_row(g, up ? pv->wifi.ssid : tr(i18n::Str::wifi_not_connected), up ? tr(i18n::Str::home_connected) : "—",
                  up ? theme::text2() : theme::warning());
        if (up) {
            int rssi = pv->wifi.rssi;
            i18n::Str strength = rssi > -60 ? i18n::Str::wifi_strong : rssi > -72 ? i18n::Str::wifi_ok : i18n::Str::wifi_weak;
            value_row(g, tr(i18n::Str::wifi_signal), std::string(tr(strength)) + " · " + std::to_string(rssi) + " dBm");
            value_row(g, tr(i18n::Str::wifi_address), pv->wifi.ip);
        }
        action_row(g, nullptr, tr(i18n::Str::wifi_change), [] { send(app::cmd::SetupGo{app::SetupStep::Wifi}); });
        footnote(g, tr(i18n::Str::wifi_drops_body));
    }

    void server(lv_obj_t* pane) {
        const app::PanelView* pv = state().panel.get();
        const auto& c = state().connection;
        bool up = state().connected();
        lv_obj_t* g = group(pane, tr(i18n::Str::settings_server));
        value_row(g, tr(i18n::Str::server_server), pv ? pv->server_address : "—");
        value_row(g, tr(i18n::Str::server_version), c.server.server_version.empty() ? "—" : c.server.server_version);
        value_row(g, tr(i18n::Str::server_signed_in),
                  pv && !pv->token_hint.empty() ? i18n::tr(i18n::Str::server_token_ending, pv->token_hint) : "—");
        value_row(g, tr(i18n::Str::conn_status), tr(up ? i18n::Str::home_connected : i18n::Str::home_reconnecting),
                  up ? theme::text2() : theme::warning());
        action_row(g, nullptr, tr(i18n::Str::server_change), [] { send(app::cmd::SetupGo{app::SetupStep::Server}); });
        action_row(g, nullptr, tr(i18n::Str::server_sign_in_again), [] { send(app::cmd::SignInAgain{}); });

        g = group(pane, tr(i18n::Str::server_services));
        std::vector<std::string> streaming;
        if (pv) {
            for (const auto& p : pv->providers) {
                if (!p.streaming && p.domain != "builtin") continue;
                if (p.streaming && std::find(streaming.begin(), streaming.end(), p.domain) == streaming.end()) streaming.push_back(p.domain);
                lv_obj_t* r = line(g);
                kit::badge(r, p.domain == "builtin" ? "library" : p.domain);
                grow_text(r, p.domain == "builtin" ? tr(i18n::Str::menu_your_library) : p.name);
                kit::text(r, tr(p.available ? i18n::Str::server_working : i18n::Str::server_not_responding), theme::meta(),
                          p.available ? theme::text2() : theme::warning());
            }
        }

        // Preferred source: library & favourites always first (core rule), then this order.
        g = group(pane, tr(i18n::Str::server_order));
        std::vector<std::string> list = pv ? pv->source_order : std::vector<std::string>{};
        for (const auto& d : streaming)
            if (std::find(list.begin(), list.end(), d) == list.end()) list.push_back(d);
        auto numbered = [g](size_t n, const std::string& badge, const std::string& name) {
            lv_obj_t* r = line(g);
            lv_obj_t* num = kit::text(r, std::to_string(n), theme::meta(), theme::text3());
            lv_obj_set_width(num, 16);
            kit::badge(r, badge);
            grow_text(r, name);
            return r;
        };
        numbered(1, "library", tr(i18n::Str::server_library_first));
        for (size_t i = 0; i < list.size(); ++i) {
            lv_obj_t* r = numbered(i + 2, list[i], kit::provider_name(list[i]));
            if (i > 0) {
                kit::icon_button(r, icon::chevron_up, [list, i] {
                    auto next = list;
                    std::swap(next[i], next[i - 1]);
                    send(app::cmd::SetSourceOrder{next});
                }, 48, 22);
            }
        }
        footnote(g, tr(i18n::Str::server_order_sub));
    }

    void language(lv_obj_t* pane) {
        lv_obj_t* g = group(pane, tr(i18n::Str::settings_language));
        for (int i = 0; i < i18n::kLangCount; ++i) {
            auto lang = static_cast<i18n::Lang>(i);
            lv_obj_t* r = line(g);
            lv_obj_set_style_bg_color(r, theme::elevated(), LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
            grow_text(r, language_label(lang));
            if (lang == i18n::language()) kit::icon(r, icon::check, 24, theme::accent());
            kit::on_click(r, [lang] { send(app::cmd::SetLanguage{lang}); });
        }
        footnote(g, tr(i18n::Str::language_hint));
    }

    void about(lv_obj_t* pane) {
        const app::PanelView* pv = state().panel.get();
        lv_obj_t* g = group(pane, tr(i18n::Str::about_title));
        if (pv) {
            value_row(g, tr(i18n::Str::about_software), pv->system.firmware);
            value_row(g, tr(i18n::Str::about_running), uptime(pv->system.uptime_s));
            value_row(g, tr(i18n::Str::about_memory),
                      i18n::tr(i18n::Str::about_memory_value, std::to_string(pv->system.free_internal_kb) + " KB · PSRAM " +
                                                                  std::to_string(pv->system.free_psram_kb / 1024) + " MB"));
        }

        // Software update: one row whose text and action follow the update state.
        g = group(pane, tr(i18n::Str::ota_title));
        const app::UpdateView& u = state().update;
        switch (u.state) {
            case app::UpdateState::Checking: value_row(g, tr(i18n::Str::ota_checking), ""); break;
            case app::UpdateState::Installing:
                value_row(g, i18n::tr(i18n::Str::ota_installing, std::to_string(u.percent)), u.version);
                break;
            case app::UpdateState::Available:
                value_row(g, i18n::tr(i18n::Str::ota_available, u.version), "");
                action_row(g, icon::refresh, tr(i18n::Str::ota_install), [] { send(app::cmd::InstallUpdate{}); });
                break;
            default: {
                i18n::Str note = u.state == app::UpdateState::UpToDate  ? i18n::Str::ota_up_to_date
                               : u.state == app::UpdateState::Failed    ? i18n::Str::ota_failed
                               : u.state == app::UpdateState::NoNetwork ? i18n::Str::ota_no_network
                                                                        : i18n::Str::ota_check;
                if (u.state != app::UpdateState::Idle) value_row(g, tr(note), "");
                action_row(g, icon::refresh, tr(i18n::Str::ota_check), [] { send(app::cmd::CheckUpdate{}); });
            }
        }

        g = group(pane, tr(i18n::Str::about_diagnostics));
        app::Diagnostics d = pv ? pv->diagnostics : app::Diagnostics{};
        toggle_row(g, tr(i18n::Str::about_fps), tr(i18n::Str::about_fps_sub), d.fps,
                   [d](bool on) { auto x = d; x.fps = on; send(app::cmd::SetDiagnostics{x}); });
        toggle_row(g, tr(i18n::Str::about_touches), tr(i18n::Str::about_touches_sub), d.touches,
                   [d](bool on) { auto x = d; x.touches = on; send(app::cmd::SetDiagnostics{x}); });

        g = group(pane, {});
        action_row(g, icon::power, tr(i18n::Str::about_restart), [] {
            confirm(tr(i18n::Str::about_restart_q), tr(i18n::Str::about_restart_body), tr(i18n::Str::edit_delete_keep),
                    tr(i18n::Str::about_restart), [] { send(app::cmd::RestartPanel{}); });
        });
        action_row(g, icon::trash, tr(i18n::Str::about_reset), [] {
            confirm(tr(i18n::Str::about_reset_q), tr(i18n::Str::about_reset_body), tr(i18n::Str::edit_delete_keep),
                    tr(i18n::Str::about_reset), [] { send(app::cmd::ResetPanel{}); });
        }, true);
    }

    lv_obj_t* root_ = nullptr;
    bool dragging_ = false;
};

}  // namespace

std::unique_ptr<Screen> make_settings() { return std::make_unique<Settings>(); }

}  // namespace ui
