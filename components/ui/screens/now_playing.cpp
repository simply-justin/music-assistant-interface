// Now Playing (direction A): art-coloured page, big cover + title on the left, "Up next" on the
// right. Transport, progress and volume live in the player bar below, so they are not repeated.
#include "../shell.hpp"
#include "../text.hpp"

namespace ui {
namespace {

class NowPlaying : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(root, theme::bg(), 0);
    }

    void refresh(Topic t) override {
        if (t == Topic::Progress || t == Topic::Search || t == Topic::Browse || t == Topic::Setup) return;
        rebuild();
    }

    bool full_screen() const override { return true; }
    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::np_title); }

private:
    void rebuild() {
        lv_obj_clean(root_);
        auto item = state().now_playing();
        const core::PlayerState* p = state().selected();

        lv_obj_t* header = kit::row(root_, 12);
        lv_obj_set_size(header, lv_pct(100), 80);
        lv_obj_set_style_pad_hor(header, 24, 0);
        kit::icon_button(header, icon::chevron_down, [] { back(); }, 56, 32);
        kit::spacer(header);
        lv_obj_t* spk = kit::chip(header, icon::speaker, p ? p->name : tr(i18n::Str::speaker_none), false, [] { open_speakers(); });
        lv_obj_set_style_border_color(spk, theme::light() ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_opa(spk, LV_OPA_30, 0);
        if (item) {
            auto it = *item;
            kit::icon_button(header, icon::more, [it] { open_track_menu(it); }, 56, 28);
        }

        if (!item) {
            lv_obj_set_style_bg_color(root_, theme::bg(), 0);
            lv_obj_t* empty = kit::column(root_, 12);
            lv_obj_set_style_pad_all(empty, 80, 0);
            kit::text(empty, tr(i18n::Str::home_nothing_playing), theme::h1(), theme::text());
            kit::text(empty, tr(i18n::Str::home_pick_favourite), theme::lead(), theme::text2());
            return;
        }
        lv_color_t tint = theme::art_tint(theme::art_colour(item->uri.c_str()));
        lv_obj_set_style_bg_color(root_, tint, 0);
        bool radio = item->type == core::MediaType::Radio;

        const auto* q = state().queue.get();
        if (q && q->current && !q->current->available) {
            lv_obj_t* wrap = kit::row(root_);
            lv_obj_set_width(wrap, lv_pct(100));
            lv_obj_set_style_pad_hor(wrap, 56, 0);
            lv_obj_t* b = banner(wrap, tr(i18n::Str::np_error_title),
                                 i18n::tr(i18n::Str::np_error_body, kit::provider_name(item->badge_provider())));
            kit::button(b, kit::Button::Secondary, icon::next, tr(i18n::Str::np_skip), [] { send(app::cmd::Next{}); });
        }

        lv_obj_t* body = kit::row(root_, 56);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_flex_grow(body, 1);
        lv_obj_set_style_pad_hor(body, 56, 0);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        // Left: cover + what it is. The background follows the real artwork once decoded.
        lv_obj_t* left = kit::column(body, 16);
        lv_obj_set_width(left, 440);
        lv_obj_t* root = root_;
        kit::art(left, *item, 440, theme::r_art(), tint, [root](uint32_t avg) {
            lv_obj_set_style_bg_color(root, theme::art_tint(lv_color_hex(avg)), 0);
        });
        lv_obj_t* titles = kit::row(left, 12);
        lv_obj_set_width(titles, lv_pct(100));
        lv_obj_t* col = kit::column(titles, 4);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_width(col, 1);
        if (radio) kit::tag(col, tr(i18n::Str::np_live), theme::text());
        kit::text(col, item->name, theme::h2(), theme::text());
        std::string sub = radio && p && !p->media_title.empty() ? p->media_title + " · " + p->media_artist : kit::subtitle(*item);
        kit::text(col, sub, theme::meta(), theme::text2());
        auto it = *item;
        kit::icon_button(titles, it.favorite ? icon::heart_filled : icon::heart,
                         [it] { send(app::cmd::SetFavourite{it, !it.favorite}); }, 56, 28, false, theme::text());

        // Right: up next.
        lv_obj_t* right = kit::column(body, 4);
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_width(right, 1);
        lv_obj_set_height(right, 440 + 80);
        lv_obj_t* head = kit::row(right, 12);
        lv_obj_set_width(head, lv_pct(100));
        lv_obj_set_style_margin_bottom(head, 8, 0);
        lv_obj_t* ht = kit::text(head, tr(radio ? i18n::Str::np_live_radio : i18n::Str::np_up_next), theme::h2(), theme::text());
        lv_obj_set_flex_grow(ht, 1);
        if (!radio) {
            bool shuffle = q && q->shuffle;
            kit::chip(head, icon::shuffle, tr(i18n::Str::queue_shuffle), shuffle, [shuffle] { send(app::cmd::SetShuffle{!shuffle}); });
        }
        if (radio) {
            kit::text(right, tr(i18n::Str::np_radio_hint), theme::meta(), theme::text2(), true);
            return;
        }
        // The current song first (highlighted, equalizer), then what comes next.
        kit::RowOptions cur;
        cur.playing = true;
        kit::media_row(right, *item, cur);
        size_t first = q && q->current_index >= 0 ? static_cast<size_t>(q->current_index) + 1 : 0;
        size_t shown = 0;
        for (size_t i = first; q && i < q->items.size() && shown < 6; ++i, ++shown) {
            const core::QueueItem& qi = q->items[i];
            core::MediaItem m = qi.media ? *qi.media : core::MediaItem{};
            if (!qi.media) m.name = qi.name;
            int index = static_cast<int>(i);
            kit::RowOptions o;
            o.on_tap = [index] { send(app::cmd::PlayQueueIndex{index}); };
            kit::media_row(right, m, o);
        }
        if (!shown) kit::text(right, tr(i18n::Str::queue_empty), theme::meta(), theme::text2(), true);
    }

    lv_obj_t* root_ = nullptr;
};

}  // namespace

std::unique_ptr<Screen> make_now_playing() { return std::make_unique<NowPlaying>(); }

}  // namespace ui
