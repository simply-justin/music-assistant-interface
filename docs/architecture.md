# Architecture

This document explains how the firmware is organised and why it is organised that way. For step-by-step instructions on adding code, see [Extending the panel](extending.md).

## Design goals

- **A new feature costs almost nothing.** Every feature is a module that plugs into fixed seams: commands come in, server events and connection changes arrive, and views go out. Adding one touches its own file and one line in a registry.
- **Everything except the drivers runs on a PC.** The same code runs in the firmware, the simulator and the host tests.
- **The UI never waits.** Network work happens on another task; the UI only reads finished snapshots.

The code is C++17 without exceptions or RTTI. Screens are written by hand from a shared component kit; there is no UI designer or generated layout.

## Layers

The project is a set of ESP-IDF components:

```
main/                 The firmware: display and touch start, the core task, the two queues, NVS settings
  board.cpp           Platform on the board: Wi-Fi (ESP32-C6), mDNS discovery, pairing server, backlight, clock
  board_art.cpp       Artwork: esp_http_client + the hardware JPEG decoder
components/
  core/               Pure domain logic: model, dedupe, title parsing, search session, repeat guard
  backend/            The Backend interface and its implementations
    ma_session          Music Assistant WebSocket protocol, without the socket
    ma_commands         Every Music Assistant command, argument and field name
    ma_client           esp_websocket_client glue (device only)
    mock_backend        A simulated server: mixed providers, duplicates, latency
  app/                The core application: App, the feature registry and the Platform seam
    include/app/        commands.hpp (UI → core), updates.hpp (core → UI), feature.hpp, platform.hpp
    features/           One file per feature: browse, favourites, panel, playback, players,
                        playlists, preferences, queue, search, setup
  i18n/               strings.csv → generated string IDs and tables; i18n::tr()
  img/                Artwork: proxy URLs, scaling with pre-rounded corners, LRU cache with pins
  pairing/            The phone pairing page: HTML and token parsing (portable, unit-tested)
  ui/                 LVGL 9 UI: theme (tokens), kit (components), shell (sidebar, player bar,
                      navigation, overlays), screens/ (one file per screen), fonts/ (generated)
test/
  host/               PC build of core, backend, app and i18n: unit and integration tests, panel_cli
  sim/                The simulator (an X11 window, or headless) and the end-to-end tests
```

Dependencies only point down:

```
main → ui → app → backend → core
              └→ i18n
```

- `core` has no ESP-IDF or LVGL includes, so the host tests compile it unchanged.
- `ma_commands` is the only file that knows Music Assistant's schema. When the server API changes, only this file changes.
- The UI depends on `app` (commands and updates) only, never on `backend`.

## Threads and data flow

```
UI task (LVGL) ──Command──► jobs queue ──► core task: App.handle → features → Backend.request
     ▲                                          │
     └── lv_timer drains ◄── updates queue ◄────┘ publish(Update: immutable snapshot)

WebSocket task ──text frame──► jobs queue ──► ma::Session on the core task
```

- **One task owns all state.** Features run only on the core task, so they need no locks.
- **Views are immutable snapshots.** Updates are `shared_ptr<const …>`, so the UI reads them without locking and without copying.
- **The UI never blocks on the network.** A tap becomes a command on a queue and returns immediately. The result arrives later as an update.

In the simulator and the host tests, everything runs on a single thread; the queues are plain vectors.

### Rendering updates

`ui::apply()` receives every update. To keep the panel smooth, it skips work the user would not see:

- It drops an update whose visible content equals the previous one. This matters because Music Assistant sends `player_updated` and `queue_time_updated` events several times a second.
- When only the playback position changed, it redraws just the progress bar and the elapsed time.
- When it rebuilds a screen, the screen keeps its scroll position.
- Artwork fades in only when it arrives from the network, not when it comes from the cache.

## The feature seam

A feature implements `app::Feature`:

```cpp
class Feature {
public:
    virtual void on_connected() {}                      // after (re)connect: full re-sync
    virtual void on_connection(backend::Connection) {}  // every connection state change
    virtual void on_command(const Command&) {}          // from the UI or another feature
    virtual void on_event(const backend::Event&) {}     // from the server
    virtual void tick() {}                              // every core loop
};
```

Through its `Context`, a feature can send requests to the server, publish updates to the UI, dispatch commands to other features and read settings. `components/app/features.cpp` creates the features in order; the order matters where one feature depends on another (for example, `players` selects the speaker the others use).

## The platform seam

`app::Platform` (`components/app/include/app/platform.hpp`) holds everything that differs between the board and a PC: Wi-Fi scanning and joining, server discovery, the pairing page that receives a token from a phone, the backlight, system information, local time and restart.

- The simulator and tests use `test/host/fake_platform.hpp`.
- The firmware uses `board::BoardPlatform` in `main/board.cpp`:
  - **Wi-Fi** through the ESP32-C6 co-processor (`esp_wifi_remote` over SDIO), with the network saved for the next boot and reconnecting by itself.
  - **Discovery** browses mDNS for `_mass._tcp`, the service Music Assistant announces on its web port.
  - **Pairing** serves the page from `components/pairing` on port 80, at a random six-character path that the QR code carries, so other devices on the network cannot post a token to a guessable address.
  - **Backlight** through the Waveshare BSP; **local time** through SNTP and the time zone from menuconfig.

First-time setup (`features/setup.cpp`) and panel settings (`features/panel.cpp`) only talk to this interface, so they are fully tested on the PC.

## The UI

- A screen is a class in `components/ui/screens/` that implements `Screen`. The shell builds it once and refreshes it by topic when data changes.
- Screens stay alive. Navigation shows and hides them instantly, without transitions.
- Screens use only `kit::` components and `theme::` tokens, and every string comes from `strings.csv`. A change of theme or language rebuilds the screens.
- Long lists (songs, queue, library tracks and radio) are windowed: only visible rows exist, about 15 objects for a 400-song playlist. Card grids are capped at 300 items.
- `components/ui/tools/gen_assets.py` generates the fonts (Inter 4.1) and icons (Phosphor 2.1.1).

See [Design](design.md) for the visual system.

## The Music Assistant protocol

Verified against Music Assistant 2.10.13 (API schema 65):

- **Transport.** `ws://<server>:8095/ws`. The server sends `server_info` first; the client must then send `auth` with its token (required since schema 28).
- **Commands.** Every command and argument name in `ma_commands.cpp` matches the server's `/api-docs/commands.json`.
- **Library-first search** uses `providers: ["library"]`, the documented replacement for `library_only`.
- **Players** use `name`, `playback_state` and `group_members`. A player's queue comes from `player_queues/get_active_queue`; it is the player's own ID unless the player is grouped.
- **Group volume** is `players/cmd/group_volume`.
- **Clearing the queue.** `player_queues/clear` empties the whole queue and stops playback, even with `skip_stop: true`. To clear only the upcoming items, the panel deletes them one by one.
- **Playlist positions** for `remove_playlist_tracks` count from 1.
- **External IDs** are a list of `[type, value]` pairs. Images carry a `type` (the panel uses `thumb`) and a `proxy_id`.

### Security

- The panel stores the server's `server_id` and never sends its token to a different server at the same address.
- During development the token lives in `test/host/server.env`, which Git ignores.

## Connection handling

When the connection drops, the panel shows "Can't reach Music Assistant" with a Retry button and reconnects on its own with exponential backoff: 1, 2, 4… up to 30 seconds between attempts. After reconnecting, every feature re-syncs in `on_connected()`.

## Known limitations

- **Board support is not verified on hardware.** The display, touch, Wi-Fi, discovery, pairing page, backlight, clock and hardware JPEG decoding are written against ESP-IDF 5.5 and the Waveshare BSP, and the firmware builds in CI, but none of it has run on the panel yet. The screen rotation and touch mapping in particular need checking on the device.
- **Playback errors** appear only when Music Assistant marks the current queue item as unavailable. A richer error signal is an open question.
- Wi-Fi via the ESP32-C6, PSRAM, flash size, the partition layout and the chip revision will follow Waveshare's reference examples.
