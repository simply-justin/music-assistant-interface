# How to test (without the panel)

Everything below runs on your PC under WSL. The simulator is the real panel software (same core,
same screens) in a 1280×800 window; only the board-specific parts (display driver, Wi-Fi chip,
backlight) are replaced by stand-ins.

## 1. Build once

```sh
cd ~/projects/open-collective/music-assistant-interface
export IDF_PATH=~/esp/v5.5.1/esp-idf                      # only needed for the first configure
cmake -S test/sim  -B test/sim/build  && cmake --build test/sim/build  -j
cmake -S test/host -B test/host/build && cmake --build test/host/build -j
```

After pulling new code, only the `cmake --build …` lines are needed.

Your server and token live in `test/host/server.env` (never committed):

```
MA_URL=ws://192.168.2.80:8095/ws
MA_TOKEN=<long-lived token>
```

## 2. Pick a way to test

| Command | What you get |
|---|---|
| `test/sim/build/music_panel_sim` | The panel with **fake data**. Nothing reaches your server; safe to tap anything. |
| `test/sim/build/music_panel_sim --live test/host/server.env` | The panel on **your server**. Play/queue/volume/playlist taps are real — they play on your speakers. |
| `test/sim/build/music_panel_sim --setup` | First-time **setup** from the start (fake Wi-Fi password: `music`). Add `--live test/host/server.env` to finish it against your server. |
| `test/host/build/panel_cli test/host/server.env` | Typed commands instead of screens (type `help`). Handy to try one thing quickly. |
| `test/host/build/host_tests` | All automatic checks; prints `all checks passed`. |
| `music_panel_sim --shots <folder>` | Renders every screen to image files (`.ppm`) without a window. |

The mouse is your finger: click = tap, hold ~0.5 s = long-press, drag = scroll.
Close the window to quit.

**Before tapping Play in `--live` mode:** pick the speaker (top right chip) and turn the volume
down (speaker icon in the bottom bar) — the panel plays exactly where and as loud as you tell it.

## 3. What to try (and what should happen)

**Home**
- Favourites grid (6 tiles): tap → plays. Hold → preview with Play / Play next / Add to queue / Remove.
- "Recently played / Your playlists" switch below. "See all" → Library.
- Speaker chip → speaker list; pick one → it becomes the speaker for everything.

**Search** (rail → Search)
- Type with the on-screen keyboard: your library answers first ("From your library, still
  searching everywhere…"), then everything. One row per song; "+2 sources" when Spotify/YouTube/
  library have the same recording. The grey letter is where it will play from.
- Chips filter by type (never by service). "123" / "ABC" switch the keyboard.
- "…" on a song → Play now / Play next / Add to queue / Add to playlist / favourite / Play from.

**Now Playing** (tap the bottom bar or the Home card)
- Big artwork, background colour follows the artwork. Seek, shuffle, repeat, volume, heart.
- Radio: no seek / skip / shuffle (a LIVE tag and a stop button instead).

**Queue**
- Tap a song → plays it. × removes. Hold → Play next / Move up / Move down.
- Add the same song twice → "is already in the queue" with **Add again**.
- After adding a song the toast offers **Undo**.
- "Clear" → asks first → removes everything *after* the current song; the music keeps playing.

**Library** — tabs Favourites / Playlists / Albums / Artists / Tracks / Radio.
Open a playlist → Play / Shuffle / heart; **Edit** (only for playlists you can change) → × removes
a song, "Delete playlist" asks first.

**Settings**
- Screen & night mode: brightness, dim after, night schedule, "Preview dimmed screen".
- Leave the window alone for the "dim after" time → clock screen; click → wakes.
- Music Assistant: server, version, your services and whether they work, preferred-service order.
- Language: English ↔ Nederlands, instantly.
- About & diagnostics: frame-rate counter and touch dots (try them).

**Setup** (`--setup`) — Wi-Fi list → wrong password shows the error → `music` works → server
found (or type an address; a wrong one says "Nothing answered") → sign in (QR; the fake "phone"
sends the token after 3 s) → choose speaker → done → Home.

**Offline** — in `--live`, switch off Music Assistant (or Wi-Fi) for a moment: Home shows "Can't
reach Music Assistant" with Retry; it reconnects by itself (1 s, 2 s, 4 s … up to 30 s apart) and
reloads.

## 4. Telling me about problems

Most useful: the screen, what you tapped, what you expected, what happened. A screenshot of the
window is perfect. For timing/stutter issues, switch on the frame-rate counter first.

## 5. What the PC cannot tell us

Real touch feel, scrolling smoothness, frame rate and memory on the ESP32-P4, the Wi-Fi chip,
backlight, and how the hardware JPEG decoder performs. Those are measured when the panel arrives
(Phase 2b and Phase 5).

## 6. When the panel arrives

```sh
. ~/esp/v5.5.1/esp-idf/export.sh
idf.py menuconfig          # Music panel → Backend: Music Assistant, URL + token (until setup runs on the board)
idf.py build flash monitor
```

The board package (display, touch, Wi-Fi via the C6, backlight, artwork decoder) is Phase 2b; until
then the firmware runs the core and logs what it would show.
