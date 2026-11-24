#include "pairing/pairing.hpp"

#include <cctype>

namespace pairing {

namespace {

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// URL-decodes one form value; nullopt on a broken %-escape.
std::optional<std::string> decode(std::string_view v) {
    std::string out;
    out.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '+') {
            out += ' ';
        } else if (v[i] == '%') {
            if (i + 2 >= v.size()) return std::nullopt;
            int a = hex(v[i + 1]), b = hex(v[i + 2]);
            if (a < 0 || b < 0) return std::nullopt;
            out += static_cast<char>(a * 16 + b);
            i += 2;
        } else {
            out += v[i];
        }
    }
    return out;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

struct Text {
    const char *title, *intro, *server, *label, *button, *rejected, *thanks;
};

const Text& text(std::string_view lang) {
    static const Text en{"Sign in the music panel",
                         "In Music Assistant, open your profile, create a long-lived token and paste it here.",
                         "The panel signs in to the Music Assistant at",
                         "Token", "Send to the panel",
                         "That didn't look like a token. Copy the whole token and try again.",
                         "Sent. Look at the panel: it signs in by itself. You can close this page."};
    static const Text nl{"Het muziekscherm aanmelden",
                         "Open in Music Assistant je profiel, maak een langlevend token aan en plak het hier.",
                         "Het scherm meldt zich aan bij de Music Assistant op",
                         "Token", "Naar het scherm sturen",
                         "Dat leek geen token. Kopieer het hele token en probeer opnieuw.",
                         "Verstuurd. Kijk op het scherm: het meldt zich vanzelf aan. Je kunt deze pagina sluiten."};
    return lang == "nl" ? nl : en;
}

std::string page(std::string_view lang, const std::string& body) {
    return std::string("<!doctype html><html lang=\"") + std::string(lang == "nl" ? "nl" : "en") +
           "\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<title>" + text(lang).title + "</title><style>"
           "body{margin:0;padding:24px;font:17px/1.5 system-ui,sans-serif;background:#0a0a0a;color:#fff}"
           "main{max-width:520px;margin:0 auto}h1{font-size:26px;margin:8px 0 12px}p{color:#a3a3a3}"
           "label{display:block;margin:20px 0 8px;font-weight:600}"
           "textarea{box-sizing:border-box;width:100%;min-height:140px;padding:12px;font:15px monospace;"
           "background:#161616;color:#fff;border:2px solid #8f8f8f;border-radius:8px}"
           "button{margin-top:16px;width:100%;padding:14px;font:600 17px system-ui;border:0;border-radius:8px;"
           "background:#fff;color:#000}.err{color:#f08c7e}"
           "@media(prefers-color-scheme:light){body{background:#f6f6f7;color:#111}p{color:#52525b}"
           "textarea{background:#fff;color:#111;border-color:#63636b}button{background:#111;color:#fff}.err{color:#b42318}}"
           "</style></head><body><main>" + body + "</main></body></html>";
}

}  // namespace

std::string new_code(uint32_t (*random32)()) {
    static const char kAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no 0/o, 1/l/i
    std::string code;
    for (int i = 0; i < 6; ++i) code += kAlphabet[random32() % (sizeof kAlphabet - 1)];
    return code;
}

// Escapes text for HTML (the server address is typed during setup).
std::string escape(std::string_view s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string form_page(std::string_view lang, std::string_view server, bool rejected) {
    const Text& t = text(lang);
    std::string body = std::string("<h1>") + t.title + "</h1><p>" + t.intro + "</p>";
    if (!server.empty()) body += std::string("<p>") + t.server + " <strong>" + escape(server) + "</strong>.</p>";
    if (rejected) body += std::string("<p class=\"err\" role=\"alert\">") + t.rejected + "</p>";
    body += std::string("<form method=\"post\"><label for=\"token\">") + t.label +
            "</label><textarea id=\"token\" name=\"token\" required autocomplete=\"off\" autocapitalize=\"off\" "
            "spellcheck=\"false\"></textarea><button type=\"submit\">" + t.button + "</button></form>";
    return page(lang, body);
}

std::string thanks_page(std::string_view lang) {
    const Text& t = text(lang);
    return page(lang, std::string("<h1>") + t.title + "</h1><p>" + t.thanks + "</p>");
}

std::optional<std::string> token_from_form(std::string_view body) {
    if (body.size() > kMaxToken * 3 + 16) return std::nullopt;  // even fully %-escaped it would be too long
    for (size_t start = 0; start <= body.size();) {
        size_t end = body.find('&', start);
        if (end == std::string_view::npos) end = body.size();
        std::string_view pair = body.substr(start, end - start);
        if (pair.rfind("token=", 0) == 0) {
            auto value = decode(pair.substr(6));
            if (!value) return std::nullopt;
            std::string_view t = trim(*value);
            if (t.empty() || t.size() > kMaxToken) return std::nullopt;
            for (char c : t)
                if (std::isspace(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) < 0x21) return std::nullopt;
            return std::string(t);
        }
        start = end + 1;
    }
    return std::nullopt;
}

}  // namespace pairing
