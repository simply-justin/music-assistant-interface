#include "backend/ma_session.hpp"

using namespace backend;

namespace ma {

void Session::set_connection(Connection c) {
    connection_ = c;
    if (c == Connection::Connected) {
        auto waiting = std::move(waiting_);
        waiting_.clear();
        for (auto& [req, done] : waiting) send_now(std::move(req), std::move(done));
    } else if (c != Connection::Connecting) {
        fail_waiting();
    }
    if (listener_.on_connection) listener_.on_connection(c, info_);
}

void Session::fail_waiting() {
    auto waiting = std::move(waiting_);
    waiting_.clear();
    for (auto& [req, done] : waiting) {
        Response r;
        r.error_code = "disconnected";
        if (done) done(std::move(r));
    }
}

void Session::request(Request req, Backend::Done done) {
    // The server rejects everything but `auth` until authenticated (verified 2.10).
    if (connection_ == Connection::Connecting && req.command != "auth") {
        waiting_.emplace_back(std::move(req), std::move(done));
        return;
    }
    if (connection_ != Connection::Connected && req.command != "auth") {
        Response r;
        r.error_code = "disconnected";
        if (done) done(std::move(r));
        return;
    }
    send_now(std::move(req), std::move(done));
}

void Session::send_now(Request req, Backend::Done done) {
    std::string id = std::to_string(next_id_++);
    Json msg(cJSON_CreateObject());
    cJSON_AddStringToObject(msg.get(), "message_id", id.c_str());
    cJSON_AddStringToObject(msg.get(), "command", req.command.c_str());
    if (req.args) cJSON_AddItemToObject(msg.get(), "args", req.args.release());
    char* text = cJSON_PrintUnformatted(msg.get());
    bool sent = text && send_(text);
    cJSON_free(text);
    if (!sent) {
        Response r;
        r.error_code = "disconnected";
        if (done) done(std::move(r));
        return;
    }
    pending_[id] = Pending{std::move(done), nullptr};
}

void Session::on_open() {
    info_ = {};
    set_connection(Connection::Connecting);  // until server_info (and auth) arrive
}

void Session::on_closed() {
    auto pending = std::move(pending_);
    pending_.clear();
    for (auto& [id, p] : pending) {
        Response r;
        r.error_code = "disconnected";
        if (p.done) p.done(std::move(r));
    }
    set_connection(Connection::Disconnected);
}

void Session::on_text(std::string_view text) {
    Json msg(cJSON_ParseWithLength(text.data(), text.size()));
    if (!cJSON_IsObject(msg.get())) return;  // ignore garbage rather than crash

    const cJSON* id = cJSON_GetObjectItemCaseSensitive(msg.get(), "message_id");
    if (cJSON_IsString(id)) return on_response(id->valuestring, msg.get());
    if (cJSON_IsNumber(id)) return on_response(std::to_string(static_cast<long long>(id->valuedouble)), msg.get());

    std::string event = json_str(msg.get(), "event");
    if (!event.empty()) {
        if (!listener_.on_event) return;
        Event ev{event, json_str(msg.get(), "object_id"), nullptr};
        if (cJSON* data = cJSON_DetachItemFromObjectCaseSensitive(msg.get(), "data")) ev.data.reset(data);
        listener_.on_event(std::move(ev));
        return;
    }
    if (cJSON_GetObjectItemCaseSensitive(msg.get(), "server_version")) on_server_info(msg.get());
}

void Session::on_server_info(const cJSON* msg) {
    info_.server_id = json_str(msg, "server_id");
    info_.server_version = json_str(msg, "server_version");
    info_.schema_version = static_cast<int>(json_num(msg, "schema_version"));
    if (info_.schema_version < kMinSchemaVersion) return set_connection(Connection::IncompatibleServer);
    if (!expected_server_id_.empty() && info_.server_id != expected_server_id_)
        return set_connection(Connection::WrongServer);
    if (token_.empty()) return set_connection(Connection::AuthRequired);  // auth is mandatory (schema ≥ 28)

    Json args(cJSON_CreateObject());
    cJSON_AddStringToObject(args.get(), "token", token_.c_str());
    request({"auth", std::move(args)}, [this](Response&& r) {
        if (connection_ != Connection::Connecting) return;  // closed meanwhile
        set_connection(r.ok ? Connection::Connected : Connection::AuthFailed);
    });
}

void Session::on_response(const std::string& id, const cJSON* msg) {
    auto it = pending_.find(id);
    if (it == pending_.end()) return;  // late reply to a request we already failed
    Pending& p = it->second;

    if (cJSON_GetObjectItemCaseSensitive(msg, "error_code")) {
        Response r;
        const cJSON* code = cJSON_GetObjectItemCaseSensitive(msg, "error_code");
        r.error_code = cJSON_IsString(code) ? code->valuestring : std::to_string(static_cast<int>(json_num(msg, "error_code")));
        r.details = json_str(msg, "details");
        auto done = std::move(p.done);
        pending_.erase(it);
        if (done) done(std::move(r));
        return;
    }

    cJSON* result = cJSON_DetachItemFromObjectCaseSensitive(const_cast<cJSON*>(msg), "result");
    if (json_bool(msg, "partial")) {
        // Large lists arrive in chunks: join arrays until the final message.
        if (!p.partial) p.partial.reset(cJSON_CreateArray());
        if (cJSON_IsArray(result)) {
            while (cJSON* item = cJSON_DetachItemFromArray(result, 0)) cJSON_AddItemToArray(p.partial.get(), item);
        }
        cJSON_Delete(result);
        return;
    }
    Response r;
    r.ok = true;
    if (p.partial) {
        if (cJSON_IsArray(result))
            while (cJSON* item = cJSON_DetachItemFromArray(result, 0)) cJSON_AddItemToArray(p.partial.get(), item);
        cJSON_Delete(result);
        r.result = std::move(p.partial);
    } else {
        r.result.reset(result);
    }
    auto done = std::move(p.done);
    pending_.erase(it);
    if (done) done(std::move(r));
}

}  // namespace ma
