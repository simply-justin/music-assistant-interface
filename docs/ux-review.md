# UI / UX and WCAG review (22 Nov 2026, direction A)

Reviewed in the simulator (mock and live server, dark and light, every accent, all roundings).
Target: WCAG 2.2 AA where it applies to a touch-only wall panel without a screen reader.

## Contrast (1.4.3 text 4.5:1, 1.4.11 non-text 3:1)

Computed from `components/ui/theme.cpp` (script in the commit message's review notes):

| Pair | Dark | Light |
|---|---|---|
| text on page / sidebar / tile / selected | ≥ 15:1 | ≥ 14:1 |
| text-2 (secondary) on any surface | ≥ 6.4:1 | ≥ 6.6:1 |
| text-3 (tertiary) on any surface | ≥ 4.8:1 | ≥ 4.6:1 |
| warning on any surface | ≥ 7:1 | ≥ 5.2:1 |
| label on accent (teal, blue, orange, pink, green) | ≥ 8.7:1 | ≥ 5.0:1 |
| accent vs page (non-text) | ≥ 8.2:1 | ≥ 4.6:1 |
| text-2 on the art-derived Now Playing page | ≥ 4.9:1 | ≥ 5.8:1 |
| text on art-tinted cards (Now on…, Top result) | ≥ 10:1 | ≥ 12:1 |

Fixed during the review:
- **Toggle "on" was invisible in dark/neutral** (white knob on a white track) → the knob now
  uses the colour that sits on the accent; "off" is a grey knob on an outlined track.
- Dark tertiary text 4.47:1 on the selected item → `#8F8F8F` (4.8:1).
- Light warning 4.40:1 on tiles → `#8A4B00` (5.2:1).
- Grey text on art-tinted cards could fall to ~3:1 with mid-tone covers → those cards use only
  the main text colour.

## Touch targets (2.5.8, spec minimum 56 px)

All controls have a ≥ 56 px tap area, even when they look smaller:
sidebar items 40 + 2×8, pills 40 + 2×8, tabs and chips 48 + 2×4, icon buttons 48 → 56 via
`ext_click_area`, rows 56, sliders 4 px track with a 56 px hit area.

## Motion (2.2.2, 2.3.3)

The equalizer loops while music plays. Settings › Appearance › **Reduce motion** switches off the
equalizer animation, the press shrink and the artwork fade (stored with the appearance).
Full-screen navigation was already instant.

## Other checks

- 1.4.1 Use of colour: "now playing" = equalizer + highlight + accent; errors = icon + text.
- 1.4.4 / 1.4.12: text never below 14 px; labels stay on one line and end with "…" instead of
  overlapping (was wrapping into neighbours before).
- 3.3.1 Errors (setup): wrong password, server not found, token rejected are stated in words.
- Not applicable on the panel: keyboard focus order, screen-reader names (no AT on the device).

## UX fixes in this round

- **Home tabs:** filter pills moved next to the greeting (40 px, readable), section headers share
  one layout with "See all" on the same baseline, the page fits above the player bar.
- **Volume slider clipping:** the knob was cut off at 100 % at the right edge of the player bar.
- **Player bar with nothing playing:** no transport buttons that do nothing.
- **Station/artist names:** label wider than the circle, so names fit.
- **Settings pages:** extra bottom space so the last item is not hidden under the player bar.

## Still to check on the real panel

Viewing distance and glare (1–2 m), touch accuracy of the 40 px-looking items, frame rate of the
press-shrink transform (it renders the tile as a layer; drop it if it costs frames).
