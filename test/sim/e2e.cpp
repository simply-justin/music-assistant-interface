// End-to-end tests: the real UI and the real core on the mock server, headless, driven the way a
// person drives the panel: taps and drags go through LVGL's own input device (hit testing,
// gestures, scrolling), and checks read what is on screen.
//
//   test/sim/build/panel_e2e            # all scenarios; exit code 0 when every check passes
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../host/check.hpp"
#include "app/app.hpp"
#include "backend/mock_backend.hpp"
#include "fake_platform.hpp"
#include "host_ma_backend.hpp"  // host_now_ms()
#include "fonts/assets.hpp"
#include "lvgl.h"
#include "ui/ui.hpp"

namespace {

constexpr int W = 1280, H = 800;
std::vector<uint16_t> g_frame(W * H);

// The finger: the pointer input device reads this.
struct Finger {
    lv_point_t at{0, 0};
    bool down = false;
} g_finger;

// One panel: LVGL display + touch, the core on the mock server, and the UI. The UI can start only
// once per process, so the first-time setup scenario runs in its own process (--setup).
struct Panel {
    backend::MockBackend mock;
    app::MemorySettings settings;
    FakePlatform platform;
    std::vector<app::Update> inbox;
    std::unique_ptr<app::App> core;
    std::shared_ptr<const app::PlayersView> players;  // newest, for checks
    std::shared_ptr<const core::QueueState> queue;
    std::shared_ptr<const app::PanelView> panel;

    explicit Panel(bool first_time_setup) {
        if (first_time_setup) {
            mock.set_unconfigured();
        } else {
            settings.set("server_url", "mock");
            settings.set("token", "mock-token");
            settings.set("setup_done", "1");
        }
        core = std::make_unique<app::App>(mock, settings, platform, [this](app::Update u) {
            if (auto* p = std::get_if<std::shared_ptr<const app::PlayersView>>(&u)) players = *p;
            if (auto* q = std::get_if<std::shared_ptr<const core::QueueState>>(&u)) queue = *q;
            if (auto* v = std::get_if<std::shared_ptr<const app::PanelView>>(&u)) panel = *v;
            inbox.push_back(std::move(u));
        });
        core->start();
        ui::start([this](app::Command c) { core->handle(std::move(c)); });
        run(1500);
    }

    void run(uint32_t ms) {
        uint32_t end = host_now_ms() + ms;
        while (host_now_ms() < end) {
            platform.tick(host_now_ms());
            core->tick(host_now_ms());
            auto batch = std::move(inbox);
            inbox.clear();
            for (auto& u : batch) ui::apply(std::move(u));
            lv_timer_handler();
            usleep(2000);
        }
    }
};

Panel* g_panel = nullptr;

// ---- Finding things on screen

bool visible(lv_obj_t* o) {
    for (; o; o = lv_obj_get_parent(o))
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return false;
    return true;
}

lv_area_t area(lv_obj_t* o) {
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    return a;
}

bool clip_to(lv_area_t& a, const lv_area_t& b) {
    a = {std::max(a.x1, b.x1), std::max(a.y1, b.y1), std::min(a.x2, b.x2), std::min(a.y2, b.y2)};
    return a.x1 <= a.x2 && a.y1 <= a.y2;
}

// Visible and not clipped away: what is left after every parent's clip, at least half its height.
bool on_screen(lv_obj_t* o) {
    if (!visible(o)) return false;
    lv_area_t a = area(o);
    int32_t full_h = a.y2 - a.y1 + 1;
    lv_area_t screen{0, 0, W - 1, H - 1};
    if (!clip_to(a, screen)) return false;
    for (lv_obj_t* p = lv_obj_get_parent(o); p; p = lv_obj_get_parent(p)) {
        lv_area_t clip = area(p);
        if (!clip_to(a, clip)) return false;
    }
    return (a.y2 - a.y1 + 1) * 2 >= full_h;
}

void walk(lv_obj_t* o, const std::function<void(lv_obj_t*)>& fn) {
    fn(o);
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) walk(lv_obj_get_child(o, static_cast<int32_t>(i)), fn);
}

// The visible label with exactly this text (overlays first: they sit on top).
lv_obj_t* find_text(const std::string& text, bool contains = false) {
    lv_obj_t* found = nullptr;
    auto match = [&](lv_obj_t* o) {
        if (found || !lv_obj_check_type(o, &lv_label_class) || !on_screen(o)) return;
        std::string t = lv_label_get_text(o);
        if (contains ? t.find(text) != std::string::npos : t == text) found = o;
    };
    walk(lv_layer_top(), match);
    if (!found) walk(lv_screen_active(), match);
    return found;
}

lv_obj_t* find_type(const lv_obj_class_t* cls) {
    lv_obj_t* found = nullptr;
    auto match = [&](lv_obj_t* o) {
        if (!found && lv_obj_check_type(o, cls) && on_screen(o)) found = o;
    };
    walk(lv_layer_top(), match);
    if (!found) walk(lv_screen_active(), match);
    return found;
}

lv_point_t center(lv_obj_t* o) {
    lv_area_t a = area(o);
    return {(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
}

// ---- Touch

void press_at(lv_point_t p, uint32_t hold_ms) {
    g_finger.at = p;
    g_finger.down = true;
    g_panel->run(hold_ms);
    g_finger.down = false;
    g_panel->run(150);
}

// What a press at this label would hit, when that is something that acts on a tap (it has an
// event handler): the label's row or button, not a screen-wide container behind a plain label.
lv_obj_t* tap_target(lv_obj_t* label) {
    lv_point_t p = center(label);
    lv_obj_t* hit = lv_indev_search_obj(lv_layer_top(), &p);
    if (!hit) hit = lv_indev_search_obj(lv_screen_active(), &p);
    for (lv_obj_t* o = label; o; o = lv_obj_get_parent(o))
        if (o == hit) return lv_obj_get_event_count(hit) > 0 ? hit : nullptr;
    return nullptr;
}

// Taps the visible text that leads somewhere (the same words can also sit on a plain label).
bool tap(const std::string& text) {
    lv_obj_t* first = nullptr;
    lv_obj_t* actionable = nullptr;
    auto match = [&](lv_obj_t* o) {
        if (actionable || !lv_obj_check_type(o, &lv_label_class) || !on_screen(o) || text != lv_label_get_text(o)) return;
        if (!first) first = o;
        if (tap_target(o)) actionable = o;
    };
    walk(lv_layer_top(), match);
    if (!actionable) walk(lv_screen_active(), match);
    lv_obj_t* l = actionable ? actionable : first;
    if (!l) {
        std::printf("    (nothing on screen reads \"%s\")\n", text.c_str());
        return false;
    }
    press_at(center(l), 80);
    g_panel->run(250);
    return true;
}

void drag(lv_point_t from, lv_point_t to) {
    g_finger.at = from;
    g_finger.down = true;
    g_panel->run(60);
    for (int i = 1; i <= 12; ++i) {
        g_finger.at = {from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12};
        g_panel->run(20);
    }
    g_finger.down = false;
    g_panel->run(300);
}

// The on-screen keyboard is a button matrix: pick the key by its label and fire it, exactly as a
// press on that key does.
void type(const std::string& keys) {
    lv_obj_t* kb = find_type(&lv_buttonmatrix_class);
    if (!kb) {
        std::printf("    (no keyboard on screen)\n");
        return;
    }
    auto press_key = [kb](const std::string& key) {
        for (uint32_t id = 0;; ++id) {
            const char* t = lv_buttonmatrix_get_button_text(kb, id);
            if (!t) break;
            if (key == t) {
                lv_buttonmatrix_set_selected_button(kb, id);
                lv_obj_send_event(kb, LV_EVENT_VALUE_CHANGED, nullptr);
                g_panel->run(30);
                return;
            }
        }
        std::printf("    (no key \"%s\")\n", key.c_str());
    };
    for (char c : keys) press_key(std::string(1, c));
}

void key(const std::string& label) {
    lv_obj_t* kb = find_type(&lv_buttonmatrix_class);
    for (uint32_t id = 0; kb; ++id) {
        const char* t = lv_buttonmatrix_get_button_text(kb, id);
        if (!t) break;
        if (label == t) {
            lv_buttonmatrix_set_selected_button(kb, id);
            lv_obj_send_event(kb, LV_EVENT_VALUE_CHANGED, nullptr);
            g_panel->run(300);
            return;
        }
    }
    std::printf("    (no key \"%s\")\n", label.c_str());
}

lv_obj_t* scroll_parent(lv_obj_t* o) {
    while (o && !(lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) && lv_obj_get_scroll_bottom(o) + lv_obj_get_scroll_y(o) > 0))
        o = lv_obj_get_parent(o);
    return o;
}

void close_overlays() {
    lv_obj_clean(lv_layer_top());
    g_panel->run(100);
}

// ---- Scenarios (mock data: speakers Living Room + Kitchen, Viva La Vida in three sources)

TEST(home_shows_the_main_sections) {
    ui::open(ui::ScreenId::Home);
    g_panel->run(500);
    CHECK(find_text("Recently played"));
    CHECK(find_text("Your stations"));
    CHECK(find_text("Radio Paradise"));
}

TEST(sidebar_navigates_between_screens) {
    CHECK(tap("Library"));
    CHECK(find_text("Favourites"));  // the first Library tab
    CHECK(tap("Queue"));
    CHECK(find_text("Viva La Vida"));  // the mock queue's current song
    CHECK(tap("Home"));
    CHECK(find_text("Recently played"));
}

TEST(drag_scrolls_home) {
    ui::open(ui::ScreenId::Home);
    g_panel->run(300);
    lv_obj_t* stations = find_text("Your stations");
    lv_obj_t* page = stations ? scroll_parent(stations) : nullptr;
    CHECK(page);
    if (!page) return;
    drag({700, 600}, {700, 250});
    CHECK(lv_obj_get_scroll_y(page) > 0);
    drag({700, 250}, {700, 650});
    CHECK(lv_obj_get_scroll_y(page) == 0);
}

TEST(speaker_button_switches_the_speaker) {
    ui::open(ui::ScreenId::Home);
    g_panel->run(300);
    CHECK(g_panel->players && g_panel->players->selected_id == "living");
    CHECK(tap(ui::icon::speaker));
    CHECK(find_text("Kitchen"));
    CHECK(tap("Kitchen"));
    CHECK(!find_text("Kitchen"));  // the sheet closed on pick
    CHECK(g_panel->players && g_panel->players->selected_id == "kitchen");
    CHECK(g_panel->settings.get("player") == "kitchen");
    // And back, so later scenarios start from the default.
    tap(ui::icon::speaker);
    tap("Living Room");
    close_overlays();
}

TEST(search_by_typing_merges_duplicates) {
    CHECK(tap("Search"));
    CHECK(find_type(&lv_buttonmatrix_class));
    type("viva");
    g_panel->run(1500);
    CHECK(find_text("Viva La Vida"));
    CHECK(find_text("Viva La Vida (Live in Buenos Aires)"));  // a different version stays separate
    CHECK(find_text("+2", true));                           // three sources, one row
    tap("Home");
}

// Search: the action key hides the keyboard so the results are visible; tapping the field brings
// it back. Pressed with the finger on the real key (not the shortcut that fires the key event).
TEST(search_key_hides_keyboard_and_field_brings_it_back) {
    CHECK(tap("Search"));
    lv_obj_t* input = find_type(&lv_textarea_class);
    CHECK(input);
    if (!input) return;
    press_at(center(input), 80);  // with text from an earlier search the keyboard starts hidden
    g_panel->run(300);
    lv_obj_t* kb = find_type(&lv_buttonmatrix_class);
    CHECK(kb);
    if (!kb) return;
    type("a");
    lv_area_t a = area(kb);
    int32_t row_h = (lv_area_get_height(&a) - 32 - 3 * 10) / 4;
    press_at({a.x2 - 80, a.y1 + 16 + row_h + 10 + row_h / 2}, 80);  // the action key, end of row 2
    g_panel->run(400);
    CHECK(!find_type(&lv_buttonmatrix_class));  // hidden
    lv_obj_t* field = find_type(&lv_textarea_class);
    CHECK(field);
    if (field) press_at(center(field), 80);
    g_panel->run(300);
    CHECK(find_type(&lv_buttonmatrix_class));  // back
    tap("Home");
}

TEST(tapping_a_favourite_plays_it) {
    ui::open(ui::ScreenId::Home);
    g_panel->run(300);
    CHECK(tap("Radio Paradise"));  // the favourite tile (it comes before the stations row)
    // Shown in the player bar at once, before the server answers.
    int at_once = 0;
    walk(lv_screen_active(), [&](lv_obj_t* o) {
        if (lv_obj_check_type(o, &lv_label_class) && on_screen(o) && std::string(lv_label_get_text(o)) == "Radio Paradise" &&
            area(o).y1 >= H - 96)
            ++at_once;
    });
    CHECK(at_once == 1);
    g_panel->run(800);
    // Play replaces the queue with what was tapped, and the player bar shows it.
    CHECK(g_panel->queue && g_panel->queue->items.size() == 1 && g_panel->queue->items[0].name == "Radio Paradise");
    lv_obj_t* title = find_text("Radio Paradise");
    int bar_hits = 0;
    walk(lv_screen_active(), [&](lv_obj_t* o) {
        if (lv_obj_check_type(o, &lv_label_class) && on_screen(o) && std::string(lv_label_get_text(o)) == "Radio Paradise" &&
            area(o).y1 >= H - 96)  // the player bar
            ++bar_hits;
    });
    CHECK(title && bar_hits == 1);
}

// The panel publishes its status (uptime, Wi-Fi) every 30 s. A tap that is in progress when one
// arrives must still land: the screen under the finger must not be rebuilt for data it doesn't show.
TEST(tap_survives_a_status_update) {
    ui::open(ui::ScreenId::Home);
    g_panel->run(300);
    lv_obj_t* tile = find_text("NPO Radio 2");
    CHECK(tile && g_panel->panel);
    if (!tile || !g_panel->panel) return;
    auto status = std::make_shared<app::PanelView>(*g_panel->panel);
    status->system.uptime_s += 30;
    g_finger.at = center(tile);
    g_finger.down = true;
    g_panel->run(60);
    ui::apply(std::shared_ptr<const app::PanelView>(status));  // arrives mid-press
    g_panel->run(60);
    g_finger.down = false;
    g_panel->run(800);
    CHECK(g_panel->queue && !g_panel->queue->items.empty() && g_panel->queue->items[0].name == "NPO Radio 2");
}

TEST(language_switch_applies_instantly) {
    CHECK(tap("Settings"));
    CHECK(tap("Language"));
    CHECK(tap("Nederlands"));
    g_panel->run(500);
    CHECK(find_text("Bibliotheek"));
    CHECK(g_panel->settings.get("lang") == "nl");
    CHECK(tap("English"));
    g_panel->run(500);
    CHECK(find_text("Library"));
    tap("Home");
}

// A key held down while the screen replaces the keyboard (Join → the step changes) must not read
// the freed key map: crashed the panel on the device. Run under ASan in CI.
TEST(setup_keyboard_replaced_under_a_finger) {
    CHECK(tap("Get started"));
    g_panel->run(1200);
    CHECK(tap("HomeNetwork"));
    lv_obj_t* kb = find_type(&lv_buttonmatrix_class);
    CHECK(kb);
    if (!kb) return;
    lv_area_t a = area(kb);
    g_finger.at = {a.x1 + 40, a.y1 + 40};  // on the "q" key
    g_finger.down = true;
    g_panel->run(100);
    g_panel->core->handle(app::cmd::SetupGo{app::SetupStep::Wifi});  // the step changes under the finger
    g_panel->run(200);
    g_finger.down = false;
    g_panel->run(200);
    CHECK(find_text("Connect to Wi-Fi"));
    CHECK(tap("Back"));  // back to the start for the next scenario
    g_panel->run(300);
}

TEST(setup_first_time_reaches_home) {
    // A fresh panel: no server, no token. The fake phone sends the token after 3 s.
    CHECK(tap("Get started"));
    g_panel->run(1200);  // Wi-Fi scan
    CHECK(tap("HomeNetwork"));
    type("wrong");
    key("Join");
    g_panel->run(1500);
    CHECK(find_text("Check capitals", true));  // wrong password is explained
    lv_obj_t* field = find_type(&lv_textarea_class);
    if (field) lv_textarea_set_text(field, "");
    type("music");
    key("Join");
    g_panel->run(3000);
    CHECK(find_text("Find your music server"));
    CHECK(tap("Music Assistant"));
    g_panel->run(4000);  // sign-in page, then the phone delivers the token
    CHECK(find_text("Choose a speaker"));
    CHECK(tap("Continue"));
    CHECK(find_text("You're all set"));
    CHECK(tap("Start listening"));
    g_panel->run(1000);
    CHECK(find_text("Recently played"));
    CHECK(g_panel->settings.get("setup_done") == "1");
}

}  // namespace

int main(int argc, char** argv) {
    bool setup = argc > 1 && !std::strcmp(argv[1], "--setup");
    lv_init();
    lv_tick_set_cb([] { return host_now_ms(); });
    lv_display_t* disp = lv_display_create(W, H);
    lv_display_set_buffers(disp, g_frame.data(), nullptr, g_frame.size() * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, [](lv_indev_t*, lv_indev_data_t* d) {
        d->point = g_finger.at;
        d->state = g_finger.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    });

    // Scenarios named setup_* need a panel that was never set up; the rest a configured one.
    auto& tests = check::registry();
    tests.erase(std::remove_if(tests.begin(), tests.end(),
                               [setup](const check::Test& t) { return (std::strncmp(t.name, "setup_", 6) == 0) != setup; }),
                tests.end());
    Panel panel(setup);
    g_panel = &panel;
    return check::run_all(setup ? "e2e (setup)" : "e2e");
}
