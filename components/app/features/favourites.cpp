// Favourite / unfavourite any item.
#include "backend/ma_commands.hpp"
#include "features/features.hpp"

namespace app {
namespace {

class Favourites : public Feature {
public:
    explicit Favourites(Context& ctx) : ctx_(ctx) {}

    void on_command(const Command& c) override {
        auto* f = std::get_if<cmd::SetFavourite>(&c);
        if (!f) return;
        if (f->on) {
            ctx_.send(ma::cmd::favorite_add(f->item.uri),
                      [this](backend::Response&&) { ctx_.toast(i18n::tr(i18n::Str::toast_favourite_added)); });
        } else if (f->item.provider == "library") {  // removal needs the library id (verify)
            ctx_.send(ma::cmd::favorite_remove(f->item.type, f->item.item_id),
                      [this](backend::Response&&) { ctx_.toast(i18n::tr(i18n::Str::toast_favourite_removed)); });
        }
    }

private:
    Context& ctx_;
};

}  // namespace

std::unique_ptr<Feature> make_favourites(Context& ctx) { return std::make_unique<Favourites>(ctx); }

}  // namespace app
