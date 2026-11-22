#pragma once
// The panel UI (LVGL 9). Runs on the GUI task only: create the LVGL display first, call start(),
// then feed every core Update to apply(). Commands go out through the Send callback.
#include <functional>

#include "app/commands.hpp"
#include "app/updates.hpp"

namespace ui {

using Send = std::function<void(app::Command)>;

enum class ScreenId : uint8_t { Home, Search, Library, Queue, Settings, NowPlaying, Detail, SettingsPage, Setup, Boot };
enum class SettingsPage : uint8_t { Display, Wifi, Server, Language, About };

void start(Send send);
void apply(app::Update update);

// Navigation entry points (also used by the simulator's screenshot mode).
void open(ScreenId id);
void open_detail(const core::MediaItem& item);
void open_speakers();
void open_volume();
void open_track_menu(const core::MediaItem& item);
void open_settings_page(SettingsPage page);
void open_favourite_preview(const core::MediaItem& item);
void show_ambient();  // the dimmed clock (normally after the idle timeout)

}  // namespace ui
