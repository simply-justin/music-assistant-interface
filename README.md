# Music Assistant Interface

Wall panel for Music Assistant on the Waveshare ESP32-P4 10.1" touch display (ESP-IDF 5.5, LVGL 9, C++17).

- Design: [docs/design-spec.md](docs/design-spec.md) (canvas linked inside)
- **How to test (no panel needed): [docs/testing.md](docs/testing.md)**
- Architecture, build and test: [docs/architecture.md](docs/architecture.md)

Status: core app + first UI slice (Home, Search, Now Playing, Queue, Library, album/artist/playlist detail, Settings, speaker/volume/track/playlist sheets) run in the PC simulator against the mock or a real Music Assistant server, and build for the ESP32-P4. Board bring-up (display, touch, Wi-Fi) waits for the hardware.
