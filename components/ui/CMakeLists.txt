# The panel UI (LVGL 9). Device display/touch wiring comes with the board package in Phase 2b;
# until then this compiles for the ESP32-P4 and runs in the PC simulator (test/sim).
file(GLOB srcs "*.cpp" "screens/*.cpp" "fonts/*.c")
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "." REQUIRES app lvgl__lvgl)
