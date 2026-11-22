// First-time setup: Welcome → Wi-Fi (+ password) → server → sign in → speaker → done.
// Core (features/setup.cpp) owns the step and the errors; this screen only shows them.
#include "../shell.hpp"

namespace ui {
namespace {

using app::SetupStep;

class Setup : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_pad_hor(root, 96, 0);
        lv_obj_set_style_pad_top(root, 56, 0);
        lv_obj_set_style_pad_bottom(root, 40, 0);
        lv_obj_set_style_pad_row(root, 16, 0);
    }

    void refresh(Topic t) override {
        if (t != Topic::All && t != Topic::Setup && t != Topic::Players) return;
        const app::SetupView* v = state().setup.get();
        if (!v) return;
        if (t == Topic::Players && v->step != SetupStep::Speaker) return;  // keep a field being typed in
        if (v->step != shown_step_) typing_ = false;
        shown_step_ = v->step;
        lv_obj_clean(root_);
        kb_ = input_ = nullptr;
        switch (v->step) {
            case SetupStep::Welcome: welcome(); break;
            case SetupStep::Wifi: wifi(*v); break;
            case SetupStep::WifiPassword: password(*v); break;
            case SetupStep::Server: server(*v); break;
            case SetupStep::SignIn: sign_in(*v); break;
            case SetupStep::Speaker: speaker(); break;
            case SetupStep::Done: done(); break;
            case SetupStep::Finished: break;
        }
    }

    bool full_screen() const override { return true; }
    bool mini_player() const override { return false; }
    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::setup_title); }

private:
    static void go(SetupStep s) { send(app::cmd::SetupGo{s}); }

    void header(int step, i18n::Str title, i18n::Str body, const std::string& title_arg = {}) {
        if (step) kit::overline(root_, i18n::tr(i18n::Str::setup_step, std::to_string(step)), theme::accent());
        kit::text(root_, title_arg.empty() ? std::string(tr(title)) : i18n::tr(title, title_arg), theme::display(), theme::text());
        if (body != i18n::Str::setup_title) kit::text(root_, tr(body), theme::lead(), theme::text2(), true);
    }

    lv_obj_t* content() {
        lv_obj_t* c = kit::scroll_column(root_, 8);
        lv_obj_set_width(c, lv_pct(100));
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_style_pad_top(c, 12, 0);
        return c;
    }

    void footer(SetupStep back, kit::Action next, bool next_enabled = true, i18n::Str next_label = i18n::Str::setup_continue) {
        lv_obj_t* f = kit::row(root_, 16);
        lv_obj_set_width(f, lv_pct(100));
        kit::button(f, kit::Button::Secondary, icon::chevron_left, tr(i18n::Str::setup_back), [back] { go(back); });
        kit::spacer(f);
        if (next) {
            lv_obj_t* b = kit::button(f, kit::Button::Primary, nullptr, tr(next_label), next);
            if (!next_enabled) {
                lv_obj_add_state(b, LV_STATE_DISABLED);
                lv_obj_set_style_bg_color(b, theme::elevated(), LV_STATE_DISABLED);
                lv_obj_set_style_text_color(lv_obj_get_child(b, 0), theme::text3(), 0);
            }
        }
    }

    // A text field + keyboard along the bottom (the keyboard covers the footer while typing).
    void field(lv_obj_t* parent, const std::string& placeholder, bool password, i18n::Str done_key,
               std::function<void(std::string)> on_done) {
        input_ = lv_textarea_create(parent);
        lv_textarea_set_one_line(input_, true);
        lv_textarea_set_password_mode(input_, password);
        lv_textarea_set_placeholder_text(input_, placeholder.c_str());
        lv_obj_set_size(input_, lv_pct(100), 64);
        lv_obj_set_style_radius(input_, 16, 0);
        lv_obj_set_style_bg_color(input_, theme::surface(), 0);
        lv_obj_set_style_border_width(input_, 2, 0);
        lv_obj_set_style_border_color(input_, theme::border(), 0);
        lv_obj_set_style_border_color(input_, theme::accent(), LV_STATE_FOCUSED);
        lv_obj_set_style_text_font(input_, theme::h2(), 0);
        lv_obj_set_style_text_color(input_, theme::text(), 0);
        lv_obj_set_style_text_color(input_, theme::text3(), LV_PART_TEXTAREA_PLACEHOLDER);
        lv_obj_set_style_pad_hor(input_, 20, 0);
        lv_obj_set_style_pad_ver(input_, 14, 0);
        auto finish = [this, on_done] {
            std::string text = lv_textarea_get_text(input_);
            typing_ = false;
            if (kb_) lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
            if (!text.empty()) on_done(text);
        };
        kb_ = kit::keyboard(root_, input_, tr(i18n::Str::key_delete), tr(done_key), tr(i18n::Str::key_space), finish);
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 40);  // root has 40 px bottom padding
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
        kit::on_click(input_, [this] { show_keyboard(); });
    }

    void show_keyboard() {
        typing_ = true;
        lv_obj_add_state(input_, LV_STATE_FOCUSED);
        lv_obj_remove_flag(kb_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(kb_);
    }

    void welcome() {
        lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(0, i18n::Str::setup_welcome_title, i18n::Str::setup_welcome_body);
        lv_obj_t* b = kit::button(root_, kit::Button::Primary, nullptr, tr(i18n::Str::setup_get_started), [] { go(SetupStep::Wifi); });
        lv_obj_set_style_margin_top(b, 24, 0);
    }

    void wifi(const app::SetupView& v) {
        lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(1, i18n::Str::setup_wifi_title, i18n::Str::setup_wifi_body);
        lv_obj_t* c = content();
        lv_obj_t* head = kit::row(c, 12);
        lv_obj_set_width(head, lv_pct(100));
        lv_obj_t* l = kit::overline(head, tr(v.scanning ? i18n::Str::setup_scanning : i18n::Str::setup_networks));
        lv_obj_set_flex_grow(l, 1);
        kit::chip(head, icon::refresh, tr(i18n::Str::setup_scan_again), false, [] { send(app::cmd::SetupScanWifi{}); });
        for (const auto& n : v.networks) {
            std::string ssid = n.ssid;
            bool open = !n.secure;
            lv_obj_t* r = nav_row(c, icon::wifi, n.ssid, "", [this, ssid, open] {
                chosen_ssid_ = ssid;
                if (open) send(app::cmd::SetupJoinWifi{ssid, ""});
                else go(SetupStep::WifiPassword);
            }, false);
            if (n.secure) kit::icon(r, icon::lock, 22, theme::text3());
        }
        footer(SetupStep::Welcome, {});
    }

    void password(const app::SetupView& v) {
        lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        std::string ssid = v.ssid.empty() ? chosen_ssid_ : v.ssid;
        header(1, i18n::Str::setup_join, i18n::Str::setup_title, ssid);
        kit::text(root_, i18n::tr(i18n::Str::setup_password_for, ssid), theme::body(), theme::text2());
        field(root_, "", true, i18n::Str::setup_join_key, [ssid](std::string pw) { send(app::cmd::SetupJoinWifi{ssid, pw}); });
        if (v.joining) kit::text(root_, tr(i18n::Str::setup_joining), theme::body(), theme::text2());
        if (v.wifi_error)
            kit::text(root_, tr(*v.wifi_error == app::WifiResult::WrongPassword ? i18n::Str::setup_wrong_password
                                                                               : i18n::Str::setup_wifi_failed),
                      theme::body(), theme::danger_text(), true);
        kit::spacer(root_);
        footer(SetupStep::Wifi, {});
        if (!v.joining) show_keyboard();
    }

    void server(const app::SetupView& v) {
        lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(2, i18n::Str::setup_server_title, i18n::Str::setup_server_body);
        lv_obj_t* c = content();
        kit::overline(c, tr(v.searching ? i18n::Str::setup_still_looking : i18n::Str::setup_found));
        for (const auto& s : v.servers) {
            std::string address = s.address;
            nav_row(c, icon::server, s.name, s.address, [address] { send(app::cmd::SetupUseServer{address}); });
        }
        if (!v.searching && v.servers.empty()) {
            lv_obj_t* b = banner(c, tr(i18n::Str::setup_no_server), tr(i18n::Str::setup_no_server_hint));
            kit::chip(b, icon::refresh, tr(i18n::Str::setup_search_again), false, [] { send(app::cmd::SetupFindServers{}); });
        }
        kit::overline(c, tr(i18n::Str::setup_enter_address));
        field(c, tr(i18n::Str::setup_address_example), false, i18n::Str::setup_connect_key,
              [](std::string a) { send(app::cmd::SetupUseServer{a}); });
        if (v.checking) kit::text(c, i18n::tr(i18n::Str::setup_checking, v.address), theme::body(), theme::text2());
        if (v.server_error) kit::text(c, tr(i18n::Str::setup_server_error), theme::body(), theme::danger_text(), true);
        footer(SetupStep::Wifi, {});
    }

    void sign_in(const app::SetupView& v) {
        lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(3, i18n::Str::setup_signin_title, i18n::Str::setup_signin_body);
        lv_obj_t* body = kit::row(root_, 40);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        if (!v.pairing_url.empty() && !typing_) {
            lv_obj_t* qr_box = kit::column(body);
            lv_obj_set_style_pad_all(qr_box, 16, 0);
            lv_obj_set_style_radius(qr_box, 16, 0);
            lv_obj_set_style_bg_color(qr_box, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(qr_box, LV_OPA_COVER, 0);
#if LV_USE_QRCODE
            lv_obj_t* qr = lv_qrcode_create(qr_box);
            lv_qrcode_set_size(qr, 200);
            lv_qrcode_set_dark_color(qr, lv_color_black());
            lv_qrcode_set_light_color(qr, lv_color_white());
            lv_qrcode_update(qr, v.pairing_url.data(), static_cast<uint32_t>(v.pairing_url.size()));
#endif
        }
        lv_obj_t* col = kit::column(body, 12);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        if (!v.pairing_url.empty() && !typing_) {
            kit::text(col, tr(i18n::Str::setup_use_phone), theme::h2(), theme::text());
            kit::text(col, i18n::tr(i18n::Str::setup_phone_1, v.pairing_url), theme::body(), theme::text2(), true);
            kit::text(col, tr(i18n::Str::setup_phone_2), theme::body(), theme::text2(), true);
            kit::text(col, tr(i18n::Str::setup_phone_3), theme::body(), theme::text2(), true);
        }
        if (v.checking) kit::text(col, tr(i18n::Str::setup_checking_token), theme::body(), theme::text2());
        else if (v.token_error) banner(col, tr(i18n::Str::setup_token_rejected), tr(i18n::Str::setup_token_rejected_body));
        else if (!v.pairing_url.empty() && !typing_) kit::text(col, tr(i18n::Str::setup_waiting_token), theme::body(), theme::accent());
        field(col, tr(i18n::Str::setup_token_placeholder), false, i18n::Str::setup_signin_key,
              [](std::string token) { send(app::cmd::SetupToken{token}); });
        if (v.pairing_url.empty() || typing_) show_keyboard();
        else lv_obj_add_flag(input_, LV_OBJ_FLAG_HIDDEN);
        if (!typing_ && !v.pairing_url.empty())
            kit::chip(col, icon::edit, tr(i18n::Str::setup_type_token), false, [this] {
                typing_ = true;
                kit::later([this] { refresh(Topic::Setup); });
            });
        kit::spacer(root_);
        footer(SetupStep::Server, {});
    }

    void speaker() {
        typing_ = false;
        lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(4, i18n::Str::setup_speaker_title, i18n::Str::setup_speaker_body);
        lv_obj_t* c = content();
        const auto* pv = state().players.get();
        if (!pv) kit::text(c, tr(i18n::Str::list_loading), theme::lead(), theme::text2());
        bool any = pv && !pv->selected_id.empty();
        for (size_t i = 0; pv && i < pv->players.size(); ++i) {
            const auto& p = pv->players[i];
            std::string id = p.player_id;
            std::string sub = !p.available ? tr(i18n::Str::setup_offline)
                            : p.group_members.empty() ? tr(i18n::Str::setup_available) : tr(i18n::Str::setup_group);
            lv_obj_t* r = nav_row(c, icon::speaker, p.name, sub, [id] { send(app::cmd::SelectPlayer{id}); }, false);
            if (id == pv->selected_id) {
                lv_obj_set_style_border_width(r, 2, 0);
                lv_obj_set_style_border_color(r, theme::accent(), 0);
                kit::icon(r, icon::check, 28, theme::accent());
            }
        }
        footer(SetupStep::SignIn, [] { go(SetupStep::Done); }, any);
    }

    void done() {
        lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(0, i18n::Str::setup_done_title, i18n::Str::setup_title);
        const core::PlayerState* p = state().selected();
        if (p) kit::text(root_, i18n::tr(i18n::Str::queue_playing_on, p->name), theme::lead(), theme::accent());
        lv_obj_t* tips = kit::row(root_, 24);
        lv_obj_set_width(tips, lv_pct(100));
        lv_obj_set_style_pad_ver(tips, 24, 0);
        struct Tip { const char* glyph; i18n::Str title, body; };
        for (Tip t : {Tip{icon::heart, i18n::Str::setup_tip1, i18n::Str::setup_tip1_body},
                      Tip{icon::search, i18n::Str::setup_tip2, i18n::Str::setup_tip2_body},
                      Tip{icon::moon, i18n::Str::setup_tip3, i18n::Str::setup_tip3_body}}) {
            lv_obj_t* c = card(tips);
            lv_obj_set_flex_grow(c, 1);
            lv_obj_set_width(c, 1);
            kit::icon(c, t.glyph, 28, theme::accent());
            kit::text(c, tr(t.title), theme::body(), theme::text());
            kit::text(c, tr(t.body), theme::meta(), theme::text2(), true);
        }
        kit::button(root_, kit::Button::Primary, icon::play, tr(i18n::Str::setup_start), [] { send(app::cmd::SetupFinish{}); });
    }

    lv_obj_t* root_ = nullptr;
    lv_obj_t* input_ = nullptr;
    lv_obj_t* kb_ = nullptr;
    std::string chosen_ssid_;
    bool typing_ = false;
    SetupStep shown_step_ = SetupStep::Finished;
};

}  // namespace

std::unique_ptr<Screen> make_setup() { return std::make_unique<Setup>(); }

}  // namespace ui
