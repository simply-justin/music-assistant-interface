// Unified search: wires core::SearchSession to the backend.
#include <algorithm>

#include "backend/ma_commands.hpp"
#include "core/search.hpp"
#include "core/text.hpp"
#include "features/features.hpp"

namespace app {
namespace {

constexpr int kLimitPerType = 25;
constexpr size_t kMaxRecent = 8;

class Search : public Feature {
public:
    explicit Search(Context& ctx) : ctx_(ctx), session_(ctx.prefs) {
        std::string saved = ctx_.settings.get("recent_searches");  // newline-separated, newest first
        for (size_t at = 0; at < saved.size();) {
            size_t end = saved.find('\n', at);
            if (end == std::string::npos) end = saved.size();
            if (end > at) recent_.push_back(saved.substr(at, end - at));
            at = end + 1;
        }
        ctx_.publish(RecentSearches{recent_});
    }

    void on_command(const Command& c) override {
        if (auto* s = std::get_if<cmd::Search>(&c)) {
            if (session_.set_query(s->text, ctx_.now_ms)) publish();
        } else if (auto* r = std::get_if<cmd::SaveRecentSearch>(&c)) {
            remember(r->text);
        } else if (std::holds_alternative<cmd::SetSourceOrder>(c)) {
            session_.set_prefs(ctx_.prefs);  // the panel feature already updated ctx_.prefs
        }
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
    void remember(std::string text) {
        while (!text.empty() && text.back() == ' ') text.pop_back();
        while (!text.empty() && text.front() == ' ') text.erase(0, 1);
        for (auto& ch : text)
            if (ch == '\n') ch = ' ';
        if (text.empty()) return;
        std::string key = core::normalize(text);
        recent_.erase(std::remove_if(recent_.begin(), recent_.end(), [&](const std::string& q) { return core::normalize(q) == key; }),
                      recent_.end());
        recent_.insert(recent_.begin(), text);
        if (recent_.size() > kMaxRecent) recent_.resize(kMaxRecent);
        std::string joined;
        for (const auto& q : recent_) joined += (joined.empty() ? "" : "\n") + q;
        ctx_.settings.set("recent_searches", joined);
        ctx_.publish(RecentSearches{recent_});
    }

    void publish() { ctx_.publish(std::make_shared<const core::SearchView>(session_.view())); }

    Context& ctx_;
    core::SearchSession session_;
    std::vector<std::string> recent_;
};

}  // namespace

std::unique_ptr<Feature> make_search(Context& ctx) { return std::make_unique<Search>(ctx); }

}  // namespace app
