// Settings (first slice): which speaker plays, language, and connection info.
// Sub-screens (screen & night mode, Wi-Fi, Music Assistant, about) follow in the next slice.
#include "../shell.hpp"

namespace ui {
namespace {

class Settings : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_pad_all(root, theme::kPad, 0);
        lv_obj_set_style_pad_row(root, 24, 0);
    }

    void refresh(Topic t) override {
        if (t != Topic::All && t != Topic::Players && t != Topic::Connection) return;
        lv_obj_clean(root_);
        kit::text(root_, tr(i18n::Str::nav_settings), theme::h1(), theme::text());
        lv_obj_t* cols = kit::row(root_, 24);
        lv_obj_set_width(cols, lv_pct(100));
        lv_obj_set_flex_align(cols, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        lv_obj_t* speakers = card(cols, tr(i18n::Str::settings_play_on));
        if (const auto* pv = state().players.get()) {
            for (const auto& p : pv->players) speaker_row(speakers, p, p.player_id == pv->selected_id);
        }

        lv_obj_t* panel = card(cols, tr(i18n::Str::settings_this_panel));
        kit::overline(panel, tr(i18n::Str::settings_language));
        lv_obj_t* langs = kit::row(panel, 12);
        for (int i = 0; i < i18n::kLangCount; ++i) {
            auto lang = static_cast<i18n::Lang>(i);
            kit::chip(langs, nullptr, language_label(lang),
                      i18n::language() == lang, [lang] { send(app::cmd::SetLanguage{lang}); });
        }
        kit::overline(panel, tr(i18n::Str::settings_server));
        const auto& c = state().connection;
        std::string server = c.server.server_version.empty() ? tr(i18n::Str::home_reconnecting)
                                                             : "Music Assistant " + c.server.server_version;
        kit::text(panel, server, theme::body(), theme::text());
    }

    ScreenId tab() const override { return ScreenId::Settings; }
    std::string title() const override { return tr(i18n::Str::nav_settings); }

private:
    // Each language names itself, so it is findable whatever language is active.
    static std::string language_label(i18n::Lang lang) {
        i18n::Lang current = i18n::language();
        i18n::set_language(lang);
        std::string name = tr(i18n::Str::language_name_self);
        i18n::set_language(current);
        return name;
    }

    static lv_obj_t* card(lv_obj_t* parent, const std::string& title) {
        lv_obj_t* c = kit::column(parent, 16);
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_width(c, 1);
        lv_obj_set_style_pad_all(c, 28, 0);
        lv_obj_set_style_radius(c, theme::kRadiusCard, 0);
        lv_obj_set_style_bg_color(c, theme::surface(), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        kit::text(c, title, theme::h2(), theme::text());
        return c;
    }

    static void speaker_row(lv_obj_t* parent, const core::PlayerState& p, bool selected) {
        lv_obj_t* r = kit::row(parent, 16);
        lv_obj_set_size(r, lv_pct(100), 72);
        lv_obj_set_style_pad_hor(r, 16, 0);
        lv_obj_set_style_radius(r, theme::kRadiusRow, 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(r, selected ? theme::elevated() : theme::surface(), 0);
        lv_obj_set_style_bg_color(r, lv_color_mix(lv_color_white(), theme::elevated(), 31), LV_STATE_PRESSED);
        kit::icon(r, icon::speaker, 28, selected ? theme::accent() : theme::text2());
        lv_obj_t* name = kit::text(r, p.name, theme::body(), p.available ? theme::text() : theme::text3());
        lv_obj_set_flex_grow(name, 1);
        if (selected) kit::icon(r, icon::check, 28, theme::accent());
        std::string id = p.player_id;
        kit::on_click(r, [id] { send(app::cmd::SelectPlayer{id}); });
    }

    lv_obj_t* root_ = nullptr;
};

}  // namespace

std::unique_ptr<Screen> make_settings() { return std::make_unique<Settings>(); }

}  // namespace ui
