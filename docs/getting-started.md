# Getting started

This tutorial takes you from a fresh clone to the panel running on your PC, first with mock data and then connected to your own Music Assistant server. You do not need the hardware.

**Time:** about 15 minutes, most of it spent on the first build.

## Before you begin

You need:

- Linux. On Windows, use WSL2: its built-in graphics support (WSLg) shows the simulator window.
- CMake 3.16 or later, a C++17 compiler (GCC or Clang), Python 3 and Git.
- The X11 development headers, for the simulator window: `sudo apt install libx11-dev` on Debian or Ubuntu.
- [ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32p4/get-started/). The PC builds use the cJSON library that ships with it.

## 1. Build the simulator

From the repository root:

```sh
export IDF_PATH=~/esp/v5.5.1/esp-idf
cmake -S test/sim -B test/sim/build
cmake --build test/sim/build -j
```

The first configure downloads LVGL 9.3, which takes a minute. After that, `cmake --build test/sim/build -j` is enough, and `IDF_PATH` is remembered.

## 2. Run it with mock data

```sh
test/sim/build/music_panel_sim
```

A 1280 × 800 window opens with the Home screen. The data is fake, so nothing you do reaches a real server and no speaker plays.

The mouse acts as your finger:

| Mouse | Touch |
|---|---|
| Click | Tap |
| Hold for about half a second | Long-press |
| Drag | Scroll |

Try this:

1. Tap a favourite tile. The player bar at the bottom shows the track.
2. Tap the player bar to open Now Playing.
3. Open **Search** in the sidebar and type "viva". Library results appear first, then the rest.
4. Tap the speaker icon in the player bar and choose **Kitchen**.

Close the window to quit.

## 3. Connect to your Music Assistant server

1. In Music Assistant, open **Settings › Profile** and create a long-lived access token.
2. Create `test/host/server.env` with your server address and the token:

   ```ini
   MA_URL=ws://192.168.1.20:8095/ws
   MA_TOKEN=<your long-lived token>
   ```

   This file is listed in `.gitignore`, so the token is never committed.

3. Start the simulator against it:

   ```sh
   test/sim/build/music_panel_sim --live test/host/server.env
   ```

> [!WARNING]
> In live mode every tap is real. Play, queue and volume commands play on your speakers, at the volume you set. Choose the speaker and turn the volume down before you press Play.

## 4. Walk through first-time setup

```sh
test/sim/build/music_panel_sim --setup
```

This starts the panel as it is out of the box. The Wi-Fi networks are fake, and the password for each is `music`. When the panel asks you to sign in, the simulated phone sends the token after about three seconds. Add `--live test/host/server.env` to finish setup against your real server.

## 5. Run the tests

```sh
cmake -S test/host -B test/host/build
cmake --build test/host/build -j
ctest --test-dir test/host/build --output-on-failure   # unit and integration tests
ctest --test-dir test/sim/build --output-on-failure    # end-to-end tests
```

All tests should pass. The unit and integration tests are built with AddressSanitizer and UndefinedBehaviorSanitizer, so memory errors fail the run too. [Testing](testing.md#automated-tests) explains what each level covers.

## Next steps

- [Testing](testing.md) lists every way to run the panel and a checklist of what to try on each screen.
- [Architecture](architecture.md) explains how the code is organised.
- [Extending the panel](extending.md) shows how to add your own feature.
