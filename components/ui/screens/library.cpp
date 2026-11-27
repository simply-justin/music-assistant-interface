// Library: one library, tabs by media type (never by provider).
#include "../shell.hpp"

namespace ui {
namespace {

using Kind = app::cmd::BrowseKind;
enum class Tab : uint8_t { Favourites, Playlists, Albums, Artists, Tracks, Radio };

class Library : public Screen {
public:
    void build(lv_obj_t* root) override {
        lv_obj_set_style_pad_hor(root, theme::kPad, 0);
        lv_obj_set_style_pad_top(root, theme::kPad, 0);
        lv_obj_set_style_pad_row(root, 24, 0);
        kit::text(root, tr(i18n::Str::nav_library), theme::h1(), theme::text());
        tabs_ = kit::row(root, 12);
        content_ = kit::scroll_column(root, 4);
        lv_obj_set_width(content_, lv_pct(100));
        lv_obj_set_flex_grow(content_, 1);
        lv_obj_set_style_pad_bottom(content_, 24, 0);
    }

    void on_open() override { load(); }

    void refresh(Topic t) override {
        if (t != Topic::All && t != Topic::Browse && t != Topic::Playlists) return;
        build_tabs();
        build_content();
    }

    ScreenId tab() const override { return ScreenId::Library; }
    std::string title() const override { return tr(i18n::Str::nav_library); }

private:
    Kind kind() const {
        switch (tab_) {
            case Tab::Favourites: return Kind::Favourites;
            case Tab::Albums: return Kind::LibraryAlbums;
            case Tab::Artists: return Kind::LibraryArtists;
            case Tab::Tracks: return Kind::LibraryTracks;
            default: return Kind::LibraryRadio;
        }
    }

    void load() {
        if (tab_ == Tab::Playlists) send(app::cmd::LoadPlaylists{});
        else send(app::cmd::Browse{kind(), {}});
    }

    void build_tabs() {
        lv_obj_clean(tabs_);
        struct T { Tab t; i18n::Str s; };
        for (T x : {T{Tab::Favourites, i18n::Str::lib_favourites}, T{Tab::Playlists, i18n::Str::filter_playlists},
                    T{Tab::Albums, i18n::Str::filter_albums}, T{Tab::Artists, i18n::Str::filter_artists},
                    T{Tab::Tracks, i18n::Str::filter_tracks}, T{Tab::Radio, i18n::Str::filter_radio}}) {
            Tab t = x.t;
            kit::tab(tabs_, tr(x.s), tab_ == t, [this, t] {
                tab_ = t;
                kit::later([this] {
                    load();
                    refresh(Topic::All);
                });
            });
        }
    }

    void build_content() {
        lv_obj_clean(content_);
        std::vector<core::MediaItem> items;
        bool loading = false;
        if (tab_ == Tab::Playlists) {
            if (state().playlists) items = state().playlists->playlists;
            else loading = true;
        } else if (const auto* l = state().list(kind())) {
            items = l->items;
            loading = l->loading;
        } else {
            loading = true;
        }
        if (items.empty()) {
            kit::text(content_, tr(loading ? i18n::Str::list_loading : i18n::Str::list_empty), theme::lead(), theme::text2());
            return;
        }
        if (tab_ == Tab::Tracks || tab_ == Tab::Radio) {
            auto list = std::make_shared<std::vector<core::MediaItem>>(std::move(items));
            kit::virtual_list(content_, list->size(), 72, 0, [list](lv_obj_t* parent, size_t i) {
                const auto& it = (*list)[i];
                kit::RowOptions o;
                o.on_tap = [it] { play(it); };
                o.on_more = [it] { open_track_menu(it); };
                return kit::media_row(parent, it, o);
            });
            return;
        }
        // The same six-column grid as Home: 152 px covers, 16 px apart. Windowed by row of six, so
        // a library of thousands of albums only builds the rows on screen.
        constexpr size_t kPerRow = 6;
        constexpr int32_t kCardRowH = 212;  // cover + name + subtitle
        auto all = std::make_shared<std::vector<core::MediaItem>>(std::move(items));
        size_t rows = (all->size() + kPerRow - 1) / kPerRow;
        kit::virtual_list(content_, rows, kCardRowH, 24, [all](lv_obj_t* list, size_t r) {
            lv_obj_t* row = kit::row(list, 16);
            lv_obj_set_size(row, lv_pct(100), kCardRowH);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            for (size_t i = r * kPerRow; i < all->size() && i < (r + 1) * kPerRow; ++i) {
                const auto& it = (*all)[i];
                kit::media_card(row, it, kit::subtitle(it), [it] { open_item(it); }, 152);
            }
            return row;
        });
    }

    lv_obj_t *tabs_ = nullptr, *content_ = nullptr;
    Tab tab_ = Tab::Playlists;
};

}  // namespace

std::unique_ptr<Screen> make_library() { return std::make_unique<Library>(); }

}  // namespace ui
