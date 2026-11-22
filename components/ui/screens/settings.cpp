// Settings: "Play music on" + "This panel" rows leading to the sub-pages (settings_page.cpp).
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

class Settings : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_pad_hor(root, theme::kPad, 0);
        lv_obj_set_style_pad_top(root, 24, 0);
        lv_obj_set_style_pad_row(root, 20, 0);
    }

    void refresh(Topic t) override {
        if (t != Topic::All && t != Topic::Players && t != Topic::Connection && t != Topic::Panel) return;
        lv_obj_clean(root_);
        kit::text(root_, tr(i18n::Str::nav_settings), theme::h1(), theme::text());
        lv_obj_t* cols = kit::row(root_, 24);
        lv_obj_set_width(cols, lv_pct(100));
        lv_obj_set_flex_align(cols, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        lv_obj_t* speakers = card(cols, tr(i18n::Str::settings_play_on));
        lv_obj_set_flex_grow(speakers, 1);
        lv_obj_set_width(speakers, 1);
        if (const auto* pv = state().players.get()) {
            for (const auto& p : pv->players) speaker_row(speakers, p, p.player_id == pv->selected_id, true);
            kit::text(speakers, tr(i18n::Str::settings_speakers_hint), theme::label(), theme::text3(), true);
        }

        lv_obj_t* panel = card(cols, tr(i18n::Str::settings_this_panel));
        lv_obj_set_flex_grow(panel, 1);
        lv_obj_set_width(panel, 1);
        const app::PanelView* pv = state().panel.get();
        std::string display_sub;
        if (pv) {
            const auto& d = pv->display;
            display_sub = d.dim_after_s ? i18n::tr(i18n::Str::settings_dims_after, dim_label(d.dim_after_s)) : tr(i18n::Str::dim_never);
            if (d.night) display_sub += " · " + i18n::tr(i18n::Str::settings_night_range, clock_text(d.night_from) + "–" + clock_text(d.night_to));
        }
        if (pv) {
            const auto& a = pv->appearance;
            static const i18n::Str rounds[] = {i18n::Str::round_sharp, i18n::Str::round_normal, i18n::Str::round_round};
            std::string look = std::string(tr(a.mode == app::ThemeMode::Light ? i18n::Str::appearance_light : i18n::Str::appearance_dark)) +
                               " · " + tr(rounds[static_cast<int>(a.roundness)]);
            nav_row(panel, icon::eye, tr(i18n::Str::page_appearance), look, [] { open_settings_page(SettingsPage::Appearance); });
        }
        nav_row(panel, icon::sun, tr(i18n::Str::page_display), display_sub, [] { open_settings_page(SettingsPage::Display); });
        nav_row(panel, icon::wifi, tr(i18n::Str::page_wifi), pv && pv->wifi.connected ? pv->wifi.ssid : tr(i18n::Str::wifi_not_connected),
                [] { open_settings_page(SettingsPage::Wifi); });
        std::string server_sub = std::string(tr(state().connected() ? i18n::Str::home_connected : i18n::Str::home_reconnecting)) +
                                 (pv && !pv->server_address.empty() ? " · " + pv->server_address : "");
        nav_row(panel, icon::server, tr(i18n::Str::settings_server), server_sub, [] { open_settings_page(SettingsPage::Server); });
        nav_row(panel, icon::language, tr(i18n::Str::settings_language), language_label(i18n::language()),
                [] { open_settings_page(SettingsPage::Language); });
        nav_row(panel, icon::info, tr(i18n::Str::page_about), tr(i18n::Str::page_about_sub), [] { open_settings_page(SettingsPage::About); });
    }

    ScreenId tab() const override { return ScreenId::Settings; }
    std::string title() const override { return tr(i18n::Str::nav_settings); }

    static std::string dim_label(int s) { return s < 60 ? std::to_string(s) + " s" : std::to_string(s / 60) + " min"; }

private:
    lv_obj_t* root_ = nullptr;
};

}  // namespace

std::unique_ptr<Screen> make_settings() { return std::make_unique<Settings>(); }

}  // namespace ui
