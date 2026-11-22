idf_component_register(
    SRCS "app.cpp" "features.cpp"
         "features/players.cpp" "features/playback.cpp" "features/queue.cpp"
         "features/search.cpp" "features/favourites.cpp" "features/preferences.cpp"
         "features/playlists.cpp"
    INCLUDE_DIRS "include" "."
    REQUIRES core backend i18n)
