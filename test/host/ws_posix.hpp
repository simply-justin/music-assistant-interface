#pragma once
// Minimal blocking-connect, non-blocking-read WebSocket client for the PC test app (ws:// only).
// The device uses esp_websocket_client instead; this exists so the same core runs on a PC.
#include <string>
#include <vector>

class WsPosix {
public:
    ~WsPosix() { close(); }
    bool connect(const std::string& url);  // "ws://host:port/path"
    bool send_text(const std::string& text);
    // Reads what is available (waits up to timeout_ms). Complete text messages go to `out`.
    // Returns false when the connection is gone.
    bool poll(int timeout_ms, std::vector<std::string>& out);
    void close();
    bool connected() const { return fd_ >= 0; }

private:
    bool send_frame(int opcode, const std::string& payload);
    int fd_ = -1;
    std::string buf_, message_;
};
