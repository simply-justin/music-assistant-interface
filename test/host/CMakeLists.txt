# Host (PC) build of the portable layers + their tests:
#   cmake -S test/host -B test/host/build && cmake --build test/host/build -j
#   ctest --test-dir test/host/build --output-on-failure      # or run unit_tests / integration_tests
cmake_minimum_required(VERSION 3.16)
project(panel_host_tests CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_LIST_DIR}/../core.cmake)
target_compile_options(panel_core PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -fno-rtti> -fsanitize=address,undefined)
target_link_options(panel_core PUBLIC -fsanitize=address,undefined)

enable_testing()

# Unit: pure functions and single classes.
add_executable(unit_tests unit_tests.cpp ${ROOT}/components/ui/text.cpp)  # ui/text has no LVGL
target_include_directories(unit_tests PRIVATE ${ROOT}/components/ui)
target_link_libraries(unit_tests panel_core)
add_test(NAME unit COMMAND unit_tests)
set_tests_properties(unit PROPERTIES LABELS unit)

# Integration: App + every feature against the mock server and the fake platform.
add_executable(integration_tests integration_tests.cpp)
target_link_libraries(integration_tests panel_core)
add_test(NAME integration COMMAND integration_tests)
set_tests_properties(integration PROPERTIES LABELS integration TIMEOUT 120)

# PC test app against a real server (or --mock).
add_executable(panel_cli panel_cli.cpp ws_posix.cpp)
target_link_libraries(panel_cli panel_core)
