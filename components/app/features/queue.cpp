// The selected player's queue: items, add (with "already in the queue"), remove, move, clear.
#include <algorithm>

#include "backend/ma_commands.hpp"
#include "core/repeat_guard.hpp"
#include "features/features.hpp"

namespace app {
namespace {

constexpr int kPage = 500;        // items per request
constexpr int kMaxItems = 5000;   // a queue longer than this is cut off (PSRAM: ~1 KB per item)
constexpr uint32_t kRefreshDelayMs = 250;  // bursts of queue_items_updated (Clear up next) fetch once
constexpr uint32_t kUndoWaitMs = 3000;     // longest wait for the added song to show in the queue

class Queue : public Feature {
public:
    explicit Queue(Context& ctx) : ctx_(ctx) {}

    void on_connected() override { resolve_queue(); }

    void on_event(const backend::Event& ev) override {
        if (ev.name == "player_updated" && ev.object_id == ctx_.player_id) {
            // MA sends this every few seconds while playing; only a (un)group can move the queue.
            if (!ev.data) return;
            core::PlayerState p = ma::parse::player(ev.data.get());
            std::string group = p.synced_to;
            for (const auto& m : p.group_members) group += ',' + m;
            if (group == group_) return;
            group_ = group;
            return resolve_queue();
        }
        if (ev.object_id != ctx_.queue_id) return;
        if (ev.name == "queue_items_updated") {
            refresh_at_ = ctx_.now_ms + kRefreshDelayMs;
        } else if (ev.name == "queue_updated" && ev.data) {
            ma::parse::queue_state(ev.data.get(), queue_);
            publish();
        } else if (ev.name == "queue_time_updated" && cJSON_IsNumber(ev.data.get())) {
            queue_.elapsed_s = static_cast<uint32_t>(ev.data->valuedouble);  // verify: data is the elapsed seconds
            ctx_.publish(QueueTime{ctx_.queue_id, queue_.elapsed_s});
        }
    }

    void on_command(const Command& c) override {
        if (std::holds_alternative<cmd::PlayerChanged>(c)) return resolve_queue();
        const std::string& q = ctx_.queue_id;
        if (q.empty()) return;
        if (auto* a = std::get_if<cmd::AddToQueue>(&c)) {
            add(*a);
        } else if (auto* t = std::get_if<cmd::TransferQueue>(&c)) {
            ctx_.send(ma::cmd::transfer(t->from_player, t->to_player));  // a player's own queue has its id
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
        if (a.item.type == core::MediaType::Radio) return;  // a live stream never ends, so nothing after it would play
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
        size_t before = count(uri);
        ctx_.send(ma::cmd::play_media(q, {uri}, option), [this, name, uri, before](backend::Response&&) {
            // The toast offers Undo once the queue (refreshed by the server's event) shows the song.
            undo_ = Undo{name, uri, before, ctx_.now_ms};
        });
    }

    size_t count(const std::string& uri) const {
        return static_cast<size_t>(std::count_if(queue_.items.begin(), queue_.items.end(),
                                                 [&](const core::QueueItem& qi) { return qi.media && qi.media->uri == uri; }));
    }

    // "Added …" with Undo when the new item is in the queue; without it if it never showed up.
    void settle_undo(bool give_up) {
        if (!undo_) return;
        std::string id;
        if (count(undo_->uri) > undo_->before)
            for (const auto& qi : queue_.items)
                if (qi.media && qi.media->uri == undo_->uri) id = qi.queue_item_id;  // last match = newest
        if (id.empty() && !give_up) return;
        Toast t{i18n::tr(i18n::Str::toast_added_to_queue, undo_->name), std::nullopt, {}, false};
        if (!id.empty()) {
            t.action = Command{cmd::RemoveFromQueue{id}};
            t.action_label = i18n::tr(i18n::Str::toast_undo);
        }
        undo_.reset();
        ctx_.publish(std::move(t));
    }

public:
    void tick() override {
        if (refresh_at_ && static_cast<int32_t>(ctx_.now_ms - refresh_at_) >= 0) {
            refresh_at_ = 0;
            refresh();
        }
        if (undo_ && ctx_.now_ms - undo_->at_ms > kUndoWaitMs) settle_undo(true);
    }

private:

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
        auto items = std::make_shared<std::vector<core::QueueItem>>();
        fetch_page(ctx_.queue_id, items, ++generation_);
    }

    // Page by page until a short page; published once complete, so the UI never sees half a queue.
    void fetch_page(const std::string& q, std::shared_ptr<std::vector<core::QueueItem>> items, uint32_t gen) {
        int offset = static_cast<int>(items->size());
        ctx_.send(ma::cmd::queue_items(q, kPage, offset), [this, q, items, gen](backend::Response&& r) {
            if (q != ctx_.queue_id || gen != generation_) return;  // queue changed, or a newer refresh runs
            int got = 0;
            const cJSON* it;
            cJSON_ArrayForEach(it, r.result.get()) {
                items->push_back(ma::parse::queue_item(it));
                ++got;
            }
            if (got == kPage && items->size() < static_cast<size_t>(kMaxItems)) return fetch_page(q, items, gen);
            queue_.queue_id = q;
            queue_.items = std::move(*items);
            publish();
            settle_undo(false);
        });
    }

    void publish() { ctx_.publish(std::make_shared<const core::QueueState>(queue_)); }
    std::string group_;  // grouping of our player at the last resolve

    struct Undo {
        std::string name, uri;
        size_t before;   // copies of uri in the queue before adding
        uint32_t at_ms;
    };
    std::optional<Undo> undo_;
    uint32_t refresh_at_ = 0;  // debounced refresh due (0 = none)
    uint32_t generation_ = 0;  // newest refresh; older pages are dropped

    Context& ctx_;
    core::QueueState queue_;
    core::RepeatGuard guard_;
};

}  // namespace

std::unique_ptr<Feature> make_queue(Context& ctx) { return std::make_unique<Queue>(ctx); }

}  // namespace app
