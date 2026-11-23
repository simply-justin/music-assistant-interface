# Testing

Every part of the panel except the board drivers can be tested on a PC. The simulator runs the same core and the same screens as the firmware; only the display driver, Wi-Fi chip and backlight are replaced by stand-ins.

If you have not built anything yet, start with [Getting started](getting-started.md).

## Ways to test

| What it proves | Command |
|---|---|
| Logic: dedupe, search, protocol, features, setup, settings | `test/host/build/host_tests` |
| The panel with mock data; safe to tap anything | `test/sim/build/music_panel_sim` |
| The panel against your server; taps are real | `test/sim/build/music_panel_sim --live test/host/server.env` |
| First-time setup from the start | `test/sim/build/music_panel_sim --setup [--live test/host/server.env]` |
| Every screen rendered to images, without a window | `test/sim/build/music_panel_sim --shots <dir>` |
| The core without a UI, driven by typed commands | `test/host/build/panel_cli test/host/server.env` (or `--mock`) |
| The firmware compiles for the ESP32-P4 | `idf.py build` |

All options are described in [Configuration](configuration.md).

### Host tests

`host_tests` builds the core, backend, app and i18n components for the PC with AddressSanitizer and UndefinedBehaviorSanitizer. It prints `all checks passed` and exits with 0, or prints each failed check with its file and line.

### Screenshots

`--shots <dir>` walks through every screen and overlay and writes one `.ppm` image per screen. It also runs two automatic checks and prints their results:

- **Windowed list:** opens a 400-song playlist, scrolls to the end, and reports how many row objects exist (about 15).
- **Speaker row tap test:** checks that the name, the status line and empty space in a speaker row all hit the same row. It prints `PASS` or `FAIL`.

With `--setup`, it captures each setup step instead, including the error states.

### Command-line client

`panel_cli` runs the same core as the panel without any screen, which makes it the quickest way to try one command against a real server. Type `help` for the list. The main commands are:

| Command | Does |
|---|---|
| `s <text>` | Search everywhere; results are numbered |
| `play <n>`, `add <n>`, `next <n>` | Play result *n*, add it to the end of the queue, or play it next |
| `pp`, `n`, `p` | Play/pause, next, previous |
| `speakers`, `use <n>` | List speakers, select speaker *n* |
| `vol <0-100>`, `gvol <0-100>` | Speaker volume, group volume |
| `q`, `rm <n>`, `clear` | Show the queue, remove item *n*, clear upcoming items |
| `pl`, `plsongs <p>` | List playlists, list a playlist's songs |
| `lang en\|nl` | Switch language |

## Manual test checklist

Use the simulator with mock data unless a step says otherwise.

### Home

- [ ] Tapping a favourite tile plays it.
- [ ] Holding a favourite tile opens a preview with Play, Play next, Add to queue and Remove.
- [ ] **See all** opens the Library.
- [ ] The speaker icon in the player bar opens the speaker list. Choosing a speaker makes it the target for everything.
- [ ] **Play on more speakers**, below the list, groups speakers. It only offers speakers that Music Assistant can group.

### Search

- [ ] While typing, library results appear first with "From your library, still searching everywhere…", then the full results.
- [ ] A recording available from several sources shows once, with a "+2 sources" tag. The grey letter shows where it will play from.
- [ ] Chips filter by media type, never by service.
- [ ] **123** and **ABC** switch the keyboard layout.
- [ ] The **…** menu on a song offers Play now, Play next, Add to queue, Add to playlist, Favourite and Play from.

### Now Playing

- [ ] The background colour follows the artwork.
- [ ] Seek, shuffle, repeat, volume and favourite all work.
- [ ] For radio, seek, skip and shuffle are hidden; a LIVE tag and a stop button appear instead.

### Queue

- [ ] Tapping a song plays it; **×** removes it; holding offers Play next, Move up and Move down.
- [ ] Adding a song that is already queued shows "is already in the queue" with **Add again**.
- [ ] After adding a song, the toast offers **Undo**.
- [ ] **Clear** asks for confirmation, then removes everything after the current song. The music keeps playing.

### Library

- [ ] The tabs are Favourites, Playlists, Albums, Artists, Tracks and Radio.
- [ ] A playlist offers Play, Shuffle and Favourite.
- [ ] **Edit** appears only on playlists you can change. In edit mode, **×** removes a song and **Delete playlist** asks for confirmation.

### Settings

- [ ] **Appearance:** theme, accent, corner style and Reduce motion apply immediately.
- [ ] **Screen & night mode:** brightness, dim after, night schedule and **Preview dimmed screen** work. Leaving the window alone for the dim-after time shows the clock; a click wakes it.
- [ ] **Music Assistant:** shows the server, its version, your services and their status, and the preferred-source order.
- [ ] **Language:** English and Nederlands switch instantly.
- [ ] **About & diagnostics:** the frame-rate counter and touch dots can be switched on.

### Setup (`--setup`)

- [ ] A wrong Wi-Fi password shows an error; `music` connects.
- [ ] The server is found automatically. A wrong typed address shows "Nothing answered".
- [ ] Sign-in shows a QR code; the simulated phone sends the token after about three seconds.
- [ ] Choosing a speaker and finishing leads to Home.

### Connection loss (`--live`)

- [ ] Stop Music Assistant, or disconnect the network, for a moment. Home shows "Can't reach Music Assistant" with **Retry**.
- [ ] The panel reconnects by itself, waiting 1, 2, 4… up to 30 seconds between attempts, and reloads its data.

## Reporting a problem

A useful report includes:

1. The screen and what you tapped.
2. What you expected and what happened instead.
3. A screenshot of the simulator window.
4. For stutter or timing problems, the frame rate: switch on the counter in **Settings › About & diagnostics** first.

## What a PC cannot test

Touch feel, scrolling smoothness, frame rate and memory use on the ESP32-P4, the Wi-Fi chip, the backlight and the hardware JPEG decoder. These are measured on the device once board support is in place.
