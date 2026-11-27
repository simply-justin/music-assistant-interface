// Speakers: the list, the selected one (restored from settings) and volume.
#include <algorithm>

#include "backend/ma_commands.hpp"
#include "features/features.hpp"

namespace app {
namespace {

class Players : public Feature {
public:
    explicit Players(Context& ctx) : ctx_(ctx) {}

    void on_connected() override {
        ctx_.send(ma::cmd::players_all(), [this](backend::Response&& r) {
            view_.players.clear();
            const cJSON* p;
            cJSON_ArrayForEach(p, r.result.get()) view_.players.push_back(ma::parse::player(p));
            select(pick_initial());
        });
    }

    void on_event(const backend::Event& ev) override {
        if (ev.name == "player_removed") {
            auto gone = find(ev.object_id);
            if (gone == view_.players.end()) return;
            view_.players.erase(gone);
            if (ev.object_id == view_.selected_id) return select(pick_initial());
            return publish();
        }
        if ((ev.name != "player_updated" && ev.name != "player_added") || !ev.data) return;
        core::PlayerState updated = ma::parse::player(ev.data.get());
        auto it = find(updated.player_id);
        if (it == view_.players.end()) view_.players.push_back(std::move(updated));
        else *it = std::move(updated);
        publish();
    }

    void on_command(const Command& c) override {
        if (auto* s = std::get_if<cmd::SelectPlayer>(&c)) {
            // Switching away from a speaker that plays: offer to take the music along.
            auto from = find(view_.selected_id), to = find(s->player_id);
            if (from != view_.players.end() && to != view_.players.end() && from != to &&
                from->state == core::PlaybackState::Playing) {
                ctx_.publish(Toast{i18n::tr(i18n::Str::toast_move_music, to->name),
                                   Command{cmd::TransferQueue{from->player_id, to->player_id}},
                                   i18n::tr(i18n::Str::toast_move)});
            }
            ctx_.settings.set("player", s->player_id);
            select(s->player_id);
        } else if (auto* g = std::get_if<cmd::SetGroup>(&c)) {
            // Send only the difference with the current group.
            auto it = find(g->leader);
            std::vector<std::string> now = it != view_.players.end() ? it->group_members : std::vector<std::string>{};
            auto has = [](const std::vector<std::string>& v, const std::string& id) { return std::find(v.begin(), v.end(), id) != v.end(); };
            std::vector<std::string> add, remove;
            for (const auto& id : g->members)
                if (id != g->leader && !has(now, id)) add.push_back(id);
            for (const auto& id : now)
                if (id != g->leader && !has(g->members, id)) remove.push_back(id);
            if (!add.empty() || !remove.empty()) ctx_.send(ma::cmd::set_members(g->leader, add, remove));
        } else if (auto* u = std::get_if<cmd::Ungroup>(&c)) {
            ctx_.send(ma::cmd::ungroup(u->player_id));
        } else if (auto* v = std::get_if<cmd::SetVolume>(&c)) {
            int level = std::clamp(v->level, 0, 100);
            auto it = find(v->player_id);
            if (it != view_.players.end()) (v->whole_group ? it->group_volume : it->volume_level) = level;  // optimistic
            ctx_.send(v->whole_group ? ma::cmd::group_volume(v->player_id, level) : ma::cmd::volume_set(v->player_id, level));
            publish();
        }
    }

private:
    std::vector<core::PlayerState>::iterator find(const std::string& id) {
        return std::find_if(view_.players.begin(), view_.players.end(), [&](const auto& p) { return p.player_id == id; });
    }

    std::string pick_initial() {
        std::string saved = ctx_.settings.get("player");
        auto it = find(saved);
        if (it != view_.players.end()) return saved;
        for (const auto& p : view_.players)
            if (p.available) return p.player_id;
        return {};
    }

    void select(const std::string& id) {
        view_.selected_id = id;
        publish();
        if (id != ctx_.player_id) {
            ctx_.player_id = id;
            ctx_.dispatch(cmd::PlayerChanged{id});
        }
    }

    void publish() {
        auto it = find(view_.selected_id);
        ctx_.player_paused = it != view_.players.end() && it->state == core::PlaybackState::Paused;
        ctx_.publish(std::make_shared<const PlayersView>(view_));
    }

    Context& ctx_;
    PlayersView view_;
};

}  // namespace

std::unique_ptr<Feature> make_players(Context& ctx) { return std::make_unique<Players>(ctx); }

}  // namespace app
