# Artwork scaling + LRU cache (portable). Fetch/decode are platform work (HW JPEG on the board).
idf_component_register(SRCS "artwork.cpp" INCLUDE_DIRS "include" REQUIRES core)
