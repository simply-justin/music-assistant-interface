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
  ui/, img/, platform/  later: the component kit + screens (after design approval), artwork cache, BSP glue
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
| Screens on PC (Phase 3) | LVGL screens in a desktop window (SDL), with mock or live data | after design approval |

`test/host/server.env` (git-ignored, yours only):

```
MA_URL=ws://192.168.2.80:8095/ws
MA_TOKEN=<long-lived token from Music Assistant › Settings › Profile>
```

Build everything on the PC:

```sh
export IDF_PATH=~/esp/v5.5.1/esp-idf          # only for cJSON
cmake -S test/host -B test/host/build && cmake --build test/host/build -j
```

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
- Reconnect uses a fixed 3 s retry, not exponential backoff (marked shortcut).
- Wi-Fi (via the C6), PSRAM, flash size, partitions/OTA and chip revision come from Waveshare's examples once the board is here.
- No accent folding in dedupe ("Beyoncé" ≠ "Beyonce"), marked shortcut. Undo on the "added" toast is not implemented yet.
