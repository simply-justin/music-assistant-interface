// Home (direction A): greeting + filter, 8 one-tap shortcut tiles, "Now on <speaker>",
// recently played, your stations. A flat tint from the current artwork sits behind the top.
#include <ctime>

#include "../shell.hpp"

namespace ui {
namespace {

using Kind = app::cmd::BrowseKind;
enum class Filter : uint8_t { All, Music, Radio };

class Home : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_pad_hor(root, theme::kPad, 0);
        lv_obj_set_style_pad_top(root, 20, 0);
        lv_obj_set_style_pad_row(root, 14, 0);
    }

    void on_open() override {
        send(app::cmd::Browse{Kind::Favourites, {}});
        send(app::cmd::Browse{Kind::RecentlyPlayed, {}});
        send(app::cmd::Browse{Kind::LibraryRadio, {}});
    }

    void refresh(Topic t) override {
        if (t == Topic::Progress) {
            if (progress_) lv_bar_set_value(progress_, static_cast<int32_t>(state().elapsed_s), LV_ANIM_ON);
            return;
        }
        if (t == Topic::Search || t == Topic::Setup) return;
        rebuild();
    }

    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::nav_home); }

private:
    void rebuild() {
        lv_obj_clean(root_);
        progress_ = nullptr;
        auto now = state().now_playing();
        lv_color_t art = now ? theme::art_colour(now->uri.c_str()) : theme::surface();

        // Flat tint from the current artwork behind the top of the page (follows the music).
        band_ = lv_obj_create(root_);
        lv_obj_remove_style_all(band_);
        lv_obj_add_flag(band_, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_pos(band_, -theme::kPad, -20);
        lv_obj_set_size(band_, theme::kScreenW - theme::kSideW, 20 + 48 + 14 + 120 + 24);  // header + tiles + margin
        lv_obj_set_style_bg_color(band_, now ? theme::art_tint(art) : theme::bg(), 0);
        lv_obj_set_style_bg_opa(band_, LV_OPA_COVER, 0);

        header();
        shortcuts();
        middle(now);
        stations();
    }

    void header() {
        lv_obj_t* h = kit::row(root_, 10);
        lv_obj_set_size(h, lv_pct(100), 48);
        std::time_t now = std::time(nullptr);
        int hour = std::localtime(&now)->tm_hour;
        auto greeting = hour < 12 ? i18n::Str::home_morning : hour < 18 ? i18n::Str::home_afternoon : i18n::Str::home_evening;
        lv_obj_t* g = kit::text(h, tr(greeting), theme::h1(), theme::text());
        lv_obj_set_style_margin_right(g, 18, 0);
        struct F { Filter f; i18n::Str s; };
        for (F x : {F{Filter::All, i18n::Str::filter_all}, F{Filter::Music, i18n::Str::home_music}, F{Filter::Radio, i18n::Str::filter_radio}}) {
            Filter f = x.f;
            kit::pill(h, tr(x.s), filter_ == f, [this, f] {
                filter_ = f;
                kit::later([this] { rebuild(); });
            });
        }
        kit::spacer(h);
        if (state().ever_connected && !state().connected())
            kit::text(h, tr(i18n::Str::home_reconnecting), theme::meta(), theme::warning());
    }

    // 4 × 2 one-tap favourites; long-press opens the preview.
    void shortcuts() {
        lv_obj_t* grid = lv_obj_create(root_);
        lv_obj_remove_style_all(grid);
        lv_obj_set_size(grid, lv_pct(100), 56 * 2 + 8);
        static const int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        static const int32_t rows[] = {56, 56, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_grid_dsc_array(grid, cols, rows);
        lv_obj_set_style_pad_row(grid, 8, 0);
        lv_obj_set_style_pad_column(grid, 12, 0);
        const auto* favs = state().list(Kind::Favourites);
        int n = 0;
        for (size_t i = 0; favs && i < favs->items.size() && n < 8; ++i) {
            const auto& f = favs->items[i];
            bool radio = f.type == core::MediaType::Radio;
            if ((filter_ == Filter::Music && radio) || (filter_ == Filter::Radio && !radio)) continue;
            lv_obj_t* t = kit::favourite_tile(grid, f, [f] { play(f); }, [f] { open_favourite_preview(f); });
            lv_obj_set_grid_cell(t, LV_GRID_ALIGN_STRETCH, n % 4, 1, LV_GRID_ALIGN_STRETCH, n / 4, 1);
            ++n;
        }
        if (n == 0) {
            lv_obj_t* hint = kit::text(grid, tr(i18n::Str::home_no_favourites), theme::meta(), theme::text2(), true);
            lv_obj_set_grid_cell(hint, LV_GRID_ALIGN_START, 0, 4, LV_GRID_ALIGN_CENTER, 0, 2);
        }
    }

    void middle(const std::optional<core::MediaItem>& now) {
        lv_obj_t* r = kit::row(root_, 28);
        lv_obj_set_width(r, lv_pct(100));
        lv_obj_set_style_margin_top(r, 6, 0);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        // "Now on <speaker>": a card in the artwork's colour.
        lv_obj_t* left = kit::column(r, 8);
        lv_obj_set_width(left, 304);
        const core::PlayerState* p = state().selected();
        kit::section_header(left, i18n::tr(i18n::Str::home_now_on, p ? p->name : ""));
        lv_obj_t* card = kit::column(left, 0);
        lv_obj_set_size(card, 304, 180);
        lv_obj_set_style_pad_all(card, 16, 0);
        lv_obj_set_style_radius(card, theme::r_card(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_color_t tint = now ? theme::art_tint(theme::art_colour(now->uri.c_str())) : theme::surface();
        lv_color_t card_bg = theme::light() ? lv_color_mix(lv_color_black(), tint, 12) : lv_color_mix(lv_color_white(), tint, 12);
        lv_obj_set_style_bg_color(card, card_bg, 0);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        kit::press_scale(card);
        kit::on_click(card, [] { open(ScreenId::NowPlaying); });
        if (!now) {
            kit::text(card, tr(i18n::Str::home_nothing_playing), theme::body(), theme::text());
            kit::text(card, tr(i18n::Str::home_pick_favourite), theme::meta(), theme::text(), true);
        } else {
            lv_obj_t* top = kit::row(card, 14);
            lv_obj_set_width(top, lv_pct(100));
            kit::art(top, *now, 80, theme::r_art(), card_bg);
            lv_obj_t* info = kit::column(top, 4);
            lv_obj_set_flex_grow(info, 1);
            lv_obj_set_width(info, 1);
            lv_obj_t* ov = kit::row(info, 8);
            if (state().playing()) kit::equalizer(ov, theme::text(), true);
            // Only the main text colour on art-tinted cards: grey can drop below 4.5:1 there.
            kit::overline(ov, tr(state().playing() ? i18n::Str::home_now_playing : i18n::Str::home_paused), theme::text());
            kit::text(info, now->name, theme::body(), theme::text());
            kit::text(info, kit::subtitle(*now), theme::label(), theme::text());
            lv_obj_t* bottom = kit::row(card, 12);
            lv_obj_set_width(bottom, lv_pct(100));
            kit::play_button(bottom, state().playing(), [] { send(app::cmd::PlayPause{}); }, 52);
            if (now->duration_s) {
                progress_ = lv_bar_create(bottom);
                lv_obj_set_flex_grow(progress_, 1);
                lv_obj_set_height(progress_, 4);
                lv_obj_set_style_bg_color(progress_, theme::text(), LV_PART_INDICATOR);
                lv_obj_set_style_bg_color(progress_, theme::text(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(progress_, LV_OPA_20, LV_PART_MAIN);
                lv_obj_set_style_anim_duration(progress_, 1000, 0);
                lv_bar_set_range(progress_, 0, static_cast<int32_t>(now->duration_s));
                lv_bar_set_value(progress_, static_cast<int32_t>(state().elapsed_s), LV_ANIM_OFF);
            }
        }

        // Recently played (or your playlists if nothing was played yet).
        lv_obj_t* right = kit::column(r, 8);
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_width(right, 1);
        kit::section_header(right, tr(i18n::Str::home_recently_played), tr(i18n::Str::home_see_all), [] { open(ScreenId::Library); });
        lv_obj_t* cards = kit::row(right, 18);
        lv_obj_set_flex_align(cards, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        std::vector<core::MediaItem> items;
        if (const auto* recent = state().list(Kind::RecentlyPlayed)) items = recent->items;
        if (items.empty() && state().playlists) items = state().playlists->playlists;
        for (size_t i = 0; i < items.size() && i < 4; ++i) {
            const auto& it = items[i];
            kit::media_card(cards, it, kit::subtitle(it), [it] { open_item(it); }, 128);
        }
        if (items.empty()) kit::text(cards, tr(i18n::Str::list_empty), theme::meta(), theme::text3());
    }

    void stations() {
        if (state().ever_connected && !state().connected()) {
            lv_obj_t* b = banner(root_, tr(i18n::Str::offline_title), tr(i18n::Str::offline_body));
            kit::button(b, kit::Button::Secondary, icon::refresh, tr(i18n::Str::offline_retry), [] { send(app::cmd::Reconnect{}); });
            return;
        }
        if (filter_ == Filter::Music) return;
        const auto* radio = state().list(Kind::LibraryRadio);
        if (!radio || radio->items.empty()) return;
        kit::section_header(root_, tr(i18n::Str::home_stations), tr(i18n::Str::home_see_all), [] { open(ScreenId::Library); });
        lv_obj_t* row = kit::row(root_, 12);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        for (size_t i = 0; i < radio->items.size() && i < 7; ++i) {
            const auto& st = radio->items[i];
            kit::media_card(row, st, "", [st] { play(st); }, 88);
        }
    }

    lv_obj_t *root_ = nullptr, *band_ = nullptr, *progress_ = nullptr;
    Filter filter_ = Filter::All;
};

}  // namespace

std::unique_ptr<Screen> make_home() { return std::make_unique<Home>(); }

}  // namespace ui
