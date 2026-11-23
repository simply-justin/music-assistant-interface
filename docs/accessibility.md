# Accessibility

The panel targets **WCAG 2.2 level AA** where it applies to a touch-only wall display without a screen reader. This page records how the panel meets each relevant criterion. The last review (22 November 2026) took place in the simulator, in both themes, with every accent colour and corner style.

## Summary

| Criterion | Status |
|---|---|
| 1.4.1 Use of colour | Met |
| 1.4.3 Contrast (minimum) | Met in both themes |
| 1.4.4 Resize text, 1.4.12 Text spacing | Met within the fixed type scale |
| 1.4.11 Non-text contrast | Met |
| 2.2.2 Pause, stop, hide | Met with Reduce motion |
| 2.3.3 Animation from interactions | Met with Reduce motion |
| 2.5.8 Target size (minimum) | Met; every target is at least 56 px |
| 3.3.1 Error identification | Met |
| Keyboard and screen-reader criteria | Not applicable: the device has neither |

## Contrast

Required: 4.5:1 for text (1.4.3) and 3:1 for controls and graphics (1.4.11). The ratios below come from the colour tokens in `components/ui/theme.cpp`.

| Pair | Dark | Light |
|---|---|---|
| `text` on page, sidebar, tile and selected item | ≥ 15:1 | ≥ 14:1 |
| `text2` on any surface | ≥ 6.4:1 | ≥ 6.6:1 |
| `text3` on any surface | ≥ 4.8:1 | ≥ 4.6:1 |
| `warning` on any surface | ≥ 7:1 | ≥ 5.2:1 |
| Label on any accent colour | ≥ 8.7:1 | ≥ 5.0:1 |
| Accent against the page (non-text) | ≥ 8.2:1 | ≥ 4.6:1 |
| `text2` on the artwork-tinted Now Playing page | ≥ 4.9:1 | ≥ 5.8:1 |
| `text` on artwork-tinted cards | ≥ 10:1 | ≥ 12:1 |

Artwork-tinted cards use only the primary text colour, because secondary grey could fall to about 3:1 on mid-tone covers.

## Touch targets

Every control has a tap area of at least 56 px (2.5.8 requires 24 px; 56 px allows for use at arm's length on a wall). Where the visible control is smaller, its tap area extends beyond it:

| Control | Visible | Tap area |
|---|---|---|
| Sidebar items, pills | 40 px | 56 px (8 px added above and below) |
| Tabs, chips | 48 px | 56 px |
| Icon buttons | 48 px | 56 px (`ext_click_area`) |
| Rows | 56–72 px | The whole row |
| Sliders | 4 px track | 56 px |

## Motion

The equalizer on the playing item loops while music plays. **Settings › Appearance › Reduce motion** switches off:

- the equalizer animation,
- the shrink on press,
- the artwork fade-in.

Screen navigation is always instant.

## Colour and text

- **Colour is never the only signal.** The playing item has an equalizer, a highlight and the accent colour. Errors have an icon and a sentence.
- **Text is at least 14 px.** Labels stay on one line and end with "…" instead of overlapping their neighbours.
- **Errors use words, not just colour** (3.3.1). For example, setup says when the Wi-Fi password is wrong, when nothing answers at a server address, and when the server rejects a token, and it says what to do next.

## Still to verify on the device

- Readability at 1–2 m and under glare.
- Touch accuracy on the items that look 40 px high.
- The frame-rate cost of the press shrink, which renders the tile as a layer. If it costs frames, it goes.
