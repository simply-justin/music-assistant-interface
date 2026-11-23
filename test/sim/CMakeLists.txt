# PC simulator: the real UI + core in a 1280×800 window (X11, works under WSLg), or headless
# screenshots.
#   cmake -S test/sim -B test/sim/build && cmake --build test/sim/build -j
#   test/sim/build/music_panel_sim                 # mock data
#   test/sim/build/music_panel_sim --live test/host/server.env
#   test/sim/build/music_panel_sim --shots out/    # writes one PNG-ready .ppm per screen
#   ctest --test-dir test/sim/build --output-on-failure   # end-to-end tests (panel_e2e)
cmake_minimum_required(VERSION 3.16)
project(panel_sim C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.3.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(lvgl)

include(${CMAKE_CURRENT_LIST_DIR}/../core.cmake)
target_compile_options(panel_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -fno-rtti>)

set(UI ${ROOT}/components/ui)
file(GLOB UI_SOURCES ${UI}/*.cpp ${UI}/screens/*.cpp ${UI}/fonts/*.c)
find_package(Threads REQUIRED)
add_library(panel_ui STATIC ${UI_SOURCES})
target_include_directories(panel_ui PUBLIC ${UI}/include ${UI} ${CMAKE_CURRENT_LIST_DIR}/../host ${lvgl_SOURCE_DIR})
target_link_libraries(panel_ui PUBLIC panel_core lvgl X11 m Threads::Threads)

add_executable(music_panel_sim main.cpp art_loader.cpp ${CMAKE_CURRENT_LIST_DIR}/../host/ws_posix.cpp)
target_link_libraries(music_panel_sim panel_ui)

# End-to-end: the real UI + core on the mock server, headless, driven by simulated touch.
enable_testing()
add_executable(panel_e2e e2e.cpp)
target_link_libraries(panel_e2e panel_ui)
add_test(NAME e2e COMMAND panel_e2e)
add_test(NAME e2e_setup COMMAND panel_e2e --setup)
set_tests_properties(e2e e2e_setup PROPERTIES LABELS e2e TIMEOUT 180)
