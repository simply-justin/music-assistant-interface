// First-time setup: Welcome → Wi-Fi (+ password) → server → sign in → speaker → done.
// Laid out like Settings: the steps on the left (where you are, what is done), the current step on
// the right in grouped rows. Core (features/setup.cpp) owns the step and the errors; this screen
// only shows them.
#include "../shell.hpp"

namespace ui {
namespace {

using app::SetupStep;

constexpr int32_t kRailW = 344;

// Rail position of a step: 0 = before the steps, 1–4 = Wi-Fi … Speaker, 5 = all done.
int position(SetupStep s) {
    switch (s) {
        case SetupStep::Welcome: return 0;
        case SetupStep::Wifi:
        case SetupStep::WifiPassword: return 1;
        case SetupStep::Server: return 2;
        case SetupStep::SignIn: return 3;
        case SetupStep::Speaker: return 4;
        case SetupStep::Done:
        case SetupStep::Finished: return 5;
    }
    return 0;
}

class Setup : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(root, 0, 0);
        lv_obj_set_style_pad_column(root, 0, 0);
        lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
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
        rail(position(v->step));
        pane_ = kit::column(root_, 16);
        lv_obj_set_height(pane_, lv_pct(100));
        lv_obj_set_flex_grow(pane_, 1);
        lv_obj_set_style_pad_hor(pane_, 64, 0);
        lv_obj_set_style_pad_top(pane_, 64, 0);
        lv_obj_set_style_pad_bottom(pane_, 40, 0);
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
    bool player_bar() const override { return false; }
    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::setup_title); }

private:
    static void go(SetupStep s) { send(app::cmd::SetupGo{s}); }

    // The steps: done ones get a check, the current one is highlighted, later ones are dimmed.
    void rail(int at) {
        lv_obj_t* r = kit::column(root_, 4);
        lv_obj_set_size(r, kRailW, lv_pct(100));
        lv_obj_set_style_pad_hor(r, 24, 0);
        lv_obj_set_style_pad_top(r, 64, 0);
        lv_obj_set_style_bg_color(r, theme::side(), 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_RIGHT, 0);
        lv_obj_set_style_border_width(r, 1, 0);
        lv_obj_set_style_border_color(r, theme::hairline(), 0);
        lv_obj_t* h = kit::text(r, tr(i18n::Str::setup_title), theme::h1(), theme::text());
        lv_obj_set_style_pad_left(h, 12, 0);
        lv_obj_set_style_margin_bottom(h, 24, 0);
        static const i18n::Str names[] = {i18n::Str::page_wifi, i18n::Str::settings_server, i18n::Str::setup_signin_title,
                                          i18n::Str::setup_step_speaker};
        for (int i = 1; i <= 4; ++i) {
            bool now = i == at, done = i < at;
            lv_obj_t* row = kit::row(r, 14);
            lv_obj_set_size(row, lv_pct(100), 64);
            lv_obj_set_style_pad_hor(row, 12, 0);
            lv_obj_set_style_radius(row, theme::r_control(), 0);
            lv_obj_set_style_bg_color(row, theme::elevated(), 0);
            lv_obj_set_style_bg_opa(row, now ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            // Marker: a check when done, the number otherwise (filled in the accent when current).
            lv_obj_t* m = lv_obj_create(row);
            lv_obj_remove_style_all(m);
            lv_obj_remove_flag(m, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(m, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_size(m, 32, 32);
            lv_obj_set_style_radius(m, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(m, now ? theme::accent() : theme::surface(), 0);
            lv_obj_set_style_bg_opa(m, LV_OPA_COVER, 0);
            if (done) {
                lv_obj_center(kit::icon(m, icon::check, 22, theme::text2()));
            } else {
                lv_obj_t* n = kit::text(m, std::to_string(i), theme::meta(), now ? theme::on_accent() : theme::text3());
                lv_obj_center(n);
            }
            kit::text(row, tr(names[i - 1]), theme::meta(), now ? theme::text() : done ? theme::text2() : theme::text3());
        }
    }

    void header(i18n::Str title, i18n::Str body, const std::string& title_arg = {}) {
        kit::text(pane_, title_arg.empty() ? std::string(tr(title)) : i18n::tr(title, title_arg), theme::display(), theme::text());
        if (body != i18n::Str::setup_title) {
            lv_obj_t* b = kit::text(pane_, tr(body), theme::lead(), theme::text2(), true);
            lv_obj_set_width(b, lv_pct(100));
            lv_obj_set_style_max_width(b, 720, 0);
        }
    }

    // The scrolling middle of the pane; the footer stays at the bottom.
    lv_obj_t* content() {
        lv_obj_t* c = kit::scroll_column(pane_, 24);
        lv_obj_set_width(c, lv_pct(100));
        lv_obj_set_flex_grow(c, 1);
        lv_obj_set_style_pad_top(c, 16, 0);
        return c;
    }

    void footer(SetupStep back, kit::Action next, bool next_enabled = true, i18n::Str next_label = i18n::Str::setup_continue) {
        lv_obj_t* f = kit::row(pane_, 16);
        lv_obj_set_width(f, lv_pct(100));
        kit::button(f, kit::Button::Secondary, icon::chevron_left, tr(i18n::Str::setup_back), [back] { go(back); });
        kit::spacer(f);
        if (next) {
            lv_obj_t* b = kit::button(f, kit::Button::Primary, nullptr, tr(next_label), next);
            if (!next_enabled) {
                lv_obj_add_state(b, LV_STATE_DISABLED);
                lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_bg_color(b, theme::elevated(), LV_STATE_DISABLED);
                lv_obj_set_style_text_color(lv_obj_get_child(b, 0), theme::text3(), 0);
            }
        }
    }

    // A tappable row in a group: icon, title, optional subtitle; `trailing` icon on the right.
    lv_obj_t* pick_row(lv_obj_t* card, const char* glyph, const std::string& title, const std::string& sub,
                       kit::Action action, const char* trailing = nullptr, lv_color_t trailing_colour = theme::text3()) {
        lv_obj_t* r = line(card, sub.empty() ? 64 : 72);
        lv_obj_set_style_bg_color(r, theme::elevated(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
        kit::icon(r, glyph, 24, theme::text2());
        grow_text(r, title, sub);
        if (trailing) kit::icon(r, trailing, 22, trailing_colour);
        kit::on_click(r, std::move(action));
        return r;
    }

    // A row that holds wrapped text (instructions, tips).
    lv_obj_t* text_row(lv_obj_t* card, const char* glyph, lv_color_t glyph_colour, const std::string& title,
                       const std::string& body = {}) {
        lv_obj_t* r = line(card, 0);
        lv_obj_set_style_pad_ver(r, 18, 0);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        if (glyph) kit::icon(r, glyph, 24, glyph_colour);
        lv_obj_t* col = kit::column(r, 4);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        lv_obj_t* t = kit::text(col, title, theme::meta(), theme::text(), true);
        lv_obj_set_width(t, lv_pct(100));
        if (!body.empty()) {
            lv_obj_t* b = kit::text(col, body, theme::label(), theme::text2(), true);
            lv_obj_set_width(b, lv_pct(100));
        }
        return r;
    }

    // A text field + keyboard along the bottom (the keyboard covers the footer while typing).
    void field(lv_obj_t* parent, const std::string& placeholder, bool password, i18n::Str done_key,
               std::function<void(std::string)> on_done) {
        input_ = lv_textarea_create(parent);
        lv_textarea_set_one_line(input_, true);
        lv_textarea_set_password_mode(input_, password);
        lv_textarea_set_placeholder_text(input_, placeholder.c_str());
        lv_obj_set_size(input_, lv_pct(100), 64);
        lv_obj_set_style_radius(input_, theme::r_control(), 0);
        lv_obj_set_style_bg_color(input_, theme::surface(), 0);
        lv_obj_set_style_border_width(input_, 2, 0);
        lv_obj_set_style_border_color(input_, theme::text3(), 0);  // 3:1 edge (WCAG 1.4.11): the field is the control
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
        kb_ = kit::keyboard(root_, input_, tr(done_key), tr(i18n::Str::key_space), finish);
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
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
        lv_obj_set_flex_align(pane_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(i18n::Str::setup_welcome_title, i18n::Str::setup_welcome_body);
        lv_obj_t* g = group(pane_, tr(i18n::Str::setup_need_title));
        lv_obj_set_style_margin_top(lv_obj_get_parent(g), 24, 0);
        lv_obj_set_style_max_width(lv_obj_get_parent(g), 720, 0);
        text_row(g, icon::wifi, theme::text2(), tr(i18n::Str::setup_need_wifi));
        text_row(g, icon::phone, theme::text2(), tr(i18n::Str::setup_need_phone));
        lv_obj_t* b = kit::button(pane_, kit::Button::Primary, nullptr, tr(i18n::Str::setup_get_started), [] { go(SetupStep::Wifi); });
        lv_obj_set_style_margin_top(b, 24, 0);
    }

    void wifi(const app::SetupView& v) {
        header(i18n::Str::setup_wifi_title, i18n::Str::setup_wifi_body);
        lv_obj_t* c = content();
        lv_obj_t* g = group(c, tr(v.scanning ? i18n::Str::setup_scanning : i18n::Str::setup_networks));
        for (const auto& n : v.networks) {
            std::string ssid = n.ssid;
            bool open = !n.secure;
            pick_row(g, icon::wifi, n.ssid, "", [this, ssid, open] {
                chosen_ssid_ = ssid;
                if (open) send(app::cmd::SetupJoinWifi{ssid, ""});
                else go(SetupStep::WifiPassword);
            }, n.secure ? icon::lock : nullptr);
        }
        action_row(g, icon::refresh, tr(i18n::Str::setup_scan_again), [] { send(app::cmd::SetupScanWifi{}); });
        footer(SetupStep::Welcome, {});
    }

    void password(const app::SetupView& v) {
        std::string ssid = v.ssid.empty() ? chosen_ssid_ : v.ssid;
        header(i18n::Str::setup_join, i18n::Str::setup_title, ssid);
        lv_obj_t* label = kit::overline(pane_, i18n::tr(i18n::Str::setup_password_for, ssid), theme::text3());
        lv_obj_set_style_margin_top(label, 16, 0);
        field(pane_, "", true, i18n::Str::setup_join_key, [ssid](std::string pw) { send(app::cmd::SetupJoinWifi{ssid, pw}); });
        if (v.joining) kit::text(pane_, tr(i18n::Str::setup_joining), theme::body(), theme::text2());
        if (v.wifi_error) {
            lv_obj_t* e = kit::text(pane_, tr(*v.wifi_error == app::WifiResult::WrongPassword ? i18n::Str::setup_wrong_password
                                                                                          : i18n::Str::setup_wifi_failed),
                                    theme::body(), theme::danger_text(), true);
            lv_obj_set_width(e, lv_pct(100));
        }
        kit::spacer(pane_);
        footer(SetupStep::Wifi, {});
        if (!v.joining) show_keyboard();
    }

    void server(const app::SetupView& v) {
        header(i18n::Str::setup_server_title, i18n::Str::setup_server_body);
        lv_obj_t* c = content();
        if (!v.searching && v.servers.empty()) banner(c, tr(i18n::Str::setup_no_server), tr(i18n::Str::setup_no_server_hint));
        lv_obj_t* g = group(c, tr(v.searching ? i18n::Str::setup_still_looking : i18n::Str::setup_found));
        for (const auto& s : v.servers) {
            std::string address = s.address;
            pick_row(g, icon::server, s.name, s.address, [address] { send(app::cmd::SetupUseServer{address}); }, icon::chevron_right);
        }
        if (!v.searching) action_row(g, icon::refresh, tr(i18n::Str::setup_search_again), [] { send(app::cmd::SetupFindServers{}); });
        lv_obj_t* label = kit::overline(c, tr(i18n::Str::setup_enter_address), theme::text3());
        lv_obj_set_style_pad_left(label, kRowPad, 0);
        lv_obj_set_style_margin_bottom(label, -14, 0);  // as close to its field as a group title to its card
        field(c, tr(i18n::Str::setup_address_example), false, i18n::Str::setup_connect_key,
              [](std::string a) { send(app::cmd::SetupUseServer{a}); });
        if (v.checking) kit::text(c, i18n::tr(i18n::Str::setup_checking, v.address), theme::body(), theme::text2());
        if (v.server_error) {
            lv_obj_t* e = kit::text(c, tr(i18n::Str::setup_server_error), theme::body(), theme::danger_text(), true);
            lv_obj_set_width(e, lv_pct(100));
        }
        footer(SetupStep::Wifi, {});
    }

    void sign_in(const app::SetupView& v) {
        header(i18n::Str::setup_signin_title, i18n::Str::setup_signin_body);
        bool phone = !v.pairing_url.empty() && !typing_;
        lv_obj_t* body = kit::row(pane_, 32);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_style_margin_top(body, 16, 0);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        if (phone) {
            lv_obj_t* qr_box = kit::column(body);
            lv_obj_set_style_pad_all(qr_box, 16, 0);
            lv_obj_set_style_radius(qr_box, theme::r_card(), 0);
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
        lv_obj_t* col = kit::column(body, 16);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        if (phone) {
            lv_obj_t* g = group(col, tr(i18n::Str::setup_use_phone));
            text_row(g, nullptr, theme::text2(), "1   " + i18n::tr(i18n::Str::setup_phone_1, v.pairing_url));
            text_row(g, nullptr, theme::text2(), "2   " + std::string(tr(i18n::Str::setup_phone_2)));
            text_row(g, nullptr, theme::text2(), "3   " + std::string(tr(i18n::Str::setup_phone_3)));
            action_row(g, icon::edit, tr(i18n::Str::setup_type_token), [this] {
                typing_ = true;
                kit::later([this] { refresh(Topic::Setup); });
            });
        }
        if (v.checking) kit::text(col, tr(i18n::Str::setup_checking_token), theme::body(), theme::text2());
        else if (v.token_error) banner(col, tr(i18n::Str::setup_token_rejected), tr(i18n::Str::setup_token_rejected_body));
        else if (phone) {
            lv_obj_t* w = kit::text(col, tr(i18n::Str::setup_waiting_token), theme::body(), theme::accent());
            lv_obj_set_style_pad_left(w, kRowPad, 0);
        }
        field(col, tr(i18n::Str::setup_token_placeholder), false, i18n::Str::setup_signin_key,
              [](std::string token) { send(app::cmd::SetupToken{token}); });
        if (phone) lv_obj_add_flag(input_, LV_OBJ_FLAG_HIDDEN);
        else show_keyboard();
        kit::spacer(pane_);
        footer(SetupStep::Server, {});
    }

    void speaker() {
        typing_ = false;
        header(i18n::Str::setup_speaker_title, i18n::Str::setup_speaker_body);
        lv_obj_t* c = content();
        const auto* pv = state().players.get();
        if (!pv) kit::text(c, tr(i18n::Str::list_loading), theme::lead(), theme::text2());
        bool any = pv && !pv->selected_id.empty();
        lv_obj_t* g = pv ? group(c, {}) : nullptr;
        for (size_t i = 0; pv && i < pv->players.size(); ++i) {
            const auto& p = pv->players[i];
            std::string id = p.player_id;
            std::string sub = !p.available ? tr(i18n::Str::setup_offline)
                            : p.group_members.empty() ? tr(i18n::Str::setup_available) : tr(i18n::Str::setup_group);
            bool selected = id == pv->selected_id;
            pick_row(g, icon::speaker, p.name, sub, [id] { send(app::cmd::SelectPlayer{id}); },
                     selected ? icon::check : nullptr, theme::accent());
        }
        footer(SetupStep::SignIn, [] { go(SetupStep::Done); }, any);
    }

    void done() {
        lv_obj_set_flex_align(pane_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        header(i18n::Str::setup_done_title, i18n::Str::setup_title);
        const core::PlayerState* p = state().selected();
        if (p) kit::text(pane_, i18n::tr(i18n::Str::queue_playing_on, p->name), theme::lead(), theme::text2());
        lv_obj_t* g = group(pane_, {});
        lv_obj_set_style_margin_top(lv_obj_get_parent(g), 24, 0);
        lv_obj_set_style_max_width(lv_obj_get_parent(g), 720, 0);
        struct Tip { const char* glyph; i18n::Str title, body; };
        for (Tip t : {Tip{icon::heart, i18n::Str::setup_tip1, i18n::Str::setup_tip1_body},
                      Tip{icon::search, i18n::Str::setup_tip2, i18n::Str::setup_tip2_body},
                      Tip{icon::moon, i18n::Str::setup_tip3, i18n::Str::setup_tip3_body}})
            text_row(g, t.glyph, theme::accent(), tr(t.title), tr(t.body));
        lv_obj_t* b = kit::button(pane_, kit::Button::Primary, icon::play, tr(i18n::Str::setup_start), [] { send(app::cmd::SetupFinish{}); });
        lv_obj_set_style_margin_top(b, 24, 0);
    }

    lv_obj_t* root_ = nullptr;
    lv_obj_t* pane_ = nullptr;
    lv_obj_t* input_ = nullptr;
    lv_obj_t* kb_ = nullptr;
    std::string chosen_ssid_;
    bool typing_ = false;
    SetupStep shown_step_ = SetupStep::Finished;
};

}  // namespace

std::unique_ptr<Screen> make_setup() { return std::make_unique<Setup>(); }

}  // namespace ui
