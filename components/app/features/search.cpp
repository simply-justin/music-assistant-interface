// Unified search: wires core::SearchSession to the backend.
#include "backend/ma_commands.hpp"
#include "core/search.hpp"
#include "features/features.hpp"

namespace app {
namespace {

constexpr int kLimitPerType = 25;

class Search : public Feature {
public:
    explicit Search(Context& ctx) : ctx_(ctx), session_(ctx.prefs) {}

    void on_command(const Command& c) override {
        if (auto* s = std::get_if<cmd::Search>(&c))
            if (session_.set_query(s->text, ctx_.now_ms)) publish();
    }

    void tick() override {
        auto poll = session_.poll(ctx_.now_ms);
        for (const auto& req : poll.requests) {
            auto gen = req.generation;
            auto stage = req.stage;
            // Not ctx_.send: a failed search shows in the results, not as a toast.
            ctx_.backend.request(ma::cmd::search(req.query, stage, kLimitPerType), [this, gen, stage](backend::Response&& r) {
                bool changed = r.ok ? session_.on_result(gen, stage, ma::parse::search_result(r.result.get()), {}, ctx_.now_ms)
                                    : session_.on_error(gen, stage);
                if (changed) publish();
            });
        }
        if (poll.view_changed) publish();
    }

private:
    void publish() { ctx_.publish(std::make_shared<const core::SearchView>(session_.view())); }

    Context& ctx_;
    core::SearchSession session_;
};

}  // namespace

std::unique_ptr<Feature> make_search(Context& ctx) { return std::make_unique<Search>(ctx); }

}  // namespace app
