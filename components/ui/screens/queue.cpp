// Queue: now playing on the left, "Up next" on the right. Tap plays, × removes, long-press offers
// Play next / Move up / Move down (no drag: expensive in a recycled LVGL list).
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

    bool mini_player() const override { return false; }
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
        kit::art(np, *item, 340, 16);
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
        lv_obj_clean(right_);
        const core::QueueState* q = state().queue.get();
        size_t first = q && q->current_index >= 0 ? static_cast<size_t>(q->current_index) + 1 : 0;
        size_t count = q && q->items.size() > first ? q->items.size() - first : 0;
        uint32_t total = 0;
        for (size_t i = first; q && i < q->items.size(); ++i) total += q->items[i].duration_s;

        lv_obj_t* head = kit::row(right_, 12);
        lv_obj_set_size(head, lv_pct(100), 64);
        lv_obj_t* titles = kit::column(head, 2);
        lv_obj_set_flex_grow(titles, 1);
        kit::text(titles, tr(i18n::Str::queue_up_next), theme::h2(), theme::text());
        if (count) kit::text(titles, songs(count) + " · " + duration(total), theme::meta(), theme::text2());
        bool shuffle = q && q->shuffle;
        kit::chip(head, icon::shuffle, tr(i18n::Str::queue_shuffle), shuffle, [shuffle] { send(app::cmd::SetShuffle{!shuffle}); });
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
        for (size_t i = first; i < q->items.size(); ++i)
            if (!q->items[i].available && q->items[i].media) ++down[q->items[i].media->badge_provider()];
        if (!down.empty()) {
            auto worst = std::max_element(down.begin(), down.end(), [](auto& a, auto& b) { return a.second < b.second; });
            banner(right_, i18n::tr(i18n::Str::queue_provider_down, kit::provider_name(worst->first)),
                   i18n::tr(i18n::Str::queue_provider_down_body, std::to_string(worst->second)));
        }
        lv_obj_t* list = kit::scroll_column(right_, 4);
        lv_obj_set_width(list, lv_pct(100));
        lv_obj_set_flex_grow(list, 1);
        for (size_t i = first; i < q->items.size() && i < first + 200; ++i) {  // shortcut: capped, not virtualized yet
            const core::QueueItem& qi = q->items[i];
            core::MediaItem m = qi.media ? *qi.media : core::MediaItem{};
            if (!qi.media) m.name = qi.name;
            int index = static_cast<int>(i);
            std::string id = qi.queue_item_id;
            kit::RowOptions o;
            o.height = 76;
            o.on_tap = [index] { send(app::cmd::PlayQueueIndex{index}); };
            o.on_remove = [id] { send(app::cmd::RemoveFromQueue{id}); };
            lv_obj_t* r = kit::media_row(list, m, o);
            if (!qi.available) kit::tag(r, tr(i18n::Str::queue_unavailable), theme::warning());
            std::string name = m.name;
            kit::on_long_press(r, [id, name] {
                lv_obj_t* s = kit::sheet(560);
                kit::text(s, name, theme::h2(), theme::text());
                kit::button(s, kit::Button::Secondary, icon::play_next, tr(i18n::Str::menu_play_next), [id] {
                    send(app::cmd::MoveInQueue{id, -1000});  // to the front of "up next"; server clamps
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
            });
        }
    }

    lv_obj_t *left_ = nullptr, *right_ = nullptr;
};

}  // namespace

std::unique_ptr<Screen> make_queue() { return std::make_unique<Queue>(); }

}  // namespace ui
