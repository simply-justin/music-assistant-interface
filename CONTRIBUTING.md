# Contributing

Thank you for your interest in improving the panel. This guide explains how to set up, what a good change looks like and how to submit it.

## Setting up

Follow [Getting started](docs/getting-started.md) to build the simulator and the host tests. You do not need the hardware.

## Before you start

- **Bugs:** open an issue with the screen, what you did, what you expected and a screenshot. See [Reporting a problem](docs/testing.md#reporting-a-problem).
- **Features and visual changes:** open an issue first to discuss the idea. Visual changes are designed and agreed on before they are implemented.

## Making a change

[Extending the panel](docs/extending.md) lists exactly which files each kind of change touches. In short:

- **Keep layers separate.** `core` has no ESP-IDF or LVGL includes. The UI talks to the app only through commands and updates, never to the backend.
- **Keep Music Assistant knowledge in one place.** Command names, arguments and fields belong in `components/backend/ma_commands.cpp`.
- **Use the design system.** Screens use `kit::` components and `theme::` tokens, never raw colours, sizes or fonts. See [Design](docs/design.md).
- **No hard-coded text.** Every visible string is a row in `components/i18n/strings.csv`, with a translation for every language.
- **Keep the touch rules.** Every target is at least 56 px and text is at least 14 px. See [Accessibility](docs/accessibility.md).

## Code style

- C++17, without exceptions or RTTI.
- Four-space indentation and lines up to 120 characters; follow the formatting of the surrounding code.
- Comments explain *why*, not *what*.
- Prefer deleting code to adding it, and reuse a helper that already exists before writing a new one.

## Testing your change

Before you open a pull request:

1. `test/host/build/host_tests` prints `all checks passed`.
2. New logic has a test in `test/host/tests.cpp`.
3. The simulator builds and the change works there with mock data.
4. For a UI change, `music_panel_sim --shots <dir>` renders the affected screens correctly in the dark and light themes and in Dutch.
5. `idf.py build` succeeds for the ESP32-P4.

Commands that change playback play on real speakers. Test them in live mode only on your own setup.

## Pull requests

- One topic per pull request.
- Describe what changed and why, and how you tested it.
- Add before and after screenshots for visual changes.
- Update the documentation when behaviour, configuration or the design changes.
- Never commit tokens or server addresses. `test/host/server.env` is ignored by Git for that reason.
