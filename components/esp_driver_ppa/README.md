# esp_driver_ppa (ESP-IDF 5.5.6, patched)

A copy of ESP-IDF 5.5.6's `components/esp_driver_ppa` with one change in `src/ppa_srm.c`:
Espressif's temporary fix for the PPA hang (`ppa_ll_srm_bypass_mb_order(..., true)`), which
esp_lvgl_adapter ships as a patch for ESP-IDF 6.0. Without it, rotating the screen (the 10.1" panel
is portrait, the UI landscape) hangs the PPA and freezes the display.

A project component with the same name replaces ESP-IDF's, so the fix applies on every machine and
in CI. Remove this folder once ESP-IDF includes the fix.
