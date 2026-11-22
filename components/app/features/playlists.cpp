// Playlists: the library list (Home shelf + "Add to playlist" picker), add, create, remove, delete.
#include "backend/ma_commands.hpp"
#include "core/repeat_guard.hpp"
#include "features/features.hpp"

namespace app {
namespace {

constexpr int kMaxPlaylists = 200;

class Playlists : public Feature {
public:
    explicit Playlists(Context& ctx) : ctx_(ctx) {}

    void on_connected() override { load(); }

    void on_event(const backend::Event& ev) override {
        // A playlist was created, renamed or deleted elsewhere: refresh the list.
        if (ev.name == "media_item_added" || ev.name == "media_item_updated" || ev.name == "media_item_deleted") {
            const cJSON* type = cJSON_GetObjectItemCaseSensitive(ev.data.get(), "media_type");
            if (cJSON_IsString(type) && std::string(type->valuestring) == "playlist") load();
        }
    }

    void on_command(const Command& c) override {
        if (std::holds_alternative<cmd::LoadPlaylists>(c)) {
            load();
        } else if (auto* a = std::get_if<cmd::AddToPlaylist>(&c)) {
            if (!guard_.allow("pl|" + a->playlist.item_id + "|" + a->item.uri, ctx_.now_ms)) return;
            std::string name = a->playlist.name;
            ctx_.send(ma::cmd::playlist_add_tracks(a->playlist.item_id, {a->item.uri}),
                      [this, name](backend::Response&&) { ctx_.toast(i18n::tr(i18n::Str::toast_added_to_playlist, name)); });
        } else if (auto* n = std::get_if<cmd::CreatePlaylist>(&c)) {
            create(*n);
        } else if (auto* r = std::get_if<cmd::RemoveFromPlaylist>(&c)) {
            ctx_.send(ma::cmd::playlist_remove_tracks(r->playlist.item_id, {r->position}));
        } else if (auto* d = std::get_if<cmd::DeletePlaylist>(&c)) {
            ctx_.send(ma::cmd::playlist_delete(d->playlist.item_id), [this](backend::Response&&) {
                ctx_.toast(i18n::tr(i18n::Str::toast_playlist_deleted));
                load();
            });
        }
    }

private:
    void load() {
        ctx_.send(ma::cmd::library_items(core::MediaType::Playlist, false, kMaxPlaylists, 0, "last_played_desc"),
                  [this](backend::Response&& r) {
                      view_.playlists = ma::parse::media_items(r.result.get());
                      ctx_.publish(std::make_shared<const PlaylistsView>(view_));
                  });
    }

    void create(const cmd::CreatePlaylist& n) {
        auto first = n.first_item;
        std::string name = n.name;
        ctx_.send(ma::cmd::playlist_create(name), [this, first, name](backend::Response&& r) {
            core::MediaItem created = ma::parse::media_item(r.result.get());
            ctx_.toast(i18n::tr(i18n::Str::toast_playlist_created, name));
            if (first && !created.item_id.empty()) ctx_.send(ma::cmd::playlist_add_tracks(created.item_id, {first->uri}));
            load();
        });
    }

    Context& ctx_;
    PlaylistsView view_;
    core::RepeatGuard guard_;
};

}  // namespace

std::unique_ptr<Feature> make_playlists(Context& ctx) { return std::make_unique<Playlists>(ctx); }

}  // namespace app
