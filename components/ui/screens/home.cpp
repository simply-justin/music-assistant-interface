// Home: what's playing, six favourite tiles, and a "Recently played / Your playlists" shelf.
#include <ctime>

#include "../shell.hpp"

namespace ui {
namespace {

using Kind = app::cmd::BrowseKind;

class Home : public Screen {
public:
    void build(lv_obj_t* root) override {
        lv_obj_set_style_pad_all(root, theme::kPad, 0);
        lv_obj_set_style_pad_row(root, 28, 0);
        header_ = kit::row(root, 16);
        lv_obj_set_width(header_, lv_pct(100));
        top_ = kit::row(root, 24);
        lv_obj_set_size(top_, lv_pct(100), 272);
        lv_obj_set_flex_align(top_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        shelf_ = kit::column(root, 16);
        lv_obj_set_width(shelf_, lv_pct(100));
    }

    void on_open() override {
        send(app::cmd::Browse{Kind::Favourites, {}});
        send(app::cmd::Browse{Kind::RecentlyPlayed, {}});
        send(app::cmd::LoadPlaylists{});
    }

    void refresh(Topic t) override {
        if (t == Topic::Progress) {
            if (progress_) lv_bar_set_value(progress_, static_cast<int32_t>(state().elapsed_s), LV_ANIM_OFF);
            return;
        }
        if (t == Topic::All || t == Topic::Connection || t == Topic::Players) build_header();
        build_top();
        if (t != Topic::Queue && t != Topic::Panel) build_shelf();
    }

    bool mini_player() const override { return false; }
    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::nav_home); }

private:
    void build_header() {
        lv_obj_clean(header_);
        std::time_t now = std::time(nullptr);
        int hour = std::localtime(&now)->tm_hour;
        auto greeting = hour < 12 ? i18n::Str::home_morning : hour < 18 ? i18n::Str::home_afternoon : i18n::Str::home_evening;
        lv_obj_t* h = kit::text(header_, tr(greeting), theme::h1(), theme::text());
        lv_obj_set_flex_grow(h, 1);

        bool ok = state().connection.state == backend::Connection::Connected;
        lv_obj_t* status = kit::row(header_, 10);
        lv_obj_t* dot = lv_obj_create(status);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, 5, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, ok ? theme::accent() : theme::warning(), 0);
        kit::text(status, tr(ok ? i18n::Str::home_connected : i18n::Str::home_reconnecting), theme::body(),
                  ok ? theme::text2() : theme::warning());
        speaker_chip(header_);
    }

    void build_top() {
        lv_obj_clean(top_);
        progress_ = nullptr;

        // Now playing card (500 px). Tap the body → Now Playing; the buttons act directly.
        lv_obj_t* card = kit::row(top_, 24);
        lv_obj_set_size(card, 500, 272);
        lv_obj_set_style_pad_all(card, 24, 0);
        lv_obj_set_style_radius(card, theme::kRadiusCard, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1B2A2E), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x243A40), LV_STATE_PRESSED);
        auto item = state().now_playing();
        if (!item) {
            lv_obj_t* col = kit::column(card, 10);
            lv_obj_set_flex_grow(col, 1);
            kit::overline(col, tr(i18n::Str::home_nothing_playing), theme::text2());
            kit::text(col, tr(i18n::Str::home_pick_favourite), theme::lead(), theme::text(), true);
        } else {
            kit::art(card, *item, 224, 14);
            lv_obj_t* col = kit::column(card, 0);
            lv_obj_set_flex_grow(col, 1);
            lv_obj_set_size(col, 1, lv_pct(100));
            lv_obj_set_flex_align(col, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_t* info = kit::column(col, 6);
            lv_obj_set_width(info, lv_pct(100));
            kit::overline(info, tr(i18n::Str::home_now_playing), theme::accent());
            kit::text(info, item->name, theme::h2(), theme::text());
            kit::text(info, kit::subtitle(*item), theme::body(), theme::text2());
            lv_obj_t* bottom = kit::column(col, 16);
            lv_obj_set_width(bottom, lv_pct(100));
            progress_ = lv_bar_create(bottom);
            lv_obj_set_size(progress_, lv_pct(100), 4);
            lv_obj_set_style_bg_color(progress_, lv_color_hex(0x3A4A50), LV_PART_MAIN);
            lv_obj_set_style_bg_color(progress_, theme::text(), LV_PART_INDICATOR);
            lv_bar_set_range(progress_, 0, item->duration_s ? static_cast<int32_t>(item->duration_s) : 1);
            lv_bar_set_value(progress_, static_cast<int32_t>(state().elapsed_s), LV_ANIM_OFF);
            lv_obj_t* controls = kit::row(bottom, 0);
            lv_obj_set_width(controls, lv_pct(100));
            lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            bool radio = item->type == core::MediaType::Radio;
            if (!radio) kit::icon_button(controls, icon::prev, [] { send(app::cmd::Previous{}); }, 64);
            kit::play_button(controls, state().playing(), [] { send(app::cmd::PlayPause{}); }, 64);
            if (!radio) kit::icon_button(controls, icon::next, [] { send(app::cmd::Next{}); }, 64);
        }
        kit::on_click(card, [] { open(ScreenId::NowPlaying); });

        // Favourites: fixed 2 × 3 grid, one tap plays.
        lv_obj_t* grid = lv_obj_create(top_);
        lv_obj_remove_style_all(grid);
        lv_obj_set_flex_grow(grid, 1);
        lv_obj_set_height(grid, 272);
        static const int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        static const int32_t rows[] = {80, 80, 80, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_grid_dsc_array(grid, cols, rows);
        lv_obj_set_style_pad_row(grid, 16, 0);
        lv_obj_set_style_pad_column(grid, 16, 0);
        const auto* favs = state().list(Kind::Favourites);
        if (favs) {
            for (size_t i = 0; i < favs->items.size() && i < 6; ++i) {
                const auto& f = favs->items[i];
                lv_obj_t* t = kit::favourite_tile(grid, f, [f] { play(f); }, [f] { open_favourite_preview(f); });
                lv_obj_set_grid_cell(t, LV_GRID_ALIGN_STRETCH, static_cast<int32_t>(i % 2), 1, LV_GRID_ALIGN_STRETCH,
                                     static_cast<int32_t>(i / 2), 1);
            }
        }
    }

    void build_shelf() {
        lv_obj_clean(shelf_);
        if (state().ever_connected && !state().connected()) {
            // Offline: the shelf can't load anyway; say what's happening instead.
            lv_obj_t* b = banner(shelf_, tr(i18n::Str::offline_title), tr(i18n::Str::offline_body));
            kit::button(b, kit::Button::Secondary, icon::refresh, tr(i18n::Str::offline_retry), [] { send(app::cmd::Reconnect{}); });
            return;
        }
        lv_obj_t* head = kit::row(shelf_, 12);
        lv_obj_set_width(head, lv_pct(100));
        kit::chip(head, nullptr, tr(i18n::Str::home_recently_played), !show_playlists_, [this] { show_playlists_ = false; kit::later([this] { build_shelf(); }); });
        kit::chip(head, nullptr, tr(i18n::Str::home_your_playlists), show_playlists_, [this] { show_playlists_ = true; kit::later([this] { build_shelf(); }); });
        kit::spacer(head);
        kit::chip(head, nullptr, tr(i18n::Str::home_see_all), false, [] { open(ScreenId::Library); });

        lv_obj_t* cards = kit::row(shelf_, 24);
        lv_obj_set_flex_align(cards, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        std::vector<core::MediaItem> items;
        if (show_playlists_) {
            if (state().playlists) items = state().playlists->playlists;
        } else if (const auto* recent = state().list(Kind::RecentlyPlayed)) {
            items = recent->items;
        }
        for (size_t i = 0; i < items.size() && i < 6; ++i) {
            const auto& it = items[i];
            kit::media_card(cards, it, kit::subtitle(it), [it] { open_item(it); });
        }
        if (items.empty()) kit::text(cards, tr(i18n::Str::list_empty), theme::lead(), theme::text3());
    }

    lv_obj_t *header_ = nullptr, *top_ = nullptr, *shelf_ = nullptr, *progress_ = nullptr;
    bool show_playlists_ = false;
};

}  // namespace

std::unique_ptr<Screen> make_home() { return std::make_unique<Home>(); }

}  // namespace ui
