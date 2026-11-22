#include "core/search.hpp"

#include "core/text.hpp"

namespace core {

std::optional<MediaItem> pick_top_result(const SearchResult& r, const std::string& query) {
    std::string q = normalize(query);
    for (const auto* list : {&r.artists, &r.albums, &r.tracks, &r.playlists, &r.radio})
        for (const auto& item : *list)
            if (normalize(item.name) == q) return item;
    if (!r.tracks.empty()) return r.tracks.front();
    return std::nullopt;
}

bool SearchSession::set_query(const std::string& text, uint32_t now_ms) {
    std::string key = normalize(text);
    if (key.empty()) {
        ++generation_;  // any response still in flight is now stale
        pending_ = false;
        bool changed = view_.status != SearchStatus::Idle || !view_.query.empty();
        view_ = SearchView{};
        return changed;
    }
    view_.query = text;
    pending_ = true;
    due_ms_ = now_ms + kDebounceMs;
    return false;
}

SearchSession::Poll SearchSession::poll(uint32_t now_ms) {
    Poll out;
    if (!pending_ || static_cast<int32_t>(now_ms - due_ms_) < 0) return out;

    std::string key = normalize(view_.query);
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->key != key || now_ms - it->at_ms > kCacheTtlMs) continue;
        Cached hit = std::move(*it);
        cache_.erase(it);
        ++generation_;
        pending_ = false;
        view_ = hit.view;
        cache_.push_front(std::move(hit));
        out.view_changed = true;
        return out;
    }

    if (in_flight_ + 2 > kMaxInFlight) return out;  // latest query waits for a free slot

    ++generation_;
    pending_ = false;
    in_flight_ += 2;
    library_done_ = full_done_ = false;
    view_.status = SearchStatus::Loading;
    view_.result = {};
    view_.failed_providers.clear();
    out.requests.push_back({generation_, SearchStage::Library, view_.query});
    out.requests.push_back({generation_, SearchStage::Full, view_.query});
    out.view_changed = true;
    return out;
}

bool SearchSession::on_result(uint32_t generation, SearchStage stage, SearchResult result,
                              std::vector<std::string> failed_providers, uint32_t now_ms) {
    if (in_flight_ > 0) --in_flight_;
    if (generation != generation_) return false;

    auto& r = view_.result;
    merge_into(r.tracks, std::move(result.tracks), prefs_);
    merge_into(r.artists, std::move(result.artists), prefs_);
    merge_into(r.albums, std::move(result.albums), prefs_);
    merge_into(r.playlists, std::move(result.playlists), prefs_);
    merge_into(r.radio, std::move(result.radio), prefs_);
    r.top = pick_top_result(r, view_.query);
    for (auto& p : failed_providers) view_.failed_providers.push_back(std::move(p));

    (stage == SearchStage::Library ? library_done_ : full_done_) = true;
    view_.status = full_done_ ? SearchStatus::Done : SearchStatus::Partial;
    // Cache only complete, clean results.
    if (full_done_ && library_done_ && view_.failed_providers.empty()) {
        std::string key = normalize(view_.query);
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
            if (it->key == key) { cache_.erase(it); break; }
        cache_.push_front({key, view_, now_ms});
        if (cache_.size() > kCacheSize) cache_.pop_back();
    }
    return true;
}

bool SearchSession::on_error(uint32_t generation, SearchStage stage) {
    if (in_flight_ > 0) --in_flight_;
    if (generation != generation_) return false;
    (stage == SearchStage::Library ? library_done_ : full_done_) = true;
    if (stage == SearchStage::Full)
        view_.status = view_.result.empty() ? SearchStatus::Error : SearchStatus::Done;
    return true;
}

}  // namespace core
