# Design

The visual system of the panel: layout, design tokens, components and motion. The source of truth is the code: tokens in `components/ui/theme.cpp`, components in `components/ui/kit.cpp`. This document explains them and the rules behind them.

## Principles

1. **Readable from across the room.** Text is never smaller than 14 px. Contrast meets WCAG 2.2 AA in every theme.
2. **Easy to hit.** Every touch target is at least 56 px, even when the visible control is smaller.
3. **Calm.** Flat colour, no shadows, no gradients, no blur. The artwork supplies the colour.
4. **Fast on a microcontroller.** Screen changes are instant and animations stay small and cheap. Nothing may force LVGL to render a layer.
5. **Media, not services.** Screens group content by type (tracks, albums, playlists, radio), never by provider.

## Layout

The screen is 1280 × 800 in landscape.

```
┌──────────┬──────────────────────────────────────────────┐
│          │                                              │
│ Sidebar  │  Page                                        │
│  216 px  │  32 px padding                               │
│          │                                              │
├──────────┴──────────────────────────────────────────────┤
│ Player bar  96 px                                       │
└─────────────────────────────────────────────────────────┘
```

| Region | Contents |
|---|---|
| Sidebar | Home, Search, Library, Queue; Settings at the bottom |
| Player bar | Artwork, title and artist (320 px) · transport and progress (480 px) · queue, speaker, volume (320 px) |
| Page | The current screen. Now Playing and first-time setup cover the whole screen. |

When nothing is playing, the player bar keeps its layout: a placeholder cover, "Nothing playing", and the controls dimmed to 40 %.

### Grid

- Spacing follows an 8 px grid. Neighbouring targets are at least 8 px apart.
- Home uses one six-column grid of 152 px cells with 16 px gaps. Shortcut tiles, Recently played and Stations all align to it. Sections are 32 px apart.
- Settings is master-detail: categories on the left (296 px), the selected page on the right. Rows are 64 px, grouped in cards and separated by hairlines.

## Tokens

Screens use token names, never raw values.

### Colour

| Token | Dark | Light | Use |
|---|---|---|---|
| `bg` | `#0A0A0A` | `#F6F6F7` | Page |
| `side` | `#000000` | `#FFFFFF` | Sidebar and player bar |
| `surface` | `#161616` | `#ECECEE` | Tiles and cards |
| `elevated` | `#242424` | `#E0E0E4` | Selected item, keys, pressed state |
| `hairline` | `#1C1C1C` | `#E4E4E7` | Dividers |
| `border` | `#333333` | `#CFCFD4` | Outlined controls |
| `text` | `#FFFFFF` | `#111111` | Primary text |
| `text2` | `#A3A3A3` | `#52525B` | Secondary text, inactive icons |
| `text3` | `#8F8F8F` | `#63636B` | Captions, durations |
| `warning` | `#E7B14C` | `#8A4B00` | Reconnecting, unavailable |
| `danger_text` | `#F08C7E` | `#B42318` | Errors, destructive text actions |
| `danger_fill` | `#B3261E` | `#B3261E` | Destructive buttons |
| `live` | `#C2410C` | `#C2410C` | LIVE tag |

### Accent

The accent marks the play button, the active tab, the equalizer and selected items. The user chooses it in **Settings › Appearance**.

| Accent | Dark | Light |
|---|---|---|
| Neutral (default) | `#FFFFFF` | `#111111` |
| Teal | `#5CC8B8` | `#0F766E` |
| Blue | `#6EA8FF` | `#1D4ED8` |
| Orange | `#F5A15C` | `#C2410C` |
| Pink | `#F28AB8` | `#BE185D` |
| Green | `#5FD39A` | `#15803D` |

### Artwork colour

Now Playing takes its background from the artwork. The panel clamps that colour to a low lightness and saturation, so text stays readable on any cover. It is a flat fill: no gradient and no blur.

### Corner radius

The user chooses one of three corner styles.

| Token | Sharp | Normal (default) | Round | Use |
|---|---|---|---|---|
| `r_art` | 2 | 6 | 12 | Covers |
| `r_control` | 4 | 8 | 14 | Tiles, rows, fields, keys |
| `r_card` | 4 | 8 | 16 | Cards, toasts, banners |
| `r_sheet` | 8 | 12 | 20 | Dialogs and sheets |
| `r_button` | 4 | pill | pill | Text buttons and chips |

### Typography

Inter, eight fixed sizes. Glyphs cover Latin, Latin-1 and Latin Extended-A, which is enough for most European languages.

| Token | Size / weight | Use |
|---|---|---|
| `label` | 14 / SemiBold | Overlines, tags. The minimum size. |
| `meta` | 16 / Medium | Secondary lines, durations |
| `body` | 18 / SemiBold | Row titles, buttons, chips |
| `lead` | 20 / Medium | Paragraphs, empty states |
| `h2` | 24 / SemiBold | Section headings |
| `h1` | 36 / Bold | Page titles |
| `display` | 48 / Bold | Now Playing title, setup titles |
| `clock` | 140 / Light | The idle clock; digits only |

Icons are Phosphor, rendered at 22, 28, 32 and 48 px.

### Size

| Token | Value | Use |
|---|---|---|
| `kPrimary` | 64 px | Primary buttons |
| `kSecondary` | 56 px | Icon buttons, chips |
| `kMinTap` | 56 px | The smallest tap area |
| `kNavH` | 40 px visible, 56 px tap area | Sidebar items |

## Components

Every screen uses only the components in `components/ui/kit.hpp`. Every interactive component has a pressed state, and a disabled, selected or on state where one applies.

| Component | Notes |
|---|---|
| Button | Primary, secondary, danger and ghost |
| Icon button | Round, 56 px by default; optional filled background |
| Play button | Filled accent circle |
| Chip, tab, pill | Filters and segmented choices |
| Media row | Title, subtitle, source badge, "+N sources" tag, playing state |
| Media card, favourite tile | Artwork with a label; long-press opens a preview |
| Virtual list | Windowed list for long content; only visible rows exist |
| Slider | 4 px track with a 56 px tap area |
| Toggle | The whole row is the tap target |
| Keyboard | Letters, numbers and a context action key (Search, Join, Connect) |
| Sheet | Dialog on a scrim; no shadow |
| Toast | One at a time, optionally with an action such as Undo |
| Section header | Title with an optional "See all" link on the same baseline |
| Equalizer | Animated bars on the playing item |
| Badge, tag | Source letter; neutral, warning or live tag |

## Motion

| What | How |
|---|---|
| Screen navigation | Instant. Screens are kept alive and only their data changes. |
| Pressed state | Instant colour change, plus a slight shrink on tiles |
| Sheets and dialogs | The scrim appears instantly; the sheet rises 16 px over 150 ms. Closing is instant. |
| Toast | Rises 24 px over 150 ms and disappears after 4 seconds. A new toast replaces the current one. |
| Artwork | Fades in when it arrives from the network; appears instantly from the cache |
| Equalizer | Loops while music plays |
| Progress bar | Updates once a second |

**Reduce motion** (Settings › Appearance) switches off the equalizer animation, the press shrink and the artwork fade.

What is never used: blur, drop shadows, opacity on containers with children (it forces a layer render), gradient fills, animated screen transitions, shimmer placeholders.

## Duplicate handling

Music Assistant often finds the same recording on several services. The panel shows it once.

1. **One row per recording.** The panel groups items by URI, then by Music Assistant's provider mappings, then by ISRC, then by a strict title, artist and duration match.
2. **One source badge.** The row shows the source that will actually play: library first, then favourites, then the preferred-source order from Settings.
3. **"+N sources".** When alternatives exist, a tag shows how many. **Track options › Play from** lists each alternative with its source and duration.
4. **Versions stay separate.** Live, remastered, acoustic, edit, remix and deluxe versions are different rows.
5. **Conservative merging.** Artists merge only when the name is unambiguous. Playlists and radio stations never merge.
6. **Add to queue adds one source**, never the whole group.
7. **Search never splits results by provider.** A provider that fails to answer shows a warning banner instead.

## References

These patterns follow well-known apps, found through [Mobbin](https://mobbin.com):

- Sidebar and player bar with a speaker button: Spotify
- Home grid: SoundCloud, Calm
- Toast with an action: Suno
- Guarded destructive dialog: Headway, Reddit
- Long-press preview: Reddit, Suno
- Display settings and schedules: Apple Watch, Garmin Connect
- Inline field errors: Aave, X
