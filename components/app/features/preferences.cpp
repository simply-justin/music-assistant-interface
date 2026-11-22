// Panel preferences that are not tied to the server: language for now.
#include "features/features.hpp"

namespace app {
namespace {

class Preferences : public Feature {
public:
    explicit Preferences(Context& ctx) : ctx_(ctx) {}

    void on_command(const Command& c) override {
        if (auto* l = std::get_if<cmd::SetLanguage>(&c)) {
            i18n::set_language(l->lang);
            ctx_.settings.set("lang", i18n::kLangCodes[static_cast<int>(l->lang)]);
            ctx_.publish(LanguageChanged{l->lang});  // UI rebuilds the current screen
        }
    }

private:
    Context& ctx_;
};

}  // namespace

std::unique_ptr<Feature> make_preferences(Context& ctx) { return std::make_unique<Preferences>(ctx); }

}  // namespace app
