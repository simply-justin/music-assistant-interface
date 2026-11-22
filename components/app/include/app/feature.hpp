#pragma once
// A feature = one self-contained module (search, queue, players, ...). It reacts to commands,
// server events and connection, and publishes views. Register it in features.cpp (one line).
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/commands.hpp"
#include "app/settings.hpp"
#include "app/updates.hpp"
#include "backend/backend.hpp"
#include "core/dedupe.hpp"

namespace app {

// What a feature may use. All of it lives on the core task.
struct Context {
    backend::Backend& backend;
    Settings& settings;
    std::function<void(Update)> publish;     // to the UI
    std::function<void(Command)> dispatch;   // to every feature
    uint32_t now_ms = 0;
    std::string player_id;  // selected player
    std::string queue_id;   // its active queue: its own, or its group's when grouped
    core::SourcePrefs prefs;

    // Sends a request; on failure shows a generic "didn't work" toast.
    void send(backend::Request req, std::function<void(backend::Response&&)> on_ok = {});
    void toast(std::string text) { publish(Toast{std::move(text), std::nullopt, {}}); }
};

class Feature {
public:
    virtual ~Feature() = default;
    virtual void on_connected() {}                         // after (re)connect: full re-sync
    virtual void on_command(const Command&) {}
    virtual void on_event(const backend::Event&) {}
    virtual void tick() {}                                 // every core loop, ctx.now_ms is fresh
};

// The registry (features.cpp).
std::vector<std::unique_ptr<Feature>> make_features(Context& ctx);

}  // namespace app
