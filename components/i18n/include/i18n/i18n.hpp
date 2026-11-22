#pragma once
// UI strings. Usage: tr(Str::toast_favourite_added), tr(Str::toast_added_to_queue, name).
// Media titles are never translated.
#include <string>
#include <string_view>

#include "strings.gen.hpp"

namespace i18n {

void set_language(Lang lang);
Lang language();
bool language_from_code(std::string_view code, Lang& out);  // "en", "nl", ...

const char* tr(Str id);
std::string tr(Str id, std::string_view arg);  // replaces the first "{}"

}  // namespace i18n
