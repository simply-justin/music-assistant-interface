// Queue: now playing on the left, "Up next" on the right. Tap plays, × removes, the handle drags a
// song to another place, long-press offers Play next / Move up / Move down.
#include <algorithm>
#include <map>

#include "../shell.hpp"
#include "../text.hpp"

namespace ui {
namespace {

class Queue : public Screen {
public:
    void build(lv_obj_t* root) override {
        lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(root, theme::kPad, 0);
        lv_obj_set_style_pad_column(root, 40, 0);
        left_ = kit::column(root, 20);
        lv_obj_set_size(left_, 340, lv_pct(100));
        right_ = kit::column(root, 16);
        lv_obj_set_flex_grow(right_, 1);
        lv_obj_set_height(right_, lv_pct(100));
    }

    void refresh(Topic t) override {
        if (t != Topic::All && t != Topic::Queue && t != Topic::Players) return;
        build_left();
        build_right();
    }

    ScreenId tab() const override { return ScreenId::Queue; }
    std::string title() const override { return tr(i18n::Str::nav_queue); }

private:
    void build_left() {
        lv_obj_clean(left_);
        kit::text(left_, tr(i18n::Str::nav_queue), theme::h1(), theme::text());
        auto item = state().now_playing();
        if (!item) return;
        kit::overline(left_, tr(i18n::Str::home_now_playing));
        lv_obj_t* np = kit::column(left_, 16);
        kit::art(np, *item, 340, theme::r_art());
        kit::text(np, item->name, theme::h2(), theme::text());
        lv_obj_t* sub = kit::row(np, 8);
        kit::text(sub, kit::subtitle(*item), theme::body(), theme::text2());
        kit::badge(sub, item->badge_provider());
        kit::on_click(np, [] { open(ScreenId::NowPlaying); });
        lv_obj_t* status = kit::row(left_, 12);
        kit::play_button(status, state().playing(), [] { send(app::cmd::PlayPause{}); }, 56);
        const core::PlayerState* p = state().selected();
        if (p) kit::text(status, i18n::tr(i18n::Str::queue_playing_on, p->name), theme::meta(), theme::text2());
    }

    void build_right() {
        int32_t keep_y = list_ ? lv_obj_get_scroll_y(list_) : 0;  // a queue update must not jump to the top
        list_ = nullptr;
        lv_obj_clean(right_);
        const core::QueueState* q = state().queue.get();
        size_t first = q && q->current_index >= 0 ? static_cast<size_t>(q->current_index) + 1 : 0;
        // Up next lists songs only: a station MA left in the queue is not something to queue or skip to.
        auto items = std::make_shared<std::vector<std::pair<int, core::QueueItem>>>();
        for (size_t i = first; q && i < q->items.size(); ++i)
            if (!q->items[i].media || q->items[i].media->type != core::MediaType::Radio)
                items->emplace_back(static_cast<int>(i), q->items[i]);
        size_t count = items->size();
        uint32_t total = 0;
        for (const auto& e : *items) total += e.second.duration_s;

        lv_obj_t* head = kit::row(right_, 12);
        lv_obj_set_size(head, lv_pct(100), 64);
        lv_obj_t* titles = kit::column(head, 2);
        lv_obj_set_flex_grow(titles, 1);
        kit::text(titles, tr(i18n::Str::queue_up_next), theme::h2(), theme::text());
        lv_obj_set_style_pad_left(titles, 8, 0);  // in line with the row content below (media_row's padding)
        if (count) kit::text(titles, songs(count) + " · " + total_duration(total), theme::meta(), theme::text2());
        // Shuffle sits in the player bar below; only Clear belongs to this list.
        if (count) {
            kit::chip(head, icon::close, tr(i18n::Str::queue_clear), false, [count] {
                std::string body = i18n::tr(i18n::Str::queue_clear_body, std::to_string(count));
                confirm(tr(i18n::Str::queue_clear_title), body, tr(i18n::Str::queue_clear_keep), tr(i18n::Str::queue_clear_go),
                        [] { send(app::cmd::ClearUpNext{}); });
            });
        }

        if (!count) {
            kit::text(right_, tr(i18n::Str::queue_empty), theme::lead(), theme::text2(), true);
            return;
        }
        // A service that isn't answering: its songs below are marked unavailable by Music Assistant.
        std::map<std::string, int> down;
        for (const auto& e : *items)
            if (!e.second.available && e.second.media) ++down[e.second.media->badge_provider()];
        if (!down.empty()) {
            auto worst = std::max_element(down.begin(), down.end(), [](auto& a, auto& b) { return a.second < b.second; });
            banner(right_, i18n::tr(i18n::Str::queue_provider_down, kit::provider_name(worst->first)),
                   i18n::tr(i18n::Str::queue_provider_down_body, std::to_string(worst->second)));
        }
        list_ = kit::virtual_list(right_, items->size(), kRowH, kGap, [items](lv_obj_t* list, size_t n) {
            const core::QueueItem& qi = (*items)[n].second;
            core::MediaItem m = qi.media ? *qi.media : core::MediaItem{};
            if (!qi.media) m.name = qi.name;
            int index = (*items)[n].first;
            std::string id = qi.queue_item_id;
            kit::RowOptions o;
            o.height = kRowH;
            o.on_tap = [index] { send(app::cmd::PlayQueueIndex{index}); };
            o.on_remove = [id] { send(app::cmd::RemoveFromQueue{id}); };
            lv_obj_t* r = kit::media_row(list, m, o);
            if (!qi.available) kit::tag(r, tr(i18n::Str::queue_unavailable), theme::warning());
            std::string name = m.name;
            kit::on_long_press(r, [id, name] { queue_item_menu(id, name); });
            drag_handle(r, id, static_cast<int>(n), static_cast<int>(items->size()));
            return r;
        });
        if (keep_y > 0) {
            lv_obj_update_layout(list_);
            lv_obj_scroll_to_y(list_, keep_y, LV_ANIM_OFF);
        }
    }

    // Drag: the row follows the finger; on release it moves by the rows it crossed. The handle
    // neither scrolls nor hands the drag to the list, so the list stays still meanwhile.
    struct Drag {
        lv_obj_t* row = nullptr;
        int32_t start_y = 0, row_y = 0;
        std::string id;
        int index = 0, count = 0;
    };
    static Drag& drag() {
        static Drag d;
        return d;
    }

    static void drag_handle(lv_obj_t* row, const std::string& id, int index, int count) {
        lv_obj_t* h = kit::icon(row, icon::drag, 22, theme::text3());
        lv_obj_add_flag(h, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(h, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(h, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
        lv_obj_remove_flag(h, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_obj_set_ext_click_area(h, 17);  // 22 + 2 × 17 = 56 px
        auto* info = new Drag{row, 0, 0, id, index, count};
        lv_obj_add_event_cb(h, [](lv_event_t* e) {
            auto* info = static_cast<Drag*>(lv_event_get_user_data(e));
            lv_event_code_t code = lv_event_get_code(e);
            Drag& d = drag();
            if (code == LV_EVENT_DELETE) {
                if (d.row == info->row) d.row = nullptr;
                delete info;
                return;
            }
            lv_indev_t* indev = lv_indev_active();
            if (!indev) return;
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            if (code == LV_EVENT_PRESSED) {
                d = *info;
                d.start_y = p.y;
                d.row_y = lv_obj_get_y(info->row);
                lv_obj_move_foreground(info->row);
                lv_obj_set_style_bg_color(info->row, theme::elevated(), 0);
                lv_obj_set_style_bg_opa(info->row, LV_OPA_COVER, 0);
            } else if (code == LV_EVENT_PRESSING && d.row == info->row) {
                lv_obj_set_y(d.row, d.row_y + p.y - d.start_y);
            } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && d.row == info->row) {
                int32_t dy = p.y - d.start_y;
                int shift = static_cast<int>((dy + (dy >= 0 ? kPitch / 2 : -kPitch / 2)) / kPitch);
                shift = std::clamp(shift, -d.index, d.count - 1 - d.index);  // stays within up next
                lv_obj_set_y(d.row, d.row_y);
                lv_obj_set_style_bg_opa(d.row, LV_OPA_TRANSP, 0);
                d.row = nullptr;
                if (shift) send(app::cmd::MoveInQueue{info->id, shift});
            }
        }, LV_EVENT_ALL, info);
    }

    static constexpr int32_t kRowH = 76, kGap = 4, kPitch = kRowH + kGap;

    // Long-press: Play next / Move up / Move down (instead of drag).
    static void queue_item_menu(const std::string& id, const std::string& name) {
        lv_obj_t* s = kit::sheet(560);
        kit::text(s, name, theme::h2(), theme::text());
        kit::button(s, kit::Button::Secondary, icon::play_next, tr(i18n::Str::menu_play_next), [id] {
            send(app::cmd::MoveInQueue{id, -1000});  // to the front of "up next" (verify: the server clamps the shift)
            kit::close_sheets();
        });
        kit::button(s, kit::Button::Secondary, icon::chevron_up, tr(i18n::Str::queue_move_up), [id] {
            send(app::cmd::MoveInQueue{id, -1});
            kit::close_sheets();
        });
        kit::button(s, kit::Button::Secondary, icon::chevron_down, tr(i18n::Str::queue_move_down), [id] {
            send(app::cmd::MoveInQueue{id, 1});
            kit::close_sheets();
        });
    }

    lv_obj_t *left_ = nullptr, *right_ = nullptr;
    lv_obj_t* list_ = nullptr;
};

}  // namespace

std::unique_ptr<Screen> make_queue() { return std::make_unique<Queue>(); }

}  // namespace ui
