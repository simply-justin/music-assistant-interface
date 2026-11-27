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
            if (guard_.allow("play|" + p->item.uri + (p->context ? "|" + p->context->uri : "") + (p->radio ? "|radio" : ""), ctx_.now_ms)) {
                // On a paused queue MA resumes the paused song for a moment before switching:
                // stop it first. Playing already: no extra round trip.
                if (ctx_.player_paused) ctx_.send(ma::cmd::stop(q));
                // A song tapped inside an album or playlist plays that list from the song on.
                if (p->context && !p->context->uri.empty())
                    ctx_.send(ma::cmd::play_media(q, {p->context->uri}, ma::cmd::PlayOption::Play, p->item.uri, p->radio));
                else
                    ctx_.send(ma::cmd::play_media(q, {p->item.uri}, ma::cmd::PlayOption::Play, "", p->radio));
            }
        } else if (auto* t = std::get_if<cmd::SetSleepTimer>(&c)) {
            sleep_at_ = t->minutes > 0 ? ctx_.now_ms + static_cast<uint32_t>(t->minutes) * 60'000u : 0;
            sleep_q_ = q;
            if (t->minutes > 0) ctx_.toast(i18n::tr(i18n::Str::toast_sleep_set, std::to_string(t->minutes)));
            else ctx_.toast(i18n::tr(i18n::Str::toast_sleep_off));
            publish_sleep();
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

    void tick() override {
        if (!sleep_at_) return;
        if (static_cast<int32_t>(ctx_.now_ms - sleep_at_) >= 0) {
            sleep_at_ = 0;
            ctx_.send(ma::cmd::pause(sleep_q_));  // the queue it was set on, even if the speaker changed since
            publish_sleep();
        } else if (minutes_left() != shown_) {
            publish_sleep();
        }
    }

private:
    int minutes_left() const {
        return sleep_at_ ? static_cast<int>((sleep_at_ - ctx_.now_ms + 59'999u) / 60'000u) : 0;
    }
    void publish_sleep() {
        shown_ = minutes_left();
        ctx_.publish(SleepTimerView{shown_});
    }

    uint32_t sleep_at_ = 0;  // ctx_.now_ms when the music pauses; 0 = off
    std::string sleep_q_;
    int shown_ = 0;
    Context& ctx_;
    core::RepeatGuard guard_;
};

}  // namespace

std::unique_ptr<Feature> make_playback(Context& ctx) { return std::make_unique<Playback>(ctx); }

}  // namespace app
