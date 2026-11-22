# Firmware architecture

Goal: readable code where a new feature costs almost nothing. Rule: **every feature is a module that plugs into fixed seams.** These are commands in, server events, connection changes and views out. Adding one touches its own file plus one line in a registry.

Decisions (2026-11-22): C++17 without exceptions or RTTI. Screens are hand-written from a shared component kit. An OTA slot is reserved. English and Dutch, plus any language added as a CSV column.

## Layers (ESP-IDF components)

```
main/                 FreeRTOS wiring: core task, the two queues, NVS settings, backend choice (menuconfig)
components/
  app/                The core: App + feature registry
    include/app/      commands.hpp (UI → core), updates.hpp (core → UI), feature.hpp (the seam), settings.hpp
    features/         players, playback, queue, search, favourites, preferences  (one file each)
    features.cpp      the registry: one line per feature
  core/               Pure domain logic, host-tested: model, dedupe, title parsing, search session, repeat guard
  backend/            Backend interface + implementations
    ma_session        MA WebSocket protocol without the socket (host-tested)
    ma_commands       every MA command name / arg / field: the only file that knows MA's schema
    ma_client         esp_websocket_client glue (device only)
    mock_backend      simulated server: mixed providers, duplicates, latency
  i18n/               strings.csv → generated enums/tables; tr()
  ui/                 LVGL 9 UI: theme (tokens), kit (components), shell (rail, mini-player, routing,
                      overlays layer), screens/ (one file each), fonts/ (generated Inter + icons)
  img/, platform/     later: artwork cache (HW JPEG), BSP glue (display, touch, backlight, Wi-Fi)
test/host/            PC build of core + backend + app + i18n, with ASan/UBSan
```

Dependencies only point down: `main → app → backend → core`, and `app → i18n`. The UI will depend on `app` (commands + updates) only, never on `backend`. `core` has no ESP-IDF or LVGL includes.

## Threads and data flow

```
UI task (LVGL) ──Command──► g_jobs queue ──► core task: App.handle → features → Backend.request
     ▲                                          │
     └── lv_timer drains ◄── g_updates queue ◄──┘ publish(Update: immutable snapshot)
websocket task ──text frame──► g_jobs (posted as a job) ──► ma::Session on the core task
```

- One task owns all state, so there are no locks in features.
- Views go out as `shared_ptr<const …>` snapshots, so the UI reads them without locks.
- Nothing on the UI side ever waits on the network.

## How to add things

| You want | You touch |
|---|---|
| A new button/action | a struct in `commands.hpp`, plus one `if (auto* c = std::get_if<…>)` in the feature that owns it |
| A new feature | `features/<name>.cpp` (class + `make_<name>`), one line in `features.cpp`, one in `features.hpp` |
| A new MA command | one builder in `ma_commands.cpp` (+ a parser if it returns data) |
| A new language | one column in `components/i18n/strings.csv` |
| A new UI string | one row in `strings.csv`, then `i18n::tr(i18n::Str::<id>)` |

## Testing without the device

| Level | What it proves | Command |
|---|---|---|
| Host tests | logic: dedupe, search, protocol, features (with ASan/UBSan) | `test/host/build/host_tests` |
| PC test app, mock | the whole core without any server | `test/host/build/panel_cli --mock` |
| PC test app, live | the **same core code** against your real Music Assistant server | `test/host/build/panel_cli test/host/server.env` |
| Device build | it compiles for the ESP32-P4 | `idf.py build` |
| **Simulator** | the real screens in a 1280×800 window (X11/WSLg), mock or live | `test/sim/build/music_panel_sim [--live test/host/server.env]` |
| Setup flow | first-time setup from the start (fake Wi-Fi password: `music`) | `music_panel_sim --setup [--live test/host/server.env]` |
| Screenshots | every screen rendered headless to `.ppm` (review without a display) | `music_panel_sim --shots <dir>` |

`test/host/server.env` (git-ignored, yours only):

```
MA_URL=ws://192.168.2.80:8095/ws
MA_TOKEN=<long-lived token from Music Assistant › Settings › Profile>
```

Build everything on the PC:

```sh
export IDF_PATH=~/esp/v5.5.1/esp-idf          # only for cJSON (remembered after the first configure)
cmake -S test/host -B test/host/build && cmake --build test/host/build -j
cmake -S test/sim  -B test/sim/build  && cmake --build test/sim/build  -j   # fetches LVGL 9.3 once
```

In the simulator the mouse is your finger; everything you tap sends the same commands the panel
will. With `--live`, taps on Play/Add/Volume really play on your speakers.

## Platform seam

`app::Platform` (`components/app/include/app/platform.hpp`) is everything that differs between
the board and a PC: Wi-Fi scan/join, finding the server, the pairing page that receives the token
from a phone, backlight, system info, local time, restart. The simulator and tests use
`test/host/fake_platform.hpp`; `main/main.cpp` has `BoardPlatform`, which reports Wi-Fi/discovery/
pairing as "not available" until the board package lands (Phase 2b).

First-time setup (`features/setup.cpp`) and panel settings (`features/panel.cpp`) only talk to this
interface, so they are fully tested on the PC (`test_setup_flow`, `test_panel_brightness`).

## UI rules (how to add a screen)

- A screen is a class in `components/ui/screens/<name>.cpp` implementing `Screen` (build once,
  `refresh(Topic)` on data changes, `on_open()` to request data), plus one line in `ui::start()`.
- Screens use only `kit::` components and `theme::` tokens; every string comes from `strings.csv`.
- Screens stay alive; navigation shows/hides them instantly (spec: no animated transitions).
- A click that rebuilds its own container uses `kit::later()` (LVGL must not delete the object
  whose event is running).
- Fonts and icons are generated by `components/ui/tools/gen_assets.py` (Inter 4.1, Phosphor 2.1.1).
- Emoji in names are removed for display (`ui::clean`); decision 2026-11-22.

Device (when it arrives):

```sh
. $IDF_PATH/export.sh
idf.py set-target esp32p4      # once
idf.py menuconfig              # Music panel → Backend: Mock | Music Assistant (+ URL, token)
idf.py build flash monitor
```

## Verified against the real server (2.10.13, schema 65, 2026-11-22)

- Transport: `ws://<server>:8095/ws`, `server_info` first, then `auth {token}` is mandatory (schema ≥ 28)
- Every command and argument name used by `ma_commands.cpp`, checked against `/api-docs/commands.json`
- Library-first search uses `providers: ["library"]` (the documented replacement for `library_only`)
- Corrected from the brief: players use `name`, `playback_state` and `group_members`; a player's queue comes from `player_queues/get_active_queue` (it is the player id unless grouped); `clear` gets `skip_stop: true`; group master volume is `players/cmd/group_volume`
- `external_ids` is a list of `[type, value]` pairs, and images carry `type` (we use `thumb`) plus `proxy_id`
- Security: the panel remembers the `server_id` and never sends its token to a different server at the same address (advice from `/api-docs`)

## Known limits

- Not checked live yet: commands that change playback (play, add, clear, volume). That needs your OK, because they play on your speakers.
- On the board, Wi-Fi join, server discovery (mDNS), the token pairing page and backlight PWM are
  stubs until Phase 2b; set the server/token in menuconfig meanwhile.
- The playback-error banner shows when Music Assistant marks the current queue item unavailable;
  a richer error signal is open question 2.
- Reconnect uses a fixed 3 s retry, not exponential backoff (marked shortcut).
- Wi-Fi (via the C6), PSRAM, flash size, partitions/OTA and chip revision come from Waveshare's examples once the board is here.
- UI lists are capped plain lists (200–500 rows), not yet recycled/virtualized; artwork is a flat
  colour placeholder until the artwork pipeline (performance phase / Phase 4).
- No accent folding in dedupe ("Beyoncé" ≠ "Beyonce"), marked shortcut. Undo on the "added" toast is not implemented yet.
