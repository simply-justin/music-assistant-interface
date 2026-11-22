// Starting up: Wi-Fi → Music Assistant → restoring the speaker, then Home. After a minute a hint.
#include "../shell.hpp"

namespace ui {
namespace {

class Boot : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_pad_all(root, 96, 0);
        lv_obj_set_style_pad_row(root, 20, 0);
        lv_obj_set_flex_align(root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    }
    void on_open() override { opened_ = lv_tick_get(); }

    void refresh(Topic t) override {
        if (t == Topic::Progress && lv_tick_elaps(opened_) < 60'000) return;
        lv_obj_clean(root_);
        lv_obj_t* spin = lv_spinner_create(root_);
        lv_obj_set_size(spin, 36, 36);
        lv_obj_set_style_arc_width(spin, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_width(spin, 4, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(spin, theme::elevated(), LV_PART_MAIN);
        lv_obj_set_style_arc_color(spin, theme::accent(), LV_PART_INDICATOR);
        kit::text(root_, tr(i18n::Str::boot_title), theme::display(), theme::text());
        kit::text(root_, tr(i18n::Str::boot_body), theme::lead(), theme::text2());

        const auto* p = state().panel.get();
        const auto& c = state().connection;
        bool wifi = p && p->wifi.connected;
        bool server = state().connected();
        step(wifi, tr(wifi ? i18n::Str::boot_wifi_ok : i18n::Str::boot_wifi), p ? p->wifi.ssid : "");
        step(server, tr(i18n::Str::boot_server), p ? p->server_address : "");
        const core::PlayerState* pl = state().selected();
        step(server && pl, tr(i18n::Str::boot_restoring), pl ? pl->name : "");
        if (c.state == backend::Connection::AuthFailed)
            kit::text(root_, tr(i18n::Str::conn_auth_failed), theme::body(), theme::warning());
        if (lv_tick_elaps(opened_) >= 60'000)
            kit::text(root_, tr(i18n::Str::boot_slow), theme::body(), theme::warning(), true);
    }

    bool full_screen() const override { return true; }
    bool mini_player() const override { return false; }
    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::boot_title); }

private:
    void step(bool done, const std::string& label, const std::string& detail) {
        lv_obj_t* r = kit::row(root_, 16);
        kit::icon(r, done ? icon::check : icon::refresh, 28, done ? theme::accent() : theme::text3());
        kit::text(r, label, theme::body(), done ? theme::text() : theme::text2());
        if (!detail.empty()) kit::text(r, detail, theme::meta(), theme::text3());
    }

    lv_obj_t* root_ = nullptr;
    uint32_t opened_ = 0;
};

}  // namespace

std::unique_ptr<Screen> make_boot() { return std::make_unique<Boot>(); }

}  // namespace ui
