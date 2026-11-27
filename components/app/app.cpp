#include "app/app.hpp"

#include <cstdio>

#include <sstream>

namespace app {

void Context::send(backend::Request req, std::function<void(backend::Response&&)> on_ok) {
    std::string command = req.command;
    std::string args;  // for the log: which item the server refused to play
    if (command == "player_queues/play_media" && req.args) {
        char* text = cJSON_PrintUnformatted(req.args.get());
        if (text) args = text;
        cJSON_free(text);
    }
    backend.request(std::move(req), [this, command, args, on_ok = std::move(on_ok)](backend::Response&& r) {
        if (!r.ok) {
            // The toast is generic; the log says what the server answered (serial log on the panel).
            std::printf("command %s failed: %s %s %s\n", command.c_str(), r.error_code.c_str(), r.details.c_str(),
                        args.c_str());
            if (r.error_code != "disconnected")
                publish(Toast{i18n::tr(i18n::Str::toast_command_failed), std::nullopt, {}, true});
            return;  // disconnects are shown by the connection banner instead
        }
        if (on_ok) on_ok(std::move(r));
    });
}

App::App(backend::Backend& backend, Settings& settings, Platform& platform, std::function<void(Update)> publish)
    : ctx_{backend, settings, platform, std::move(publish), {}, 0, {}, {}, false, {}} {
    ctx_.dispatch = [this](Command c) { handle(c); };
}

void App::start() {
    i18n::Lang lang;
    if (i18n::language_from_code(ctx_.settings.get("lang", "en"), lang)) i18n::set_language(lang);

    std::stringstream order(ctx_.settings.get("source_order", ""));  // "spotify,ytmusic"
    for (std::string p; std::getline(order, p, ',');)
        if (!p.empty()) ctx_.prefs.provider_order.push_back(p);

    features_ = make_features(ctx_);
    ctx_.backend.set_listener({
        [this](backend::Event&& ev) {
            for (auto& f : features_) f->on_event(ev);
        },
        [this](backend::Connection c, const backend::ServerInfo& info) {
            // Remember the first server we trusted; ma::Session refuses a different one later.
            if (c == backend::Connection::Connected && !info.server_id.empty() && ctx_.settings.get("server_id").empty())
                ctx_.settings.set("server_id", info.server_id);
            ctx_.publish(ConnectionView{c, info});  // the UI reloads its lists when this says Connected
            for (auto& f : features_) f->on_connection(c);
            if (c == backend::Connection::Connected)
                for (auto& f : features_) f->on_connected();
        },
    });
    // Saved server (empty until first-time setup has run).
    std::string url = ctx_.settings.get("server_url");
    if (!url.empty()) ctx_.backend.configure(url, ctx_.settings.get("token"));
    ctx_.backend.start();
}

void App::handle(const Command& command) {
    for (auto& f : features_) f->on_command(command);
}

void App::tick(uint32_t now_ms) {
    ctx_.now_ms = now_ms;
    ctx_.backend.tick(now_ms);
    for (auto& f : features_) f->tick();
}

}  // namespace app
