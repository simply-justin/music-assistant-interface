// The feature registry: adding a feature = one file in features/ + one line here.
#include "features/features.hpp"

namespace app {

std::vector<std::unique_ptr<Feature>> make_features(Context& ctx) {
    std::vector<std::unique_ptr<Feature>> f;
    f.push_back(make_players(ctx));  // first: selects the player the others use
    f.push_back(make_playback(ctx));
    f.push_back(make_queue(ctx));
    f.push_back(make_search(ctx));
    f.push_back(make_favourites(ctx));
    f.push_back(make_preferences(ctx));
    f.push_back(make_playlists(ctx));
    // OTA: reserved seam, add make_ota(ctx) here once the partition layout is set (Phase 1).
    return f;
}

}  // namespace app
