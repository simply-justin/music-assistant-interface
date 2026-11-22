// The selected player's queue: items, add (with "already in the queue"), remove, move, clear.
#include <algorithm>

#include "backend/ma_commands.hpp"
#include "core/repeat_guard.hpp"
#include "features/features.hpp"

namespace app {
namespace {

constexpr int kMaxItems = 500;  // shortcut: one page; page through if real queues get longer

class Queue : public Feature {
public:
    explicit Queue(Context& ctx) : ctx_(ctx) {}

    void on_connected() override { resolve_queue(); }

    void on_event(const backend::Event& ev) override {
        if (ev.name == "player_updated" && ev.object_id == ctx_.player_id) return resolve_queue();  // (un)grouped?
        if (ev.object_id != ctx_.queue_id) return;
        if (ev.name == "queue_items_updated") {
            refresh();
        } else if (ev.name == "queue_updated" && ev.data) {
            ma::parse::queue_state(ev.data.get(), queue_);
            publish();
        } else if (ev.name == "queue_time_updated" && cJSON_IsNumber(ev.data.get())) {
            queue_.elapsed_s = static_cast<uint32_t>(ev.data->valuedouble);  // verify: data is the elapsed seconds
            publish();
        }
    }

    void on_command(const Command& c) override {
        if (std::holds_alternative<cmd::PlayerChanged>(c)) return resolve_queue();
        const std::string& q = ctx_.queue_id;
        if (q.empty()) return;
        if (auto* a = std::get_if<cmd::AddToQueue>(&c)) {
            add(*a);
        } else if (std::holds_alternative<cmd::ClearUpNext>(c)) {
            clear_up_next();
        } else if (auto* r = std::get_if<cmd::RemoveFromQueue>(&c)) {
            ctx_.send(ma::cmd::queue_delete_item(q, r->queue_item_id));
        } else if (auto* m = std::get_if<cmd::MoveInQueue>(&c)) {
            ctx_.send(ma::cmd::queue_move_item(q, m->queue_item_id, m->shift));
        } else if (auto* p = std::get_if<cmd::PlayQueueIndex>(&c)) {
            ctx_.send(ma::cmd::queue_play_index(q, p->index));
        }
    }

private:
    bool contains(const core::MediaItem& item) const {
        return std::any_of(queue_.items.begin(), queue_.items.end(), [&](const core::QueueItem& qi) {
            if (!qi.media) return false;
            if (qi.media->uri == item.uri) return true;
            return std::any_of(item.alternatives.begin(), item.alternatives.end(),
                               [&](const auto& alt) { return alt.uri == qi.media->uri; });
        });
    }

    void add(const cmd::AddToQueue& a) {
        if (!guard_.allow("add|" + a.item.uri, ctx_.now_ms)) return;
        if (!a.force && contains(a.item)) {
            cmd::AddToQueue again = a;
            again.force = true;
            ctx_.publish(Toast{i18n::tr(i18n::Str::toast_already_in_queue, a.item.name), Command{again},
                               i18n::tr(i18n::Str::toast_add_again)});
            return;
        }
        auto option = a.next ? ma::cmd::PlayOption::Next : ma::cmd::PlayOption::Add;
        std::string name = a.item.name, uri = a.item.uri, q = ctx_.queue_id;
        ctx_.send(ma::cmd::play_media(q, {uri}, option), [this, name, uri, q](backend::Response&&) {
            // Find the item just added (the newest with this uri) so the toast can offer Undo.
            ctx_.send(ma::cmd::queue_items(q, kMaxItems, 0), [this, name, uri](backend::Response&& r) {
                std::string id;
                const cJSON* it;
                cJSON_ArrayForEach(it, r.result.get()) {
                    core::QueueItem qi = ma::parse::queue_item(it);
                    if (qi.media && qi.media->uri == uri) id = qi.queue_item_id;  // last match = newest
                }
                Toast t{i18n::tr(i18n::Str::toast_added_to_queue, name), std::nullopt, {}, false};
                if (!id.empty()) {
                    t.action = Command{cmd::RemoveFromQueue{id}};
                    t.action_label = i18n::tr(i18n::Str::toast_undo);
                }
                ctx_.publish(std::move(t));
            });
        });
    }

    // The selected player plays from its group's queue when grouped, else its own (same id).
    void resolve_queue() {
        std::string player = ctx_.player_id;
        if (player.empty()) return;
        ctx_.send(ma::cmd::active_queue(player), [this, player](backend::Response&& r) {
            if (player != ctx_.player_id) return;  // player changed meanwhile
            bool has_queue = cJSON_IsObject(r.result.get());
            std::string q = has_queue ? backend::json_str(r.result.get(), "queue_id") : player;
            if (q != ctx_.queue_id) queue_ = core::QueueState{};
            ctx_.queue_id = q;
            if (has_queue) ma::parse::queue_state(r.result.get(), queue_);  // also gives the initial state
            refresh();
        });
    }

    // "Clear up next" keeps the current song playing. player_queues/clear can't do that: on MA
    // 2.10.13 it empties the whole queue and the player goes idle, even with skip_stop (tested
    // live 2026-11-22). So remove the upcoming items one by one; clear only when nothing plays.
    void clear_up_next() {
        const std::string& q = ctx_.queue_id;
        if (queue_.current_index < 0) return ctx_.send(ma::cmd::queue_clear(q));
        for (size_t i = queue_.items.size(); i-- > static_cast<size_t>(queue_.current_index) + 1;)
            ctx_.send(ma::cmd::queue_delete_item(q, queue_.items[i].queue_item_id));
    }

    void refresh() {
        if (ctx_.queue_id.empty()) return;
        std::string q = ctx_.queue_id;
        ctx_.send(ma::cmd::queue_items(q, kMaxItems, 0), [this, q](backend::Response&& r) {
            if (q != ctx_.queue_id) return;  // queue changed meanwhile
            queue_.queue_id = q;
            queue_.items.clear();
            const cJSON* it;
            cJSON_ArrayForEach(it, r.result.get()) queue_.items.push_back(ma::parse::queue_item(it));
            publish();
        });
    }

    void publish() { ctx_.publish(std::make_shared<const core::QueueState>(queue_)); }

    Context& ctx_;
    core::QueueState queue_;
    core::RepeatGuard guard_;
};

}  // namespace

std::unique_ptr<Feature> make_queue(Context& ctx) { return std::make_unique<Queue>(ctx); }

}  // namespace app
