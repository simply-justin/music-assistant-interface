// Search: one field, results grouped by media type (never by provider), media-type chips,
// on-screen keyboard. Every keystroke goes to core, which debounces.
#include "../shell.hpp"

namespace ui {
namespace {

enum class Filter : uint8_t { All, Tracks, Artists, Albums, Playlists, Radio };

class Search : public Screen {
public:
    void build(lv_obj_t* root) override {
        lv_obj_set_style_pad_hor(root, theme::kPad, 0);
        lv_obj_set_style_pad_top(root, 20, 0);
        lv_obj_set_style_pad_row(root, 8, 0);

        lv_obj_t* field = kit::row(root, 16);
        lv_obj_set_size(field, lv_pct(100), 52);
        lv_obj_set_style_radius(field, theme::r_control(), 0);
        lv_obj_set_style_pad_left(field, 20, 0);
        lv_obj_set_style_pad_right(field, 8, 0);
        lv_obj_set_style_bg_color(field, theme::field_bg(), 0);
        lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(field, theme::light() ? 1 : 0, 0);
        lv_obj_set_style_border_color(field, theme::border(), 0);
        kit::icon(field, icon::search, 22, lv_color_hex(0x666666));
        input_ = lv_textarea_create(field);
        lv_obj_remove_style_all(input_);
        lv_textarea_set_one_line(input_, true);
        lv_textarea_set_placeholder_text(input_, tr(i18n::Str::search_placeholder));
        lv_obj_set_width(input_, 1);  // grows into the row; never wider than the space before ×
        lv_obj_set_flex_grow(input_, 1);
        lv_obj_set_style_pad_right(input_, 12, 0);  // room for the cursor after the last letter (it was clipped)
        lv_obj_set_style_pad_ver(input_, 4, 0);     // descenders (g, y, p) stay inside the field
        lv_obj_set_style_text_font(input_, theme::body(), 0);
        lv_obj_set_style_text_color(input_, theme::field_text(), 0);
        lv_obj_set_style_text_color(input_, lv_color_hex(0x6B6B6B), LV_PART_TEXTAREA_PLACEHOLDER);
        lv_obj_set_style_border_width(input_, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_side(input_, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(input_, theme::accent(), LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_add_event_cb(input_, [](lv_event_t* e) {
            auto* self = static_cast<Search*>(lv_event_get_user_data(e));
            send(app::cmd::Search{lv_textarea_get_text(self->input_)});
        }, LV_EVENT_VALUE_CHANGED, this);
        kit::on_click(input_, [this] { show_keyboard(true); });
        kit::on_click(field, [this] { show_keyboard(true); });
        kit::icon_button(field, icon::close, [this] {
            lv_textarea_set_text(input_, "");
            show_keyboard(true);
        }, 44, 22, false, lv_color_hex(0x666666));

        chips_ = kit::row(root, 12);
        status_ = kit::text(root, "", theme::label(), theme::text2());
        results_ = kit::scroll_column(root, 18);
        lv_obj_set_width(results_, lv_pct(100));
        lv_obj_set_flex_grow(results_, 1);
        lv_obj_set_style_pad_bottom(results_, 24, 0);
        // Touching or scrolling the results puts the keyboard away (the field brings it back).
        auto hide = [](lv_event_t* e) { static_cast<Search*>(lv_event_get_user_data(e))->show_keyboard(false); };
        lv_obj_add_event_cb(results_, hide, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(results_, hide, LV_EVENT_SCROLL_BEGIN, this);
        build_keyboard(root);
    }

    void on_open() override { show_keyboard(lv_textarea_get_text(input_)[0] == '\0'); }

    void refresh(Topic t) override {
        if (t != Topic::All && t != Topic::Search) return;
        build_chips();
        build_results();
    }

    ScreenId tab() const override { return ScreenId::Search; }
    std::string title() const override { return tr(i18n::Str::nav_search); }

private:
    void build_keyboard(lv_obj_t* root) {
        kb_ = kit::keyboard(root, input_, tr(i18n::Str::key_search), tr(i18n::Str::key_space),
                            [this] { show_keyboard(false); });
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width(kb_, theme::kScreenW - theme::kSideW);
        lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
        show_keyboard(false);
    }

    void show_keyboard(bool on) {
        if (on) {
            lv_obj_remove_flag(kb_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_state(input_, LV_STATE_FOCUSED);
            lv_obj_set_style_pad_bottom(results_, 316, 0);
        } else {
            lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_state(input_, LV_STATE_FOCUSED);
            lv_obj_set_style_pad_bottom(results_, 24, 0);
        }
    }

    // ---- Results
    void build_chips() {
        lv_obj_clean(chips_);
        struct C { Filter f; i18n::Str s; };
        for (C c : {C{Filter::All, i18n::Str::filter_all}, C{Filter::Tracks, i18n::Str::filter_tracks},
                    C{Filter::Artists, i18n::Str::filter_artists}, C{Filter::Albums, i18n::Str::filter_albums},
                    C{Filter::Playlists, i18n::Str::filter_playlists}, C{Filter::Radio, i18n::Str::filter_radio}}) {
            Filter f = c.f;
            kit::tab(chips_, tr(c.s), filter_ == f, [this, f] {
                filter_ = f;
                kit::later([this] { refresh(Topic::Search); });
            });
        }
    }

    void section(const char* title) { kit::text(results_, title, theme::lead(), theme::text()); }

    void track_rows(lv_obj_t* parent, const std::vector<core::MediaItem>& tracks, size_t max) {
        for (size_t i = 0; i < tracks.size() && i < max; ++i) {
            const auto& t = tracks[i];
            kit::RowOptions o;
            o.on_tap = [t] { play(t); };
            o.on_more = [t] { open_track_menu(t); };
            kit::media_row(parent, t, o);
        }
    }

    void card_row(const std::vector<core::MediaItem>& items, size_t max, bool wrap) {
        lv_obj_t* r = kit::row(results_, 16);
        lv_obj_set_width(r, lv_pct(100));
        if (wrap) {
            lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
            lv_obj_set_style_pad_row(r, 28, 0);
        }
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        for (size_t i = 0; i < items.size() && i < max; ++i) {
            const auto& it = items[i];
            kit::media_card(r, it, kit::subtitle(it), [it] { open_item(it); }, it.type == core::MediaType::Artist ? 96 : 112);
        }
    }

    void build_results() {
        lv_obj_clean(results_);
        const core::SearchView* v = state().search.get();
        if (!v || v->query.empty()) {
            lv_label_set_text(status_, "");
            kit::text(results_, tr(i18n::Str::search_hint), theme::lead(), theme::text2(), true);
            return;
        }
        const auto& r = v->result;
        switch (v->status) {
            case core::SearchStatus::Loading: lv_label_set_text(status_, tr(i18n::Str::search_loading)); break;
            case core::SearchStatus::Partial: lv_label_set_text(status_, tr(i18n::Str::search_partial)); break;
            case core::SearchStatus::Error: lv_label_set_text(status_, tr(i18n::Str::search_error)); break;
            default: lv_label_set_text(status_, v->failed_providers.empty() ? "" : tr(i18n::Str::search_some_failed)); break;
        }
        if (v->status == core::SearchStatus::Done && r.empty()) {
            kit::text(results_, i18n::tr(i18n::Str::search_no_results, v->query), theme::h2(), theme::text());
            return;
        }

        constexpr size_t kAll = 300;  // cards are capped; long track lists are windowed
        switch (filter_) {
            case Filter::Tracks: {
                auto tracks = std::make_shared<std::vector<core::MediaItem>>(r.tracks);
                kit::virtual_list(results_, tracks->size(), 72, 0, [tracks](lv_obj_t* list, size_t i) {
                    const auto& t = (*tracks)[i];
                    kit::RowOptions o;
                    o.on_tap = [t] { play(t); };
                    o.on_more = [t] { open_track_menu(t); };
                    return kit::media_row(list, t, o);
                });
                return;
            }
            case Filter::Artists: card_row(r.artists, kAll, true); return;
            case Filter::Albums: card_row(r.albums, kAll, true); return;
            case Filter::Playlists: card_row(r.playlists, kAll, true); return;
            case Filter::Radio: card_row(r.radio, kAll, true); return;
            case Filter::All: break;
        }

        // All: Top result + first tracks side by side, then a shelf per type.
        lv_obj_t* top = kit::row(results_, 24);
        lv_obj_set_width(top, lv_pct(100));
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        if (r.top) {
            lv_obj_t* tc = kit::column(top, 8);
            lv_obj_set_width(tc, 248);
            kit::text(tc, tr(i18n::Str::search_top_result), theme::lead(), theme::text());
            lv_obj_t* card = kit::column(tc, 12);
            lv_obj_set_width(card, 248);
            lv_obj_set_style_pad_all(card, 16, 0);
            lv_obj_set_style_radius(card, theme::r_card(), 0);
            lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
            lv_color_t tint = theme::art_tint(theme::art_colour(r.top->uri.c_str()));
            lv_color_t card_bg = theme::light() ? lv_color_mix(lv_color_black(), tint, 12) : lv_color_mix(lv_color_white(), tint, 12);
            lv_obj_set_style_bg_color(card, card_bg, 0);
            kit::art(card, *r.top, 88, r.top->type == core::MediaType::Artist ? 44 : theme::r_art(), card_bg);
            lv_obj_t* name = kit::text(card, r.top->name, theme::h2(), theme::text());
            lv_obj_set_width(name, lv_pct(100));
            lv_obj_t* sub = kit::row(card, 6);
            kit::text(sub, kit::subtitle(*r.top), theme::label(), theme::text());  // art-tinted card: main colour only
            kit::badge(sub, r.top->badge_provider());
            lv_obj_t* pb = kit::play_button(card, false, [] {}, 52);
            lv_obj_add_flag(pb, LV_OBJ_FLAG_IGNORE_LAYOUT);
            lv_obj_align(pb, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
            auto t = *r.top;
            kit::on_click(pb, [t] { play(t); });
            kit::press_scale(card);
            kit::on_click(card, [t] { open_item(t); });
        }
        if (!r.tracks.empty()) {
            lv_obj_t* tracks = kit::column(top, 8);
            lv_obj_set_flex_grow(tracks, 1);
            kit::text(tracks, tr(i18n::Str::filter_tracks), theme::lead(), theme::text());
            lv_obj_t* list = kit::column(tracks, 0);
            lv_obj_set_width(list, lv_pct(100));
            track_rows(list, r.tracks, 5);
        }
        std::vector<core::MediaItem> albums_playlists = r.albums;
        albums_playlists.insert(albums_playlists.end(), r.playlists.begin(), r.playlists.end());
        if (!albums_playlists.empty()) { section(tr(i18n::Str::search_albums_playlists)); card_row(albums_playlists, 6, false); }
        if (!r.artists.empty()) { section(tr(i18n::Str::filter_artists)); card_row(r.artists, 6, false); }
        if (!r.radio.empty()) { section(tr(i18n::Str::filter_radio)); card_row(r.radio, 6, false); }
    }

    lv_obj_t *input_ = nullptr, *chips_ = nullptr, *status_ = nullptr, *results_ = nullptr, *kb_ = nullptr;
    Filter filter_ = Filter::All;
};

}  // namespace

std::unique_ptr<Screen> make_search() { return std::make_unique<Search>(); }

}  // namespace ui
