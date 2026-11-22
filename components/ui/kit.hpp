#pragma once
// The component kit (canvas board "Component library & states"). Screens are built only from
// these. Every interactive part has a pressed state; sizes follow the spec's touch targets.
#include <functional>
#include <string>

#include "core/model.hpp"
#include "lvgl.h"
#include "theme.hpp"

namespace ui::kit {

using Action = std::function<void()>;

void on_click(lv_obj_t* obj, Action action);
void on_long_press(lv_obj_t* obj, Action action);
// Runs `action` after the current event. Use it when a click rebuilds the container it sits in.
void later(Action action);

// Layout containers without any default styling.
lv_obj_t* column(lv_obj_t* parent, int32_t gap = 0);
lv_obj_t* row(lv_obj_t* parent, int32_t gap = 0);
lv_obj_t* spacer(lv_obj_t* parent);  // flex-grow 1
lv_obj_t* scroll_column(lv_obj_t* parent, int32_t gap = 0);

// Text. `text()` cleans emoji and clips long text with "…".
lv_obj_t* text(lv_obj_t* parent, const std::string& s, const lv_font_t* font, lv_color_t colour, bool wrap = false);
lv_obj_t* overline(lv_obj_t* parent, const std::string& s, lv_color_t colour = theme::text2());
lv_obj_t* icon(lv_obj_t* parent, const char* glyph, int size, lv_color_t colour = theme::text2());

enum class Button { Primary, Secondary, Danger, Ghost };
lv_obj_t* button(lv_obj_t* parent, Button kind, const char* glyph, const std::string& label, Action action);
// Round icon button: 56 by default (secondary target), `filled` = elevated background.
lv_obj_t* icon_button(lv_obj_t* parent, const char* glyph, Action action, int32_t size = 56, int icon_size = 28,
                      bool filled = false, lv_color_t colour = theme::text());
// Big round play/pause (accent).
lv_obj_t* play_button(lv_obj_t* parent, bool playing, Action action, int32_t size);
// Action chip: rectangular, 1 px border; `selected` = a toggle that is on (accent).
lv_obj_t* chip(lv_obj_t* parent, const char* glyph, const std::string& label, bool selected, Action action);
// Tab / filter: plain text, the selected one white with an accent underline.
lv_obj_t* tab(lv_obj_t* parent, const std::string& label, bool selected, Action action);
// Small filter pill (Home: All / Music / Radio): 32 px visible, 56 px tap area.
lv_obj_t* pill(lv_obj_t* parent, const std::string& label, bool selected, Action action);
// Now-playing indicator: three bars, animated while `playing`.
lv_obj_t* equalizer(lv_obj_t* parent, lv_color_t colour, bool playing);
// Micro-animation: shrink slightly while pressed (tiles, cards, buttons).
void press_scale(lv_obj_t* obj);
lv_obj_t* back_chip(lv_obj_t* parent, const std::string& label, Action action);

// Artwork box: placeholder colour (+ type icon for radio) until the real image arrives.
// `bg`: the colour behind it, used to pre-round the corners.
lv_obj_t* art(lv_obj_t* parent, const core::MediaItem& item, int32_t size, int32_t radius, lv_color_t bg = theme::bg(),
              std::function<void(uint32_t)> on_ready = {});
lv_obj_t* badge(lv_obj_t* parent, const std::string& provider);  // 20 px grey letter
lv_obj_t* tag(lv_obj_t* parent, const std::string& s, lv_color_t colour = theme::text2());

// Long list with fixed-height rows: only the rows in view (+ a few spare) exist as LVGL objects;
// they are created/deleted while scrolling. `make` builds row `i` inside the given parent.
// Fill the parent (flex-grow or a fixed height): the list scrolls itself.
lv_obj_t* virtual_list(lv_obj_t* parent, size_t count, int32_t row_height, int32_t gap,
                       std::function<lv_obj_t*(lv_obj_t* list, size_t index)> make);

struct RowOptions {
    int32_t height = 56;
    bool playing = false;
    bool show_duration = true;
    Action on_tap, on_more, on_remove;
};
lv_obj_t* media_row(lv_obj_t* parent, const core::MediaItem& item, const RowOptions& opt);
lv_obj_t* media_card(lv_obj_t* parent, const core::MediaItem& item, const std::string& sub, Action on_tap,
                     int32_t size = 144);
lv_obj_t* favourite_tile(lv_obj_t* parent, const core::MediaItem& item, Action on_tap, Action on_long_press);

// Slider: 8 px track, 36 px thumb, 56 px hit area. `on_release` fires once when the finger lifts.
lv_obj_t* slider(lv_obj_t* parent, int value, std::function<void(int)> on_release, lv_color_t fill = theme::accent());
lv_obj_t* toggle(lv_obj_t* parent, bool on, std::function<void(bool)> on_change);

// On-screen keyboard (keys 60 px) typing into `input`. Special keys are words in the current
// language, not symbol glyphs (the Inter fonts don't contain LVGL's symbols). `on_done`: the
// confirm key. Labels: delete, done (e.g. "Search"), space.
lv_obj_t* keyboard(lv_obj_t* parent, lv_obj_t* input, const std::string& del, const std::string& done,
                   const std::string& space, Action on_done);

// A sheet on the top layer: scrim + panel. Tapping the scrim closes it. Returns the panel.
lv_obj_t* sheet(int32_t width, Action on_close = {});
void close_sheets();
void toast(const std::string& message, const std::string& action_label = {}, Action action = {}, bool error = false);

std::string subtitle(const core::MediaItem& item);  // "Coldplay", "Playlist", "Album · Coldplay", ...
std::string provider_name(const std::string& domain);  // "Spotify", "Your library", ... (for "Play from")

}  // namespace ui::kit
