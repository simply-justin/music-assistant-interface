// Now Playing: full screen, flat art-derived background, large artwork and controls. Controls the
// media or player can't do are hidden (radio: no seek, prev/next, shuffle, repeat).
#include "../shell.hpp"
#include "../text.hpp"

namespace ui {
namespace {

// Art-derived background clamped to HSL lightness ≤ 16 % and saturation ≤ 45 % (spec §1).
lv_color_t background(lv_color_t art) { return lv_color_hex(img::art_background(lv_color_to_u32(art) & 0xFFFFFF)); }

class NowPlaying : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(root, theme::bg(), 0);
    }

    void refresh(Topic t) override {
        if (t == Topic::Progress) return update_progress();
        if (t == Topic::Search || t == Topic::Browse || t == Topic::Playlists) return;
        rebuild();
    }

    bool full_screen() const override { return true; }
    bool mini_player() const override { return false; }
    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::np_title); }

private:
    void update_progress() {
        if (!seek_) return;
        if (!lv_obj_has_state(seek_, LV_STATE_PRESSED)) lv_slider_set_value(seek_, static_cast<int32_t>(state().elapsed_s), LV_ANIM_OFF);
        lv_label_set_text(elapsed_, duration(state().elapsed_s).c_str());
    }

    void rebuild() {
        lv_obj_clean(root_);
        seek_ = elapsed_ = nullptr;
        auto item = state().now_playing();
        const core::PlayerState* p = state().selected();

        lv_obj_t* header = kit::row(root_, 16);
        lv_obj_set_size(header, lv_pct(100), 88);
        lv_obj_set_style_pad_hor(header, 32, 0);
        lv_obj_set_style_pad_top(header, 16, 0);
        kit::icon_button(header, icon::chevron_down, [] { back(); }, 64, 32, true);
        kit::spacer(header);
        speaker_chip(header);
        if (item) {
            auto it = *item;
            kit::icon_button(header, icon::more, [it] { open_track_menu(it); });
        }

        if (!item) {
            lv_obj_set_style_bg_color(root_, theme::bg(), 0);
            lv_obj_t* empty = kit::column(root_, 12);
            lv_obj_set_style_pad_all(empty, 80, 0);
            kit::text(empty, tr(i18n::Str::home_nothing_playing), theme::h1(), theme::text());
            kit::text(empty, tr(i18n::Str::home_pick_favourite), theme::lead(), theme::text2());
            return;
        }
        lv_color_t bg = background(theme::art_colour(item->uri.c_str()));
        lv_obj_set_style_bg_color(root_, bg, 0);
        bool radio = item->type == core::MediaType::Radio;
        // Playback error: shown only when Music Assistant marks the current item unavailable
        // (QueueItem.available, verified 2.10). Open question 2: a richer error signal, if any.
        const auto* q = state().queue.get();
        if (q && q->current && !q->current->available) {
            lv_obj_t* wrap = kit::row(root_);
            lv_obj_set_width(wrap, lv_pct(100));
            lv_obj_set_style_pad_hor(wrap, 80, 0);
            lv_obj_t* b = banner(wrap, tr(i18n::Str::np_error_title),
                                 i18n::tr(i18n::Str::np_error_body, kit::provider_name(item->badge_provider())));
            kit::button(b, kit::Button::Secondary, icon::next, tr(i18n::Str::np_skip), [] { send(app::cmd::Next{}); });
        }

        lv_obj_t* body = kit::row(root_, 72);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_flex_grow(body, 1);
        lv_obj_set_style_pad_hor(body, 80, 0);
        lv_obj_set_style_pad_bottom(body, 40, 0);
        // The art-derived background follows the real artwork once it's decoded.
        lv_obj_t* root = root_;
        kit::art(body, *item, 520, theme::kRadiusCard, bg, [root](uint32_t avg) {
            lv_obj_set_style_bg_color(root, background(lv_color_hex(avg)), 0);
        });

        lv_obj_t* col = kit::column(body, 32);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);

        lv_obj_t* info = kit::column(col, 10);
        lv_obj_set_width(info, lv_pct(100));
        if (radio) kit::tag(info, tr(i18n::Str::np_live), lv_color_hex(0xFFFFFF));
        kit::text(info, item->name, theme::display(), theme::text());
        // Radio: the stream's own "now on air" metadata comes from the player.
        std::string sub = radio && p && !p->media_title.empty() ? p->media_title + " · " + p->media_artist : kit::subtitle(*item);
        kit::text(info, sub, theme::h2(), theme::text2());
        lv_obj_t* src = kit::row(info, 10);
        if (item->album) kit::text(src, item->album->name, theme::body(), theme::text2());
        kit::badge(src, item->badge_provider());

        if (!radio && item->duration_s && (!p || p->can_seek)) {
            lv_obj_t* prog = kit::column(col, 10);
            lv_obj_set_width(prog, lv_pct(100));
            seek_ = kit::slider(prog, 0, [](int v) { send(app::cmd::Seek{v}); }, theme::text());
            lv_obj_set_width(seek_, lv_pct(100));
            lv_slider_set_range(seek_, 0, static_cast<int32_t>(item->duration_s));
            lv_obj_t* times = kit::row(prog);
            lv_obj_set_width(times, lv_pct(100));
            elapsed_ = kit::text(times, "", theme::meta(), theme::text2());
            kit::spacer(times);
            kit::text(times, duration(item->duration_s), theme::meta(), theme::text2());
            update_progress();
        }

        lv_obj_t* controls = kit::row(col, 0);
        lv_obj_set_width(controls, lv_pct(100));
        lv_obj_set_flex_align(controls, radio ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        bool prev_next = !radio && (!p || p->can_next_previous);
        if (!radio) {
            bool on = q && q->shuffle;
            kit::icon_button(controls, icon::shuffle, [on] { send(app::cmd::SetShuffle{!on}); }, 56, 28, false,
                             on ? theme::accent() : theme::text2());
        }
        if (prev_next) kit::icon_button(controls, icon::prev, [] { send(app::cmd::Previous{}); }, 80, 48);
        if (radio) {
            lv_obj_t* stop = kit::icon_button(controls, icon::stop, [] { send(app::cmd::PlayPause{}); }, 112, 48, true,
                                              theme::on_accent());
            lv_obj_set_style_bg_color(stop, theme::accent(), 0);
        } else {
            kit::play_button(controls, state().playing(), [] { send(app::cmd::PlayPause{}); }, 112);
        }
        if (prev_next) kit::icon_button(controls, icon::next, [] { send(app::cmd::Next{}); }, 80, 48);
        if (!radio) {
            core::RepeatMode mode = q ? q->repeat : core::RepeatMode::Off;
            core::RepeatMode next = mode == core::RepeatMode::Off ? core::RepeatMode::All
                                  : mode == core::RepeatMode::All ? core::RepeatMode::One
                                                                  : core::RepeatMode::Off;
            kit::icon_button(controls, mode == core::RepeatMode::One ? icon::repeat_one : icon::repeat,
                             [next] { send(app::cmd::SetRepeat{next}); }, 56, 28, false,
                             mode == core::RepeatMode::Off ? theme::text2() : theme::accent());
        }

        if (!p || p->can_set_volume) {
            lv_obj_t* vol = kit::row(col, 16);
            lv_obj_set_width(vol, lv_pct(100));
            kit::icon(vol, icon::volume, 28, theme::text2());
            std::string id = p ? p->player_id : "";
            lv_obj_t* s = kit::slider(vol, p ? p->volume_level : 0, [id](int v) { send(app::cmd::SetVolume{id, v}); }, theme::text2());
            lv_obj_set_flex_grow(s, 1);
        }

        lv_obj_t* bottom = kit::row(col, 12);
        lv_obj_set_width(bottom, lv_pct(100));
        auto it = *item;
        kit::icon_button(bottom, it.favorite ? icon::heart_filled : icon::heart,
                         [it] { send(app::cmd::SetFavourite{it, !it.favorite}); }, 56, 28, true,
                         it.favorite ? theme::accent() : theme::text());
        kit::spacer(bottom);
        if (q && q->current_index >= 0 && static_cast<size_t>(q->current_index) + 1 < q->items.size()) {
            const auto& next_item = q->items[q->current_index + 1];
            kit::chip(bottom, icon::queue, std::string(tr(i18n::Str::np_up_next)) + ": " + next_item.name, false,
                      [] { open(ScreenId::Queue); });
        }
    }

    lv_obj_t* root_ = nullptr;
    lv_obj_t* seek_ = nullptr;
    lv_obj_t* elapsed_ = nullptr;
};

}  // namespace

std::unique_ptr<Screen> make_now_playing() { return std::make_unique<NowPlaying>(); }

}  // namespace ui
