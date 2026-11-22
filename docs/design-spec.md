# Unified Music Wall Panel: design specification (Stage A)

Canvas: https://claude.ai/artifact/3t58nGkmMF25jmXUiqn1Rp (45 artboards, 1280 × 800 landscape, plus a component sheet).
Status: **awaiting approval**. No firmware is written until the design is approved.

## 1. Tokens

### Colour

| Token | Value | Use | Contrast |
|---|---|---|---|
| `bg` | `#0B0F14` | Screen background | n/a |
| `surface` | `#151B23` | Cards, rows (selected), mini-player, rail active | n/a |
| `elevated` | `#202834` | Buttons on a surface, keys, sheets' inner rows | n/a |
| `line` | `#2A3442` | 1 px borders (sheets, tags, banners), slider track, function keys | n/a |
| `text` | `#F5F7FA` | Primary text | 17.9 on bg, 13.8 on elevated |
| `text-2` | `#A0AAB8` | Secondary text, inactive icons | 8.2 on bg, 6.3 on elevated |
| `text-3` | `#8B95A4` | Captions, durations, overlines | 6.3 on bg, **4.9 on elevated (floor)** |
| `accent` | `#5CC8B8` | Active nav, primary button, toggles on, playing row | 9.5 on bg; `bg` text on it 9.5 |
| `warning` | `#E7B14C` | Reconnecting, provider down, "Unavailable" | 9.9 on bg |
| `danger-text` | `#F08C7E` | Error text, destructive text actions | 8.0 on bg |
| `danger-fill` | `#B3261E` | Destructive button (white text 6.5) | n/a |
| `live` | `#C2410C` | LIVE pill (white text 5.2; the old `#E8572A` failed at 3.6) | n/a |
| `pressed` | +12 % white over the control (or −18 % on light controls) | Pressed state for every control | n/a |
| `scrim` | `#05080C` at 78 % | Behind sheets and dialogs | n/a |

**Art-derived Now Playing background.** Use the dominant colour from `metadata/get_image_palette` (newer servers). Otherwise use the average of the 80 px thumbnail, computed when the image is decoded. Clamp it to HSL lightness ≤ 16 % and saturation ≤ 45 % so that `text` stays ≥ 12:1 and `text-3` stays ≥ 4.5:1 on any artwork. Use a flat fill: no gradient and no blur.

### Typography: Inter, 8 fonts (each one `lv_font_conv` output)

| Font | Size / weight | Use |
|---|---|---|
| `label` | 14 / SemiBold | Overlines (uppercase), rail labels, tags. **Minimum size on device.** |
| `meta` | 16 / Medium | Secondary lines, captions, durations |
| `body` | 18 / SemiBold | Row titles, buttons, chips, settings rows |
| `lead` | 20 / Medium | Paragraphs (setup, empty states), Now Playing album line |
| `h2` | 24 / SemiBold | Section headings, artist on Now Playing, dialog titles in sheets |
| `h1` | 36 / Bold | Page titles, dialog headlines, volume value |
| `display` | 48 / Bold | Now Playing title, setup titles |
| `clock` | 120–160 / Light, **digits, colon and space only** | Ambient clock |

Glyph range: Basic Latin + Latin-1 + Latin Extended-A (covers Dutch, German, French, Spanish, Nordic and Polish titles), plus `–—‘’“”•…·×−`. Use 4 bpp. Estimated flash use is about 0.6–0.9 MB in total; the `display` font dominates it. Missing glyphs (CJK, Cyrillic and so on) render as the fallback box; see open question 3. The mock-ups still contain a few off-scale weights; this table is what counts.

### Spacing, radius, size

- 8 px grid. Page padding is 40 px. The gap between neighbouring targets is at least 8 px.
- Radius: 8 for small art in rows, 12 for tiles, cards' art and keys, 14–16 for list rows and fields, 20 for cards, 24 for sheets, full for pills, chips and round buttons.
- Touch targets: **primary 64** (Play, Continue, Join, mini-player play/pause, Now Playing prev/next 80, play/pause 112). **Secondary 56** (icon buttons, chips, segmented control, steppers). Rows are 64–88 and the whole row is the target. A slider has an 8 px track, a 36 px thumb and a **56 px hit area** (`lv_obj_set_ext_click_area`). Keyboard keys are 60.
- Layout: a 96 px rail and a 96 px mini-player on browsing screens. Home, Queue and Now Playing have no mini-player.

### Motion (durations are all ≤ 200 ms)

| What | How |
|---|---|
| Pressed state | Instant (a state style, no animation) |
| Full-screen navigation | **Instant** (`lv_screen_load` without animation; screens are kept alive and only data is re-bound) |
| Sheet or dialog | Scrim appears instantly; the panel moves up 16 px and fades in over 150 ms with ease-out. Closing is instant. |
| Toast | Moves up 24 px over 150 ms, stays 4 s, fades out over 120 ms. One toast at a time; a new one replaces the old one. |
| Volume overlay | Same as a sheet; closes itself 4 s after the last touch |
| Progress bar | Updated at 1 Hz from a local clock, re-synced on `queue_time_updated`. Only the bar's area is invalidated. |
| Spinner | 36 px arc, one turn per second. Only on Boot, setup and the search status. |
| Skeleton | Static blocks, **no shimmer** |
| Everything else | Not animated: no artwork cross-fades and no list insert animations |

## 2. Component inventory (artboard "Component library & states")

Every screen is built only from these. Every interactive component has a **default**, **pressed** and, where it applies, **disabled**, **selected** or **on** state.

| Component | States | LVGL notes |
|---|---|---|
| Rail item (80×76) | default, active (surface fill, accent icon), pressed | `lv_button` + A8 icon `lv_image` + `label` font. Built once and never rebuilt. |
| Mini-player (96) | playing, paused, nothing playing (hidden) | One instance on the top layer, shown or hidden per screen. The progress strip is a 3 px `lv_bar`. |
| Media row (72; 64 in detail lists) | default, pressed, playing (accent title, surface fill), unavailable (warning tag), duplicate (+N source tag) | Recycled objects in a virtualized list: a pool of about 12 row objects, re-bound on scroll. Long-press opens options. |
| Media card (160 art) | default, pressed | Recycled grid cells. The art is an `lv_image` with a cached, pre-scaled RGB565 buffer. |
| Favourite tile (80 art) | default, pressed, long-press preview | The Home grid is fixed at 6 and is not virtualized. |
| Chip (56) | default, selected, pressed | Pill-shaped `lv_button`. Filters **media type only**, never the provider. |
| Provider badge (20) | n/a | A8 icon per provider in `text-2` on `elevated`. Always the last item before the duration. |
| Tag (32) | neutral, warning, live | Label with a 1 px border. Text uses the `label` font. |
| Button | primary 64, secondary 56, danger 64, text-only, disabled | Pressed = colour mix. No gradients. |
| Icon button (56, round) | default, pressed, disabled, active (accent) | n/a |
| Toggle (64×36) | off, on, pressed (via its row) | `lv_switch`. The **whole row** is the click target. |
| Segmented control (56) | option, selected, pressed | `lv_buttonmatrix` with one checked button |
| Slider | default, dragging | `lv_slider` with ext click area 24. Volume sends a throttled command (≤ 5/s) and a final one on release. |
| Stepper (−, value, +) | n/a | Used for the night schedule (15 min steps) and as an alternative way to set volume |
| Sheet / dialog | n/a | `surface` fill, 1 px `line` border, radius 24, on a static scrim. **No shadow.** |
| Banner (≥ 72) | info, warning, error; optional action | Inline, pushes content down. It is not an overlay. |
| Toast (72) | plain, with action (Undo / Add again / Play next) | Light on dark for visibility from across the room. Sits above the mini-player. |
| Keyboard | letters, function keys, action key, pressed | One `lv_keyboard` with custom maps (letters, 123, URL/host). Keys are 60 px. The action key reads Search, Join or Connect depending on context. |
| Field (72) | default, focused (accent border), error (danger border plus message below) | `lv_textarea`, one line. The password field has a show/hide control. |
| Skeleton | row, card, top result | Plain `elevated` rectangles |
| Spinner | n/a | `lv_spinner` with a small invalidation area |

**Icon set (fixed, A8, rendered at 22 / 28 / 32 / 48 px):** home, search, library, queue, settings, play, pause, stop, prev, next, shuffle, repeat, heart, heart-filled, plus, minus, more, close, chevron down/up/left/right, speaker, volume, mute, check, warning, info, wifi, server, radio, trash, undo, play-next, add-to-queue, lock, eye, refresh, moon, sun, phone, chart, power, plus 4 provider badges. That is 45 glyphs × 4 sizes, about 120 KB at A8. A single icon font is an equal alternative.

**What not to build in LVGL:** blur, drop shadows, `opa` on containers with children (it forces a layer render; use per-child disabled styles instead), gradient washes, animated screen transitions, shimmer, rounded image clipping on large images (pre-round artwork corners when scaling, or overlay a 4-corner mask on small art only).

## 3. Screens and navigation

Rail: **Home · Search · Library · Queue · Settings**. The back control is a text chip labelled with the previous screen ("‹ Library", "‹ Search"), driven by a back stack with a maximum depth of 8. Tapping a rail item clears the stack.

| Area | Screens |
|---|---|
| Home | Home, Home – offline, Home – favourite long-press (preview + Play / Shuffle / Play next / Add to queue / Open / Remove from favourites) |
| Search | Start (recent searches, your stations), Typing (live first results + keyboard), Results (Top result + Tracks + Albums & playlists; chips All / Tracks / Artists / Albums / Playlists / Radio), Loading (progressive), No results + "provider didn't respond" banner, Added-to-queue toast |
| Library | Tabs **Favourites** · Playlists · Albums · Artists · Tracks · Radio. Favourites is new and mixes all media types. The Artists tab is new and leads to Artist detail. The Radio tab is new: a station list with play/stop and heart. |
| Detail | Album, Artist, Playlist. Album has the "already in the queue" toast with **Add again**. |
| Now Playing | Track, Radio (no seek, prev/next, shuffle or repeat; Stop instead of pause), Playback error (banner: play from an alternative source / Skip), Volume overlay |
| Queue | Queue, Empty, Clear (guarded dialog with "Keep queue" and "Clear queue"), Service unavailable (banner plus "Unavailable" tags) |
| Overlays | Track options (with "Play from" for duplicates), Speaker picker |
| Playlists | Library › Playlists tab → Playlist detail (Edit only when `is_editable`) → Edit mode (× removes a song; guarded "Delete playlist"). Track menu › "Add to playlist" → picker of editable playlists + "New playlist" (name + keyboard). Home: "Recently played / Your playlists" switch on the bottom shelf. No reordering inside a playlist: the server has no command for it. |
| Settings | Main (Play music on + "This panel" links), Screen & night mode, Wi-Fi, Music Assistant (connection, services, preferred-source order), Language (English / Nederlands), About & diagnostics (FPS overlay, touch points, restart, reset) |
| System | Starting up, Ambient (dimmed clock, tap to wake) |
| Setup | Welcome → Wi-Fi → Password (error: wrong password) → Server (error: not found / bad address) → Sign in (error: token rejected) → Speaker → Done |

**Changes to existing screens in this review:**
- Type scale consolidated (13 → 14, 15 → 16, 17 → 18, 19 → 20, 22/26/28 → 24, 32/40 → 36, 44/52/56 → 48).
- Chips 48 → 56 px; the Settings segmented control 48 → 56 px; primary actions → 64 px.
- Pressed states added to every control.
- Shadows removed from the dragged queue row and from the dialogs.
- The LIVE pill fixed for contrast.
- The speaker chip now opens the Speaker picker everywhere, and the mini-player volume button opens the Volume overlay.
- Home: the empty "Continue listening" stub is removed, and the Now Playing card's prev / play / next are real buttons (tapping the card body opens Now Playing).
- Now Playing: "Add to playlist" is cut (it needs a playlist picker a family won't use), and "More" opens Track options.
- Queue: drag handles are cut. Tap a row to play it, tap × to remove it, long-press for Play next / Move up / Move down. Drag-reorder in a recycled LVGL list is expensive and fiddly to use.
- Settings: split into a main page and sub-pages. The preferred-source order moved to Settings › Music Assistant and uses ↑ buttons instead of drag.

## 4. Duplicate presentation rules

1. **One row per recording.** The grouping itself follows the conservative dedupe rules in the prompt: same uri → MA provider mappings → ISRC equal / different → strict fallback match.
2. The row shows **one** provider badge: the source that will actually play (library › favourite › preferred-source order). Its position in the list stays stable when later results arrive.
3. When alternatives exist, a `+N source(s)` tag appears before the badge. Tapping the row plays the preferred source. **Track options › Play from** lists every alternative with its badge and duration.
4. Live, remastered, acoustic, edit, remix and deluxe versions are **separate rows**. The version text stays in the title ("Yellow – Live in Buenos Aires").
5. Artists are merged only when the name is unambiguous in the batch. Playlists and radio stations are never merged.
6. "Add to queue" always adds the **one** chosen source, never the group.
7. On Album, Artist and Playlist detail, the header names the sources ("In your library · also on YouTube Music"). Rows inside a detail list show no badge when they all share one source.
8. Search status: "4 sources searched", "Searching · 1 of 4 sources", or a warning banner when one provider fails. Results are never split by provider.

## 5. LVGL implementation notes (per component group)

- **Screens:** build each top-level screen once at boot (Home, Search, Library, Queue, Now Playing, Settings), then re-bind data on show. Detail screens share one instance per type. Overlays live on `lv_layer_top()`.
- **Lists and grids:** virtualized with recycled objects. Scroll uses `LV_SCROLL_SNAP_NONE` and no elastic overscroll (`LV_OBJ_FLAG_SCROLL_ELASTIC` off) to keep redraws small. Rows have fixed heights so offsets are arithmetic.
- **Artwork:** request `/imageproxy` at 80 px for rows, 160 px for cards, 256 px for the Home card and 512 px for Now Playing. Decode with the hardware JPEG decoder off the GUI task, pre-scale to the exact display size, and store RGB565 in a PSRAM LRU with pin counts. Show the `elevated` placeholder until the image is ready, then swap it in with no fade. Missing art: a type icon (album, radio, user) on `elevated`.
- **Now Playing background:** one flat `bg_color` on the screen, set when the track changes.
- **Pressed:** `LV_STATE_PRESSED` style with `bg_color` = mix(base, white, 12 %). For light controls (play/pause, primary) mix with black at 18 %.
- **Disabled / unavailable:** `LV_STATE_DISABLED` style: text in `text-3`, icons in `#4A5463`, `img_opa` 50 % on the art only. Never `opa` on the container.
- **Scrim:** a full-screen object with `bg_opa` 78 %, drawn once. Do not animate it.
- **Toast:** an `lv_obj` on the top layer with a one-shot `lv_anim` on `y` (150 ms) and an `lv_timer` for dismissal.
- **Keyboard:** one shared `lv_keyboard` on the top layer, re-parented as needed, with custom maps. Typing never triggers a full-screen redraw; results update in a bounded area beneath the field.
- **Slider and volume:** the UI updates instantly (optimistic). Commands are throttled to ≤ 5/s, with a final value on release. The value is reconciled from `player_updated`.
- **Ambient:** a separate screen with only the clock label, date label and a 88 px thumbnail. Backlight via PWM (verify the BSP brightness API). The night schedule only changes brightness and suppresses the clock's brighter colours.
- **FPS overlay:** a 14 px label on the top layer fed by `esp_lv_adapter_get_fps()`, toggled in About & diagnostics.

## 6. Mobbin references used (patterns only)

- Toast confirmation, a light pill above the controls: [Suno – added to liked songs](https://mobbin.com/screens/633b88ca-a55d-4374-81bf-f7bc0eefc8b4)
- Guarded destructive dialog with the safe choice prominent: [Headway – delete highlight](https://mobbin.com/screens/db8f2297-be48-4458-9c26-6cdbe74de2f7), [Reddit – delete feed](https://mobbin.com/screens/395a5b99-f969-4f07-ad43-d6c99a5c6c64)
- Long-press preview, with the item lifted above an action list: [Reddit – comment actions](https://mobbin.com/screens/557308cb-60b7-4849-b312-20bc13984dca), [Suno – song actions](https://mobbin.com/screens/dc7e2cec-5911-480e-ac99-a3a86e1396e1)
- Volume slider with mute and max icons at either end: [Discord – output volume](https://mobbin.com/screens/36de31de-20b3-4f44-bba5-fc28136239b4)
- Display settings with grouped brightness and wake options: [Apple Watch – display & brightness](https://mobbin.com/screens/31e665e5-d6ee-4e92-a9a6-e0a538499231)
- Schedule with from/until times: [Garmin Connect – sleep schedule](https://mobbin.com/screens/3d3465f7-e425-4a3d-94fc-ca9464232381)
- Inline field error under the input: [Aave – incorrect password](https://mobbin.com/screens/cc8bc5cf-9c7d-495a-97a0-10ed23503870), [X – invalid input](https://mobbin.com/screens/6aef4b06-248e-4937-8eb5-f61732a09f8a)
- Already in use (unchanged): Spotify home, Apple Music search and player, TIDAL queue; YouTube Music source tabs as the anti-pattern.

## 7. Decisions and open questions

**Design approved 2026-11-22**, with playlist screens added the same day (add to playlist, edit, Home shelf).

Decided (answers 2026-11-22):
1. **Clear queue keeps the current track.** "Clear up next" removes only the upcoming items and the current track keeps playing. *Verify* at the MA integration phase that `player_queues/clear` behaves this way. If it stops playback instead, the firmware deletes the upcoming items itself.
4. **Group volume shows both.** A master slider moves the whole group, and each member has its own row below it. A single player shows only the master slider (see the VolumeOverlay board).
9. **Languages: English and Dutch.** There is a picker under Settings › Language and changes apply without a restart. Adding a language means adding one string table plus its glyph range (see docs/architecture.md, i18n). Copy must fit the longer Dutch wording, so chips may not use fixed widths.

Defaults I'm assuming until told otherwise:
2. **Playback error source.** Unknown. Analysis phase: read the server's `/api-docs` and watch real events. Until then the design shows the error only when MA reports one; it never guesses.
3. **Fonts.** Unknown. The default is Latin plus Latin Extended-A, which covers English, Dutch and accented artist names. Other scripts show a fallback box. The font module accepts more ranges later.
5. **Preferred-source order** stays in Settings › Music Assistant.
6. **Night mode** only dims the screen.
7. **Guarded reset** stays in About.
8. **Backlight API.** *Verify* against the Waveshare BSP in the analysis phase.
