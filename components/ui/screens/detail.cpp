// Album, artist and playlist detail: one screen re-bound to the item. Playlists Music Assistant
// marks editable get Edit mode (× removes a song; guarded delete). No reordering: no MA command.
#include "../shell.hpp"
#include "../text.hpp"

namespace ui {
namespace {

using Kind = app::cmd::BrowseKind;

class Detail : public Screen {
public:
    void build(lv_obj_t* root) override {
        root_ = root;
        lv_obj_set_style_pad_hor(root, theme::kPad, 0);
        lv_obj_set_style_pad_top(root, 20, 0);
        lv_obj_set_style_pad_row(root, 20, 0);
    }

    void on_open() override {
        editing_ = false;
        const auto& item = detail_item();
        send(app::cmd::Browse{kind(item), item});
    }

    void refresh(Topic t) override {
        if (t != Topic::All && t != Topic::Browse && t != Topic::Playlists && t != Topic::Queue) return;
        rebuild();
    }

    ScreenId tab() const override { return ScreenId::Library; }
    std::string title() const override { return detail_item().name; }

private:
    static Kind kind(const core::MediaItem& item) {
        switch (item.type) {
            case core::MediaType::Album: return Kind::AlbumTracks;
            case core::MediaType::Artist: return Kind::ArtistTracks;
            default: return Kind::PlaylistTracks;
        }
    }

    void rebuild() {
        lv_obj_clean(root_);
        const core::MediaItem item = detail_item();
        const auto* list = state().list(kind(item), item.uri);
        std::vector<core::MediaItem> tracks = list ? list->items : std::vector<core::MediaItem>{};
        uint32_t total = 0;
        for (const auto& t : tracks) total += t.duration_s;
        size_t track_count = tracks.size();

        kit::back_chip(root_, back_label(), [] { back(); });
        lv_obj_t* body = kit::row(root_, 48);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_flex_grow(body, 1);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        lv_obj_t* left = kit::column(body, 20);
        lv_obj_set_width(left, 300);
        kit::art(left, item, 300, theme::r_art());
        lv_obj_t* info = kit::column(left, 8);
        lv_obj_set_width(info, 300);
        i18n::Str type = item.type == core::MediaType::Album ? i18n::Str::type_album
                       : item.type == core::MediaType::Artist ? i18n::Str::type_artist
                                                              : i18n::Str::type_playlist;
        kit::overline(info, tr(type));
        kit::text(info, item.name, theme::h1(), theme::text(), true);
        if (track_count)
            kit::text(info, songs(track_count) + " · " + duration(total), theme::body(), theme::text2());

        lv_obj_t* right = kit::column(body, 16);
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_height(right, lv_pct(100));
        lv_obj_t* actions = kit::row(right, 12);
        lv_obj_set_size(actions, lv_pct(100), 64);
        if (editing_) {
            lv_obj_t* hint = kit::overline(actions, tr(i18n::Str::edit_hint), theme::accent());
            lv_obj_set_flex_grow(hint, 1);
            kit::button(actions, kit::Button::Ghost, icon::trash, tr(i18n::Str::edit_delete), [item] {
                confirm(i18n::tr(i18n::Str::edit_delete_title, item.name), tr(i18n::Str::edit_delete_body),
                        tr(i18n::Str::edit_delete_keep), tr(i18n::Str::edit_delete), [item] {
                            send(app::cmd::DeletePlaylist{item});
                            open(ScreenId::Library);
                        });
            });
            kit::button(actions, kit::Button::Primary, icon::check, tr(i18n::Str::edit_done), [this] {
                editing_ = false;
                kit::later([this] { rebuild(); });
            });
        } else {
            kit::button(actions, kit::Button::Primary, icon::play, tr(i18n::Str::detail_play), [item] { play(item); });
            kit::button(actions, kit::Button::Secondary, icon::shuffle, tr(i18n::Str::queue_shuffle), [item] {
                send(app::cmd::SetShuffle{true});
                play(item);
            });
            kit::icon_button(actions, item.favorite ? icon::heart_filled : icon::heart,
                             [item] { send(app::cmd::SetFavourite{item, !item.favorite}); }, 56, 28, true,
                             item.favorite ? theme::accent() : theme::text());
            kit::spacer(actions);
            if (item.type == core::MediaType::Playlist && item.editable) {
                kit::chip(actions, icon::edit, tr(i18n::Str::detail_edit), false, [this] {
                    editing_ = true;
                    kit::later([this] { rebuild(); });
                });
            }
        }

        if (tracks.empty()) {
            kit::text(right, tr(list && !list->loading ? i18n::Str::list_empty : i18n::Str::list_loading), theme::lead(), theme::text2());
            return;
        }
        auto now = state().now_playing();
        std::string now_uri = now ? now->uri : "";
        auto shared = std::make_shared<std::vector<core::MediaItem>>(std::move(tracks));
        bool editing = editing_;
        kit::virtual_list(right, shared->size(), 64, 2, [shared, now_uri, editing, item](lv_obj_t* parent, size_t i) {
            const auto& t = (*shared)[i];
            kit::RowOptions o;
            o.height = 64;
            o.playing = !now_uri.empty() && now_uri == t.uri;
            if (editing) {
                int pos = static_cast<int>(i);  // 0-based list index; ma_commands converts for the server
                o.on_remove = [item, pos] { send(app::cmd::RemoveFromPlaylist{item, pos}); };
            } else {
                o.on_tap = [t] { play(t); };
                o.on_more = [t] { open_track_menu(t); };
            }
            return kit::media_row(parent, t, o);
        });
    }

    lv_obj_t* root_ = nullptr;
    bool editing_ = false;
};

}  // namespace

std::unique_ptr<Screen> make_detail() { return std::make_unique<Detail>(); }

}  // namespace ui
