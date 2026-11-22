// PC simulator: same core and UI as the panel, one thread (core + LVGL), X11 window or headless
// screenshots.
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "app/app.hpp"
#include "backend/mock_backend.hpp"
#include "host_ma_backend.hpp"
#include "lvgl.h"
#include "ui/ui.hpp"

namespace {

std::map<std::string, std::string> read_env(const char* path) {
    std::map<std::string, std::string> env;
    std::ifstream f(path);
    for (std::string line; std::getline(f, line);) {
        auto eq = line.find('=');
        if (eq != std::string::npos && line[0] != '#') env[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return env;
}

// Headless display: one full-frame RGB565 buffer we can dump.
constexpr int W = 1280, H = 800;
std::vector<uint16_t> g_frame(W * H);

void write_ppm(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << W << " " << H << "\n255\n";
    for (uint16_t px : g_frame) {
        unsigned char rgb[3] = {static_cast<unsigned char>((px >> 11 & 0x1F) * 255 / 31),
                                static_cast<unsigned char>((px >> 5 & 0x3F) * 255 / 63),
                                static_cast<unsigned char>((px & 0x1F) * 255 / 31)};
        f.write(reinterpret_cast<char*>(rgb), 3);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const char* live = nullptr;
    const char* shots = nullptr;
    for (int i = 1; i + 1 < argc; ++i) {
        if (!std::strcmp(argv[i], "--live")) live = argv[i + 1];
        if (!std::strcmp(argv[i], "--shots")) shots = argv[i + 1];
    }

    lv_init();
    lv_tick_set_cb([] { return host_now_ms(); });
    lv_display_t* disp;
    if (shots) {
        disp = lv_display_create(W, H);
        lv_display_set_buffers(disp, g_frame.data(), nullptr, g_frame.size() * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(disp, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
    } else {
        disp = lv_x11_window_create("Music panel (simulator)", W, H);
        lv_x11_inputs_create(disp, nullptr);
    }

    std::unique_ptr<backend::Backend> be;
    if (live) {
        auto env = read_env(live);
        be = std::make_unique<HostMaBackend>(env["MA_URL"], env["MA_TOKEN"]);
    } else {
        be = std::make_unique<backend::MockBackend>();
    }
    app::MemorySettings settings;
    std::vector<app::Update> inbox;
    app::App core(*be, settings, [&](app::Update u) { inbox.push_back(std::move(u)); });
    core.start();  // first: the UI's first screen already sends commands
    ui::start([&](app::Command c) { core.handle(c); });  // same thread: the queue is just `inbox`

    auto run = [&](uint32_t ms) {
        uint32_t end = host_now_ms() + ms;
        while (host_now_ms() < end) {
            core.tick(host_now_ms());
            auto batch = std::move(inbox);
            inbox.clear();
            for (auto& u : batch) ui::apply(std::move(u));
            lv_timer_handler();
            usleep(5000);
        }
    };

    if (!shots) {
        for (;;) run(1000);
    }

    // Screenshot tour: every screen and overlay, with mock (or live) data.
    std::string dir = shots;
    auto shot = [&](const char* name) {
        run(700);
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(disp);
        write_ppm(dir + "/" + name + ".ppm");
        std::printf("wrote %s/%s.ppm\n", dir.c_str(), name);
    };
    run(1500);
    shot("01_home");
    ui::open(ui::ScreenId::NowPlaying);
    shot("02_now_playing");
    ui::open(ui::ScreenId::Search);
    shot("03_search_keyboard");
    core.handle(app::cmd::Search{"viva"});
    run(1200);
    shot("04_search_results");
    ui::open(ui::ScreenId::Queue);
    shot("05_queue");
    ui::open(ui::ScreenId::Library);
    shot("06_library_playlists");
    core::MediaItem playlist;
    playlist.type = core::MediaType::Playlist;
    playlist.item_id = "p2";
    playlist.provider = "library";
    playlist.name = "Friday Night Mix";
    playlist.uri = "library://playlist/p2";
    playlist.editable = true;
    ui::open_detail(playlist);
    shot("07_playlist_detail");
    ui::open(ui::ScreenId::Settings);
    shot("08_settings");
    ui::open(ui::ScreenId::Home);
    run(300);
    ui::open_speakers();
    shot("09_speakers");
    ui::open_volume();
    shot("10_volume");
    core::MediaItem track;
    track.type = core::MediaType::Track;
    track.name = "Viva La Vida";
    track.uri = "library://track/12";
    track.provider = "library";
    track.duration_s = 242;
    track.artists.push_back({"3", "library", "Coldplay", "library://artist/3"});
    core::MediaItem alt = track;
    alt.uri = "spotify://track/x";
    alt.provider = "spotify--a1";
    alt.mappings.push_back({"x", "spotify", "spotify--a1", true, false});
    track.alternatives.push_back(alt);
    ui::open_track_menu(track);
    shot("11_track_menu");
    core.handle(app::cmd::SetLanguage{i18n::Lang::nl});
    run(300);
    ui::open(ui::ScreenId::Home);
    shot("12_home_dutch");
    return 0;
}
