#pragma once
// Unified search state machine: debounce, generation stamps, library-first then full search,
// in-flight cap, recent-query cache. Pure logic: time is passed in, requests are returned.
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "core/dedupe.hpp"
#include "core/model.hpp"

namespace core {

enum class SearchStage : uint8_t { Library, Full };

struct SearchRequest {
    uint32_t generation;
    SearchStage stage;
    std::string query;
};

enum class SearchStatus : uint8_t { Idle, Loading, Partial, Done, Error };

struct SearchView {
    std::string query;
    SearchStatus status = SearchStatus::Idle;
    SearchResult result;
    std::vector<std::string> failed_providers;  // "partial results" notice
};

// Top result: an exact name match (artists, albums, tracks, playlists, radio), else the first track.
std::optional<MediaItem> pick_top_result(const SearchResult& r, const std::string& query);

class SearchSession {
public:
    static constexpr uint32_t kDebounceMs = 250;
    static constexpr int kMaxInFlight = 4;  // two queries' worth of library + full requests
    static constexpr size_t kCacheSize = 8;
    static constexpr uint32_t kCacheTtlMs = 60'000;

    explicit SearchSession(SourcePrefs prefs = {}) : prefs_(std::move(prefs)) {}

    // Called on every keystroke. Returns true when the view changed (cleared).
    bool set_query(const std::string& text, uint32_t now_ms);

    struct Poll {
        std::vector<SearchRequest> requests;  // send these now
        bool view_changed = false;
    };
    Poll poll(uint32_t now_ms);

    // Each returns true when the view changed. Stale generations are dropped.
    bool on_result(uint32_t generation, SearchStage stage, SearchResult result,
                   std::vector<std::string> failed_providers, uint32_t now_ms);
    bool on_error(uint32_t generation, SearchStage stage);

    const SearchView& view() const { return view_; }
    void set_prefs(SourcePrefs prefs) { prefs_ = std::move(prefs); }

private:
    struct Cached {
        std::string key;
        SearchView view;
        uint32_t at_ms;
    };

    SourcePrefs prefs_;
    SearchView view_;
    uint32_t generation_ = 0;
    int in_flight_ = 0;
    bool pending_ = false;
    uint32_t due_ms_ = 0;
    bool library_done_ = false, full_done_ = false;
    std::deque<Cached> cache_;  // front = most recent
};

}  // namespace core
