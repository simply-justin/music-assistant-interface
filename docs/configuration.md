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
| Backend | Mock (no server needed) · Music Assistant | Mock |
| Music Assistant WebSocket URL | `ws://…/ws` | `ws://homeassistant.local:8095/ws` |
| Music Assistant token | A long-lived token, or empty | *(empty)* |

The URL and token options are for development. On a finished device, first-time setup stores them instead.

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
