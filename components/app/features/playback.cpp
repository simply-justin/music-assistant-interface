// Transport controls on the selected player's queue.
#include "backend/ma_commands.hpp"
#include "core/repeat_guard.hpp"
#include "features/features.hpp"

namespace app {
namespace {

class Playback : public Feature {
public:
    explicit Playback(Context& ctx) : ctx_(ctx) {}

    void on_command(const Command& c) override {
        const std::string& q = ctx_.queue_id;
        if (q.empty()) return;
        if (auto* p = std::get_if<cmd::Play>(&c)) {
            // Only the shown source is played, never every merged duplicate.
            if (guard_.allow("play|" + p->item.uri, ctx_.now_ms)) {
                // On a paused queue MA resumes the paused song for a moment before switching:
                // stop it first. Playing already: no extra round trip.
                if (ctx_.player_paused) ctx_.send(ma::cmd::stop(q));
                ctx_.send(ma::cmd::play_media(q, {p->item.uri}, ma::cmd::PlayOption::Play));
            }
        } else if (std::holds_alternative<cmd::PlayPause>(c)) {
            if (guard_.allow("play_pause", ctx_.now_ms)) ctx_.send(ma::cmd::play_pause(q));
        } else if (std::holds_alternative<cmd::Next>(c)) {
            ctx_.send(ma::cmd::next(q));  // not guarded: tapping twice to skip two songs is intended
        } else if (std::holds_alternative<cmd::Previous>(c)) {
            ctx_.send(ma::cmd::previous(q));
        } else if (auto* s = std::get_if<cmd::Seek>(&c)) {
            ctx_.send(ma::cmd::seek(q, s->position_s));
        } else if (auto* s = std::get_if<cmd::SetShuffle>(&c)) {
            ctx_.send(ma::cmd::shuffle(q, s->on));
        } else if (auto* r = std::get_if<cmd::SetRepeat>(&c)) {
            ctx_.send(ma::cmd::repeat(q, r->mode));
        }
    }

private:
    Context& ctx_;
    core::RepeatGuard guard_;
};

}  // namespace

std::unique_ptr<Feature> make_playback(Context& ctx) { return std::make_unique<Playback>(ctx); }

}  // namespace app
