#pragma once
// The pairing page: during setup the panel serves a tiny web page where a phone pastes the Music
// Assistant token, so nobody types a 200-character token on a wall screen. Portable (no ESP-IDF):
// the board wraps it in esp_http_server, the host tests call it directly.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pairing {

// A fresh secret for the page's path, e.g. "k7p2xq": the URL in the QR code is
// http://<panel>/<code>, so other devices on the network cannot post a token by guessing "/".
// `random32` supplies randomness (esp_random on the board).
std::string new_code(uint32_t (*random32)());

// The form (GET) and the reply after a post. `lang` is "en" or "nl". `server` (host:port) is
// named on the page so the user can check the token goes to their own Music Assistant.
std::string form_page(std::string_view lang, std::string_view server, bool rejected = false);
std::string thanks_page(std::string_view lang);

// The token from an application/x-www-form-urlencoded body ("token=..."), trimmed. Empty or
// malformed input, or anything longer than kMaxToken, gives nullopt.
constexpr size_t kMaxToken = 4096;
std::optional<std::string> token_from_form(std::string_view body);

}  // namespace pairing
