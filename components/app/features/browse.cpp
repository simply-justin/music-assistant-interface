// Read-only lists for the screens: favourites, recently played, library tabs, and the tracks of
// an album, artist or playlist. One request (or a few, merged) per list.
#include "backend/ma_commands.hpp"
#include "features/features.hpp"

namespace app {
namespace {

using core::MediaType;
using Kind = cmd::BrowseKind;

constexpr int kPage = 500;       // items per request
constexpr size_t kMaxList = 5000;  // a longer library list is cut off (PSRAM: ~1 KB per item)

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

        // One maker per part; library lists are paged (the maker takes the offset), the others
        // come whole from the server.
        std::vector<std::pair<Maker, bool>> parts;  // maker, paged
        auto library = [&](MediaType t, bool fav) {
            parts.push_back({[t, fav](int offset) { return ma::cmd::library_items(t, fav, kPage, offset); }, true});
        };
        auto whole = [&](backend::Request (*make)(const core::MediaItem&)) {
            core::MediaItem of = b->of;
            parts.push_back({[make, of](int) { return make(of); }, false});
        };
        switch (b->kind) {
            // Favourites mix all types: playlists and radio first (one-tap starts), then albums, tracks.
            case Kind::Favourites:
                for (auto t : {MediaType::Playlist, MediaType::Radio, MediaType::Album, MediaType::Artist, MediaType::Track})
                    library(t, true);
                break;
            case Kind::RecentlyPlayed: parts.push_back({[](int) { return ma::cmd::recently_played(20); }, false}); break;
            case Kind::LibraryAlbums: library(MediaType::Album, false); break;
            case Kind::LibraryArtists: library(MediaType::Artist, false); break;
            case Kind::LibraryTracks: library(MediaType::Track, false); break;
            case Kind::LibraryRadio: library(MediaType::Radio, false); break;
            case Kind::AlbumTracks: whole(ma::cmd::album_tracks); break;
            case Kind::ArtistTracks: whole(ma::cmd::artist_tracks); break;
            case Kind::ArtistAlbums: whole(ma::cmd::artist_albums); break;
            case Kind::PlaylistTracks: whole(ma::cmd::playlist_tracks); break;
        }

        // Parts arrive in any order; keep them in request order and publish once all are in.
        auto results = std::make_shared<std::vector<std::vector<core::MediaItem>>>(parts.size());
        auto pending = std::make_shared<size_t>(parts.size());
        for (size_t i = 0; i < parts.size(); ++i) fetch(parts[i].first, parts[i].second, view, results, pending, i);
    }

private:
    using Maker = std::function<backend::Request(int offset)>;
    void fetch(Maker make, bool paged, std::shared_ptr<BrowseView> view,
               std::shared_ptr<std::vector<std::vector<core::MediaItem>>> results, std::shared_ptr<size_t> pending, size_t i) {
        auto& mine = (*results)[i];
        ctx_.backend.request(make(static_cast<int>(mine.size())), [=](backend::Response&& r) {
            auto& part = (*results)[i];
            size_t got = 0;
            if (r.ok) {
                auto page = ma::parse::media_items(r.result.get());
                got = page.size();
                part.insert(part.end(), std::make_move_iterator(page.begin()), std::make_move_iterator(page.end()));
            } else {
                view->failed = true;
            }
            if (paged && got == static_cast<size_t>(kPage) && part.size() < kMaxList)
                return fetch(make, paged, view, results, pending, i);
            if (--*pending) return;
            for (auto& p : *results) view->items.insert(view->items.end(), p.begin(), p.end());
            view->loading = false;
            ctx_.publish(std::shared_ptr<const BrowseView>(view));
        });
    }

    Context& ctx_;
};

}  // namespace

std::unique_ptr<Feature> make_browse(Context& ctx) { return std::make_unique<Browse>(ctx); }

}  // namespace app
