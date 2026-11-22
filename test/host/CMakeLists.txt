# Host (PC) build of the portable layers + their tests:
#   cmake -S test/host -B test/host/build && cmake --build test/host/build && test/host/build/host_tests
cmake_minimum_required(VERSION 3.16)
project(panel_host_tests CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_LIST_DIR}/../core.cmake)
target_compile_options(panel_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -fno-rtti> -fsanitize=address,undefined)
target_link_options(panel_core PUBLIC -fsanitize=address,undefined)

add_executable(host_tests tests.cpp ${ROOT}/components/ui/text.cpp)  # ui/text has no LVGL
target_include_directories(host_tests PRIVATE ${ROOT}/components/ui)
target_link_libraries(host_tests panel_core)

# PC test app against a real server (or --mock).
add_executable(panel_cli panel_cli.cpp ws_posix.cpp)
target_link_libraries(panel_cli panel_core)
