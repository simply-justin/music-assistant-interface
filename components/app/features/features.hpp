#pragma once
#include "app/feature.hpp"

namespace app {

std::unique_ptr<Feature> make_players(Context& ctx);
std::unique_ptr<Feature> make_playback(Context& ctx);
std::unique_ptr<Feature> make_queue(Context& ctx);
std::unique_ptr<Feature> make_search(Context& ctx);
std::unique_ptr<Feature> make_favourites(Context& ctx);
std::unique_ptr<Feature> make_preferences(Context& ctx);
std::unique_ptr<Feature> make_playlists(Context& ctx);

}  // namespace app
