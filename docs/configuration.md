# Configuration reference

## Simulator: `music_panel_sim`

```
music_panel_sim [--live <env-file>] [--setup] [--shots <dir>]
```

| Option | Effect |
|---|---|
| *(none)* | Opens the panel in an X11 window with mock data. |
| `--live <env-file>` | Connects to the Music Assistant server named in `<env-file>`. All commands are real. |
| `--setup` | Starts at first-time setup instead of Home. The fake Wi-Fi password is `music`. |
| `--shots <dir>` | Runs headless: renders every screen to `<dir>/<name>.ppm`, prints the automatic checks and exits. Combine with `--setup` to capture the setup steps. |

## Command-line client: `panel_cli`

```
panel_cli [<env-file> | --mock]
```

Without arguments it reads `server.env` from the current directory. `MA_URL` and `MA_TOKEN` can also be set as environment variables, which take precedence over the file. See [Testing](testing.md#command-line-client) for its commands.

## Server file: `server.env`

`--live` and `panel_cli` read this file. Keep it at `test/host/server.env`, which Git ignores.

| Key | Value |
|---|---|
| `MA_URL` | The WebSocket address, `ws://<host>:8095/ws` |
| `MA_TOKEN` | A long-lived token from **Music Assistant › Settings › Profile** |

The parser skips lines that start with `#`.

## Firmware: `idf.py menuconfig`

Under **Music panel**:

| Option | Values | Default |
|---|---|---|
| Backend | Music Assistant · Mock (no server needed) | Music Assistant |
| Screen orientation | Landscape, rotated 90° · rotated 270° | 90° |
| Time zone | A POSIX TZ string, for the clock and the night schedule | `CET-1CEST,M3.5.0,M10.5.0/3` (Amsterdam) |
| Music Assistant WebSocket URL (development only) | `ws://…/ws`, or empty | *(empty)* |
| Music Assistant token | A long-lived token, or empty | *(empty)* |

Leave the URL empty on a real device: the panel then starts with first-time setup, which stores the server and token. A URL here skips setup, which is handy during development.

### Board settings

`sdkconfig.defaults` selects the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1: the 10.1" JD9365 panel in RGB565, PSRAM, 16 MB flash and Wi-Fi through the ESP32-C6 over SDIO. `partitions.csv` splits the flash into two 6 MB app slots for over-the-air updates, NVS for settings and 1 MB of storage.

## Stored settings

The panel keeps its settings in NVS on the device, and in memory in the simulator. You change them through the Settings screens; this table is for debugging.

| Key | Holds |
|---|---|
| `setup_done` | `1` once first-time setup has finished |
| `server_url`, `token` | The Music Assistant connection |
| `server_id` | The server's identity; the token is never sent to a different server at the same address |
| `player` | The selected speaker |
| `lang` | `en` or `nl` |
| `theme`, `accent`, `round`, `reduce_motion` | Appearance |
| `source_order` | The preferred order of music sources |
| `brightness`, `dim_after`, `clock_dimmed` | Daytime screen settings |
| `night`, `night_from`, `night_to`, `night_brightness` | Night schedule |
| `diag_fps`, `diag_touches` | Diagnostics overlays |
