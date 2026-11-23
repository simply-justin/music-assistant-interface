# Extending the panel

Step-by-step guides for the most common changes. Each one lists exactly which files you touch. Read [Architecture](architecture.md) first if you have not.

| You want to add | You touch |
|---|---|
| [A UI string](#add-a-ui-string) | One row in `strings.csv` |
| [A language](#add-a-language) | One column in `strings.csv` |
| [A Music Assistant command](#add-a-music-assistant-command) | `ma_commands.cpp` (and `.hpp`) |
| [An action from the UI](#add-an-action-from-the-ui) | `commands.hpp`, plus the feature that handles it |
| [A feature](#add-a-feature) | A new file in `features/`, plus one line in two registries |
| [A screen](#add-a-screen) | A new file in `ui/screens/`, plus one line in `ui::start()` |

After any change, run the host tests and look at the result in the simulator; see [Testing](testing.md).

## Add a UI string

1. Add a row to `components/i18n/strings.csv`. The first column is the ID, followed by one column per language:

   ```csv
   queue_empty,Nothing in the queue yet,Nog niets in de wachtrij
   ```

2. Use it in code:

   ```cpp
   kit::text(parent, tr(i18n::Str::queue_empty), theme::lead(), theme::text2());
   ```

The header and tables are generated from the CSV when CMake configures, and CMake reconfigures automatically when the file changes.

Write the text so that the longest translation still fits: Dutch is often a third longer than English. Labels end with "…" rather than wrapping.

## Add a language

1. Add a column to `components/i18n/strings.csv` with the language code as its header, and translate every row. The `language_name_self` row holds the language's own name, such as "Deutsch".
2. Rebuild. The language appears under **Settings › Language** automatically.
3. If the language needs characters outside Latin, Latin-1 and Latin Extended-A, add their range to the fonts in `components/ui/tools/gen_assets.py` and regenerate them. Missing characters render as a box.

## Add a Music Assistant command

All knowledge of the server's schema lives in `components/backend/ma_commands.cpp`.

1. Check the command name and its arguments in your server's `/api-docs`.
2. Add a builder that returns a `backend::Request`. Follow the existing builders in `ma::cmd`.
3. If the command returns data, add a parser in `ma::parse` that converts the JSON into a `core::` model type.
4. Add a unit test for the parser in `test/host/unit_tests.cpp`.

## Add an action from the UI

1. Add a struct to `components/app/include/app/commands.hpp` and add it to the `Command` variant:

   ```cpp
   struct SetSleepTimer { int minutes; };
   ```

2. Handle it in the feature that owns that state:

   ```cpp
   void on_command(const Command& c) override {
       if (auto* t = std::get_if<cmd::SetSleepTimer>(&c)) {
           ctx_.send(ma::cmd::sleep_timer(ctx_.player_id, t->minutes));
       }
   }
   ```

3. Send it from the UI, for example from a button:

   ```cpp
   kit::button(parent, kit::Button::Secondary, icon::moon, tr(i18n::Str::sleep_30),
               [] { send(app::cmd::SetSleepTimer{30}); });
   ```

## Add a feature

A feature owns one area of state, such as the queue or the speakers.

1. Create `components/app/features/<name>.cpp`:

   ```cpp
   #include "features/features.hpp"

   namespace app {
   namespace {

   class SleepTimer : public Feature {
   public:
       explicit SleepTimer(Context& ctx) : ctx_(ctx) {}
       void on_connected() override { /* fetch initial state */ }
       void on_event(const backend::Event& ev) override { /* react to the server */ }
       void on_command(const Command& c) override { /* react to the UI */ }

   private:
       Context& ctx_;
   };

   }  // namespace

   std::unique_ptr<Feature> make_sleep_timer(Context& ctx) { return std::make_unique<SleepTimer>(ctx); }

   }  // namespace app
   ```

2. Declare `make_sleep_timer` in `components/app/features/features.hpp`.
3. Register it in `components/app/features.cpp`. Order matters only if the feature depends on another one.
4. Add the file to `components/app/CMakeLists.txt` and to `test/core.cmake`.
5. If the UI needs the state, add a view to `updates.hpp` and publish it with `ctx_.publish(...)`.
6. Add an integration test in `test/host/integration_tests.cpp` that drives the feature with the mock server.

Rules for features:

- Do all work on the core task; never block.
- Publish immutable snapshots, never pointers to internal state.
- Talk to other features through commands (`ctx_.dispatch`), not by calling them directly.
- Re-sync everything in `on_connected()`; the connection can drop at any time.

## Add a screen

1. Create `components/ui/screens/<name>.cpp` with a class that implements `ui::Screen`:

   | Method | Called |
   |---|---|
   | `build(root)` | Once, and again after a theme or language change |
   | `refresh(topic)` | When data for `topic` changes while the screen is visible, and with `Topic::All` when it is shown |
   | `on_open()` | When the user navigates to it: request data here |
   | `tab()` | Returns the sidebar item to highlight |
   | `title()` | Returns the label for the back chip on the next screen |

2. Declare its factory in `components/ui/shell.hpp`, add a `ScreenId` in `ui/include/ui/ui.hpp` and register it in `ui::start()` in `shell.cpp`.
3. Build it only from `kit::` components and `theme::` tokens. See [Design](design.md).
4. Add a capture to the screenshot tour in `test/sim/main.cpp`, so the screen can be reviewed with `--shots`, and an end-to-end scenario in `test/sim/e2e.cpp` for its main interaction.

LVGL must not delete the object whose event is running. When a click rebuilds its own container, wrap the work in `kit::later()`.
