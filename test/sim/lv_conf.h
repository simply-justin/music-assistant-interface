// LVGL 9.3 config for the PC simulator. The device uses Kconfig (sdkconfig) instead; keep the
// visual settings (colour depth, fonts, widgets) in sync with it.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16  // RGB565, same as the panel
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_DEF_REFR_PERIOD 16
#define LV_USE_OS LV_OS_NONE

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_FONT_MONTSERRAT_14 1  // only LVGL's internal default; the UI uses Inter
#define LV_USE_FONT_PLACEHOLDER 1

#define LV_USE_X11 1
#define LV_X11_DIRECT_EXIT 1
#define LV_X11_DOUBLE_BUFFER 0
#define LV_X11_RENDER_MODE_PARTIAL 1
#define LV_X11_RENDER_MODE_DIRECT 0
#define LV_X11_RENDER_MODE_FULL 0

#endif
