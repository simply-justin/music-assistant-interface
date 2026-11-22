// Read-only lists for the screens: favourites, recently played, library tabs, and the tracks of
// an album, artist or playlist. One request (or a few, merged) per list.
#include "backend/ma_commands.hpp"
#include "features/features.hpp"

namespace app {
namespace {

using core::MediaType;
using Kind = cmd::BrowseKind;

constexpr int kLimit = 500;  // shortcut: one page per list; page on scroll once lists get longer

class Browse : public Feature {
public:
    explicit Browse(Context& ctx) : ctx_(ctx) {}

    void on_command(const Command& c) override {
        auto* b = std::get_if<cmd::Browse>(&c);
        if (!b) return;
        auto view = std::make_shared<BrowseView>();
        view->kind = b->kind;
        view->of_uri = b->of.uri;
        ctx_.publish(std::shared_ptr<const BrowseView>(std::make_shared<BrowseView>(*view)));  // loading

        std::vector<backend::Request> reqs;
        switch (b->kind) {
            // Favourites mix all types: playlists and radio first (one-tap starts), then albums, tracks.
            case Kind::Favourites:
                for (auto t : {MediaType::Playlist, MediaType::Radio, MediaType::Album, MediaType::Artist, MediaType::Track})
                    reqs.push_back(ma::cmd::library_items(t, true, kLimit, 0));
                break;
            case Kind::RecentlyPlayed: reqs.push_back(ma::cmd::recently_played(20)); break;
            case Kind::LibraryAlbums: reqs.push_back(ma::cmd::library_items(MediaType::Album, false, kLimit, 0)); break;
            case Kind::LibraryArtists: reqs.push_back(ma::cmd::library_items(MediaType::Artist, false, kLimit, 0)); break;
            case Kind::LibraryTracks: reqs.push_back(ma::cmd::library_items(MediaType::Track, false, kLimit, 0)); break;
            case Kind::LibraryRadio: reqs.push_back(ma::cmd::library_items(MediaType::Radio, false, kLimit, 0)); break;
            case Kind::AlbumTracks: reqs.push_back(ma::cmd::album_tracks(b->of)); break;
            case Kind::ArtistTracks: reqs.push_back(ma::cmd::artist_tracks(b->of)); break;
            case Kind::ArtistAlbums: reqs.push_back(ma::cmd::artist_albums(b->of)); break;
            case Kind::PlaylistTracks: reqs.push_back(ma::cmd::playlist_tracks(b->of)); break;
        }

        // Parts arrive in any order; keep them in request order and publish once all are in.
        auto parts = std::make_shared<std::vector<std::vector<core::MediaItem>>>(reqs.size());
        auto pending = std::make_shared<size_t>(reqs.size());
        for (size_t i = 0; i < reqs.size(); ++i) {
            ctx_.backend.request(std::move(reqs[i]), [this, view, parts, pending, i](backend::Response&& r) {
                if (r.ok) (*parts)[i] = ma::parse::media_items(r.result.get());
                else view->failed = true;
                if (--*pending) return;
                for (auto& p : *parts) view->items.insert(view->items.end(), p.begin(), p.end());
                view->loading = false;
                ctx_.publish(std::shared_ptr<const BrowseView>(view));
            });
        }
    }

private:
    Context& ctx_;
};

}  // namespace

std::unique_ptr<Feature> make_browse(Context& ctx) { return std::make_unique<Browse>(ctx); }

}  // namespace app
