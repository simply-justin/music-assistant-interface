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
        auto refresh = [this](backend::Response&&) { ctx_.dispatch(cmd::Browse{cmd::BrowseKind::Favourites, {}}); };
        if (f->on) {
            ctx_.send(ma::cmd::favorite_add(f->item.uri), [this, refresh](backend::Response&& r) {
                ctx_.toast(i18n::tr(i18n::Str::toast_favourite_added));
                refresh(std::move(r));
            });
            return;
        }
        // Removal takes the library id: the item itself, or its library copy merged into it by dedupe.
        std::string id = f->item.provider == "library" ? f->item.item_id : "";
        for (const auto& alt : f->item.alternatives)
            if (id.empty() && alt.provider == "library") id = alt.item_id;
        if (!id.empty()) return remove(f->item.type, id);
        // A provider item (favourited from Search, or just now): ask the server for its library copy.
        core::MediaType type = f->item.type;
        ctx_.send(ma::cmd::get_library_item(f->item), [this, type](backend::Response&& r) {
            std::string library_id = cJSON_IsObject(r.result.get()) ? backend::json_str(r.result.get(), "item_id") : "";
            if (library_id.empty()) {
                const cJSON* n = cJSON_GetObjectItemCaseSensitive(r.result.get(), "item_id");
                if (cJSON_IsNumber(n)) library_id = std::to_string(static_cast<long long>(n->valuedouble));
            }
            if (library_id.empty()) return ctx_.publish(Toast{i18n::tr(i18n::Str::toast_favourite_failed), std::nullopt, {}, true});
            remove(type, library_id);
        });
    }

    void remove(core::MediaType type, const std::string& library_id) {
        ctx_.send(ma::cmd::favorite_remove(type, library_id), [this](backend::Response&&) {
            ctx_.toast(i18n::tr(i18n::Str::toast_favourite_removed));
            ctx_.dispatch(cmd::Browse{cmd::BrowseKind::Favourites, {}});
        });
    }

private:
    Context& ctx_;
};

}  // namespace

std::unique_ptr<Feature> make_favourites(Context& ctx) { return std::make_unique<Favourites>(ctx); }

}  // namespace app
