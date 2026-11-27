// Home (direction A): greeting + filter, 8 one-tap shortcut tiles, "Now on <speaker>",
// recently played, your stations. A flat tint from the current artwork sits behind the top.
#include <algorithm>
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
        lv_obj_set_style_pad_bottom(root, 24, 0);  // room under the last row when the page scrolls
    }

    // Home is opened often (every tab switch): reload only after a (re)connect or when the lists are
    // a few minutes old. Favourites changed on this panel are refreshed by the core right away.
    void on_open() override {
        bool fresh = loaded_epoch_ == state().connected_epoch && lv_tick_elaps(loaded_at_) < kReloadMs &&
                     state().list(Kind::Favourites);
        if (fresh) return;
        loaded_epoch_ = state().connected_epoch;
        loaded_at_ = lv_tick_get();
        send(app::cmd::Browse{Kind::Favourites, {}});
        send(app::cmd::Browse{Kind::RecentlyPlayed, {}});
        send(app::cmd::Browse{Kind::LibraryRadio, {}});
    }

    void refresh(Topic t) override {
        // Home shows lists, favourites and the connection, not the speakers or the queue (the
        // player bar does): skip those, so playback changes don't rebuild the page under a finger.
        if (t == Topic::Progress || t == Topic::Search || t == Topic::Setup || t == Topic::Players || t == Topic::Queue ||
            t == Topic::Panel || t == Topic::Sleep || t == Topic::Update)
            return;
        rebuild();
    }

    ScreenId tab() const override { return ScreenId::Home; }
    std::string title() const override { return tr(i18n::Str::nav_home); }

private:
    void rebuild() {
        lv_obj_clean(root_);
        header();
        shortcuts();
        recent();
        stations();
    }

    void header() {
        lv_obj_t* h = kit::row(root_, 10);
        lv_obj_set_size(h, lv_pct(100), 48);
        std::time_t now = std::time(nullptr);
        int hour = std::localtime(&now)->tm_hour;
        auto greeting = hour < 12 ? i18n::Str::home_morning : hour < 18 ? i18n::Str::home_afternoon : i18n::Str::home_evening;
        kit::text(h, tr(greeting), theme::h1(), theme::text());
        kit::spacer(h);
        if (state().ever_connected && !state().connected()) {
            lv_obj_t* w = kit::text(h, tr(i18n::Str::home_reconnecting), theme::meta(), theme::warning());
            lv_obj_set_style_margin_right(w, 24, 0);
        }
        // Filter tabs, right-aligned: they filter every section below.
        struct F { Filter f; i18n::Str s; };
        lv_obj_t* last = nullptr;
        for (F x : {F{Filter::All, i18n::Str::filter_all}, F{Filter::Music, i18n::Str::home_music}, F{Filter::Radio, i18n::Str::filter_radio}}) {
            Filter f = x.f;
            last = kit::tab(h, tr(x.s), filter_ == f, [this, f] {
                filter_ = f;
                kit::later([this] { rebuild(); });
            });
        }
        lv_obj_set_style_margin_right(last, 0, 0);  // flush with the right edge
    }

    bool keep(const core::MediaItem& m) const {
        bool radio = m.type == core::MediaType::Radio;
        return filter_ == Filter::All || (filter_ == Filter::Radio) == radio;
    }

    // 4 × 2 one-tap favourites; long-press opens the preview.
    void shortcuts() {
        lv_obj_t* grid = lv_obj_create(root_);
        lv_obj_remove_style_all(grid);
        lv_obj_remove_flag(grid, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(grid, lv_pct(100), 56 * 2 + kGap);
        static const int32_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
        static const int32_t rows[] = {56, 56, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_grid_dsc_array(grid, cols, rows);
        lv_obj_set_style_pad_row(grid, kGap, 0);
        lv_obj_set_style_pad_column(grid, kGap, 0);
        const auto* favs = state().list(Kind::Favourites);
        int n = 0;
        for (size_t i = 0; favs && i < favs->items.size() && n < 8; ++i) {
            const auto& f = favs->items[i];
            if (!keep(f)) continue;
            lv_obj_t* t = kit::favourite_tile(grid, f, [f] { play(f); }, [f] { open_favourite_preview(f); });
            lv_obj_set_grid_cell(t, LV_GRID_ALIGN_STRETCH, n % 4, 1, LV_GRID_ALIGN_STRETCH, n / 4, 1);
            ++n;
        }
        if (n == 0) {
            lv_obj_t* hint = kit::text(grid, tr(i18n::Str::home_no_favourites), theme::meta(), theme::text2(), true);
            lv_obj_set_grid_cell(hint, LV_GRID_ALIGN_START, 0, 4, LV_GRID_ALIGN_CENTER, 0, 2);
        }
    }

    // Recently played (or your playlists if nothing was played yet): one row of equal covers.
    void recent() {
        lv_obj_t* sh = kit::section_header(root_, tr(i18n::Str::home_recently_played), tr(i18n::Str::home_see_all), [] { open(ScreenId::Library); });
        lv_obj_set_style_margin_top(sh, kSectionGap, 0);
        lv_obj_t* cards = kit::row(root_, kGap);
        lv_obj_set_flex_align(cards, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        std::vector<core::MediaItem> items;
        if (const auto* recent = state().list(Kind::RecentlyPlayed)) items = recent->items;
        if (items.empty() && state().playlists && filter_ != Filter::Radio) items = state().playlists->playlists;
        items.erase(std::remove_if(items.begin(), items.end(), [this](const core::MediaItem& m) { return !keep(m); }), items.end());
        for (size_t i = 0; i < items.size() && i < kPerRow; ++i) {
            const auto& it = items[i];
            kit::media_card(cards, it, kit::subtitle(it), [it] { open_item(it); }, kCell);
        }
        full_width(cards);
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
        lv_obj_t* sh = kit::section_header(root_, tr(i18n::Str::home_stations), tr(i18n::Str::home_see_all), [] { open(ScreenId::Library); });
        lv_obj_set_style_margin_top(sh, kSectionGap, 0);
        lv_obj_t* row = kit::row(root_, kGap);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        for (size_t i = 0; i < radio->items.size() && i < kPerRow; ++i) {
            const auto& st = radio->items[i];
            kit::media_card(row, st, "", [st] { play(st); }, kCell - 32);  // circle + wider label = one cell
        }
        full_width(row);
    }

    // A full row spreads its cells to both content edges, so every row lines up with the tiles above.
    static void full_width(lv_obj_t* row) {
        if (lv_obj_get_child_count(row) == kPerRow)
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_width(row, lv_pct(100));
    }

    // One grid for the whole page: 6 cells of 152 px with 16 px gaps fill the 1000 px content width.
    static constexpr int32_t kCell = 152, kGap = 16, kSectionGap = 18;  // + the page's 14 px row gap = 32 px
    static constexpr size_t kPerRow = 6;
    lv_obj_t* root_ = nullptr;
    Filter filter_ = Filter::All;
    static constexpr uint32_t kReloadMs = 120'000;
    uint32_t loaded_epoch_ = UINT32_MAX, loaded_at_ = 0;
};

}  // namespace

std::unique_ptr<Screen> make_home() { return std::make_unique<Home>(); }

}  // namespace ui
