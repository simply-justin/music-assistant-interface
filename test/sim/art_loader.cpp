#include "art_loader.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "src/libs/tjpgd/tjpgd.h"

namespace {

// Minimal HTTP/1.1 GET (http:// only, Connection: close). Returns the body or "" on failure.
std::string http_get(const std::string& url) {
    if (url.rfind("http://", 0) != 0) return {};
    std::string rest = url.substr(7);
    std::string hostport = rest.substr(0, rest.find('/'));
    std::string path = rest.substr(hostport.size());
    std::string host = hostport.substr(0, hostport.find(':'));
    std::string port = hostport.find(':') == std::string::npos ? "80" : hostport.substr(hostport.find(':') + 1);
    addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) return {};
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    timeval tv{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    bool ok = fd >= 0 && connect(fd, res->ai_addr, res->ai_addrlen) == 0;
    freeaddrinfo(res);
    if (!ok) {
        if (fd >= 0) close(fd);
        return {};
    }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + hostport + "\r\nConnection: close\r\n\r\n";
    send(fd, req.data(), req.size(), MSG_NOSIGNAL);
    std::string data;
    char buf[16384];
    for (ssize_t n; (n = recv(fd, buf, sizeof buf, 0)) > 0;) data.append(buf, static_cast<size_t>(n));
    close(fd);
    size_t head_end = data.find("\r\n\r\n");
    if (data.rfind("HTTP/1.1 200", 0) != 0 || head_end == std::string::npos) return {};
    std::string head = data.substr(0, head_end), body = data.substr(head_end + 4);
    if (head.find("chunked") == std::string::npos) return body;
    std::string out;  // de-chunk
    for (size_t i = 0;;) {
        size_t eol = body.find("\r\n", i);
        if (eol == std::string::npos) break;
        size_t len = std::stoul(body.substr(i, eol - i), nullptr, 16);
        if (len == 0) break;
        out.append(body, eol + 2, len);
        i = eol + 2 + len + 2;
    }
    return out;
}

struct Decode {
    const std::string* jpeg = nullptr;
    size_t pos = 0;
    std::vector<uint8_t> rgb;  // RGB888 (TJpgDec JD_FORMAT 0)
    int w = 0;
};

size_t input(JDEC* jd, uint8_t* buf, size_t n) {
    auto* d = static_cast<Decode*>(jd->device);
    n = std::min(n, d->jpeg->size() - d->pos);
    if (buf) std::memcpy(buf, d->jpeg->data() + d->pos, n);
    d->pos += n;
    return n;
}

int output(JDEC* jd, void* bitmap, JRECT* rect) {
    auto* d = static_cast<Decode*>(jd->device);
    auto* src = static_cast<uint8_t*>(bitmap);
    int rw = rect->right - rect->left + 1;
    for (int y = rect->top; y <= rect->bottom; ++y)
        std::memcpy(&d->rgb[(static_cast<size_t>(y) * d->w + rect->left) * 3], src + static_cast<size_t>(y - rect->top) * rw * 3, rw * 3);
    return 1;
}

std::shared_ptr<img::Bitmap> fetch_and_decode(const ui::ArtRequest& r) {
    std::string jpeg = http_get(r.url);
    if (jpeg.empty()) return nullptr;
    Decode d;
    d.jpeg = &jpeg;
    std::vector<uint8_t> work(4096 * 4);
    JDEC jd;
    if (jd_prepare(&jd, input, work.data(), work.size(), &d) != JDR_OK) return nullptr;  // e.g. progressive
    d.w = jd.width;
    d.rgb.resize(static_cast<size_t>(jd.width) * jd.height * 3);
    if (jd_decomp(&jd, output, 0) != JDR_OK) return nullptr;
    return img::scale(d.rgb.data(), jd.width, jd.height, r.w, r.h, r.radius, r.bg);
}

}  // namespace

SimArtLoader::SimArtLoader() {
    for (int i = 0; i < 2; ++i) threads_.emplace_back([this] { work(); });
}

SimArtLoader::~SimArtLoader() {
    {
        std::lock_guard<std::mutex> l(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : threads_) t.join();
}

void SimArtLoader::load(ui::ArtRequest req, std::function<void(std::shared_ptr<img::Bitmap>)> done) {
    {
        std::lock_guard<std::mutex> l(mu_);
        todo_.push_back({std::move(req), std::move(done), nullptr});
    }
    cv_.notify_one();
}

void SimArtLoader::work() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> l(mu_);
            cv_.wait(l, [this] { return stop_ || !todo_.empty(); });
            if (stop_) return;
            job = std::move(todo_.front());
            todo_.pop_front();
        }
        job.result = fetch_and_decode(job.req);
        std::lock_guard<std::mutex> l(mu_);
        finished_.push_back(std::move(job));
    }
}

void SimArtLoader::drain() {
    std::deque<Job> done;
    {
        std::lock_guard<std::mutex> l(mu_);
        done.swap(finished_);
    }
    for (auto& j : done) j.done(std::move(j.result));
}
