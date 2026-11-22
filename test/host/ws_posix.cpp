#include "ws_posix.hpp"

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <random>

namespace {

std::string base64(const unsigned char* d, size_t n) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        o += t[v >> 18 & 63];
        o += t[v >> 12 & 63];
        o += i + 1 < n ? t[v >> 6 & 63] : '=';
        o += i + 2 < n ? t[v & 63] : '=';
    }
    return o;
}

bool write_all(int fd, const std::string& s) {
    size_t sent = 0;
    while (sent < s.size()) {
        ssize_t n = ::send(fd, s.data() + sent, s.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}

}  // namespace

bool WsPosix::connect(const std::string& url) {
    close();
    if (url.rfind("ws://", 0) != 0) return false;
    std::string rest = url.substr(5);
    std::string hostport = rest.substr(0, rest.find('/'));
    std::string path = rest.find('/') == std::string::npos ? "/" : rest.substr(rest.find('/'));
    std::string host = hostport.substr(0, hostport.find(':'));
    std::string port = hostport.find(':') == std::string::npos ? "80" : hostport.substr(hostport.find(':') + 1);

    addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) return false;
    for (addrinfo* a = res; a; a = a->ai_next) {
        fd_ = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd_ >= 0 && ::connect(fd_, a->ai_addr, a->ai_addrlen) == 0) break;
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }
    freeaddrinfo(res);
    if (fd_ < 0) return false;

    unsigned char key[16];
    for (auto& k : key) k = static_cast<unsigned char>(rng()());
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + hostport +
                      "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + base64(key, 16) +
                      "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    if (!write_all(fd_, req)) return close(), false;

    std::string head;
    char c;
    while (head.find("\r\n\r\n") == std::string::npos) {
        if (::recv(fd_, &c, 1, 0) != 1) return close(), false;
        head += c;
    }
    if (head.rfind("HTTP/1.1 101", 0) != 0) return close(), false;
    buf_.clear();
    message_.clear();
    return true;
}

bool WsPosix::send_frame(int opcode, const std::string& payload) {
    if (fd_ < 0) return false;
    std::string f;
    f += static_cast<char>(0x80 | opcode);
    size_t n = payload.size();
    if (n < 126) {
        f += static_cast<char>(0x80 | n);
    } else if (n < 65536) {
        f += static_cast<char>(0x80 | 126);
        f += static_cast<char>(n >> 8);
        f += static_cast<char>(n & 0xff);
    } else {
        f += static_cast<char>(0x80 | 127);
        for (int i = 7; i >= 0; --i) f += static_cast<char>((static_cast<uint64_t>(n) >> (8 * i)) & 0xff);
    }
    unsigned char mask[4];
    for (auto& m : mask) m = static_cast<unsigned char>(rng()());
    f.append(reinterpret_cast<char*>(mask), 4);
    for (size_t i = 0; i < n; ++i) f += static_cast<char>(payload[i] ^ mask[i % 4]);
    if (!write_all(fd_, f)) return close(), false;
    return true;
}

bool WsPosix::send_text(const std::string& text) { return send_frame(0x1, text); }

bool WsPosix::poll(int timeout_ms, std::vector<std::string>& out) {
    if (fd_ < 0) return false;
    pollfd p{fd_, POLLIN, 0};
    if (::poll(&p, 1, timeout_ms) > 0) {
        char tmp[16384];
        ssize_t n = ::recv(fd_, tmp, sizeof tmp, 0);
        if (n <= 0) return close(), false;
        buf_.append(tmp, static_cast<size_t>(n));
    }
    // Parse every complete frame in the buffer (server frames are not masked).
    for (;;) {
        if (buf_.size() < 2) break;
        auto b = reinterpret_cast<const unsigned char*>(buf_.data());
        bool fin = b[0] & 0x80;
        int opcode = b[0] & 0x0f;
        uint64_t len = b[1] & 0x7f;
        size_t hdr = 2;
        if (len == 126) {
            if (buf_.size() < 4) break;
            len = static_cast<uint64_t>(b[2]) << 8 | b[3];
            hdr = 4;
        } else if (len == 127) {
            if (buf_.size() < 10) break;
            len = 0;
            for (int i = 0; i < 8; ++i) len = len << 8 | b[2 + i];
            hdr = 10;
        }
        if (buf_.size() < hdr + len) break;
        std::string payload = buf_.substr(hdr, len);
        buf_.erase(0, hdr + len);
        if (opcode == 0x8) return close(), false;
        if (opcode == 0x9) { send_frame(0xA, payload); continue; }
        if (opcode == 0x1 || opcode == 0x0) {
            message_ += payload;
            if (fin) out.push_back(std::move(message_)), message_.clear();
        }
    }
    return true;
}

void WsPosix::close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
}
