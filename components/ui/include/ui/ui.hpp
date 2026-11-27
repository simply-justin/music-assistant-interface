#pragma once
#include <atomic>
// The panel UI (LVGL 9). Runs on the GUI task only: create the LVGL display first, call start(),
// then feed every core Update to apply(). Commands go out through the Send callback.
#include <functional>
#include <memory>

#include "app/commands.hpp"
#include "img/artwork.hpp"
#include "app/updates.hpp"

namespace ui {

using Send = std::function<void(app::Command)>;

enum class ScreenId : uint8_t { Home, Search, Library, Queue, Settings, NowPlaying, Detail, Setup, Boot };
enum class SettingsPage : uint8_t { Display, Wifi, Server, Language, About, Appearance };

void start(Send send);

// Artwork: the platform fetches `url` and decodes + scales it to w×h (img::scale) off the GUI
// task, then calls `done` ON THE GUI TASK (nullptr on failure). Without a loader: placeholders.
struct ArtRequest {
    std::string url;
    int w, h, radius;
    uint32_t bg;  // 0xRRGGBB behind the rounded corners
    // How many boxes on screen still wait for this image. A loader skips the download at 0 (the
    // rows were scrolled away before their turn came).
    std::shared_ptr<std::atomic<int>> wanted;
};
using ArtLoader = std::function<void(ArtRequest, std::function<void(std::shared_ptr<img::Bitmap>)>)>;
void set_art_loader(ArtLoader loader, size_t cache_budget_bytes);
void apply(app::Update update);

// Navigation entry points (also used by the simulator's screenshot mode).
void open(ScreenId id);
void open_detail(const core::MediaItem& item);
void open_speakers();
void open_volume();
void open_track_menu(const core::MediaItem& item);
void open_settings_page(SettingsPage page);
void open_favourite_preview(const core::MediaItem& item);
void show_ambient();  // the sleep screen (normally after the idle timeout)
void open_group(const std::string& leader_id);  // speaker grouping sheet

}  // namespace ui
