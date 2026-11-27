// Firmware updates: check for a newer published version, install it, restart into it.
#include "features/features.hpp"

namespace app {
namespace {

class Updater : public Feature {
public:
    explicit Updater(Context& ctx) : ctx_(ctx) {}

    void on_command(const Command& c) override {
        if (std::holds_alternative<cmd::CheckUpdate>(c)) {
            if (busy()) return;
            set({UpdateState::Checking, {}, 0});
            ctx_.platform.update_check([this](std::string version, bool online) {
                if (!online) return set({UpdateState::NoNetwork, {}, 0});
                if (version.empty()) return set({UpdateState::Failed, {}, 0});
                bool newer = version != ctx_.platform.system_info().firmware;
                set({newer ? UpdateState::Available : UpdateState::UpToDate, version, 0});
            });
        } else if (std::holds_alternative<cmd::InstallUpdate>(c)) {
            if (view_.state != UpdateState::Available) return;
            set({UpdateState::Installing, view_.version, 0});
            ctx_.platform.update_install(
                [this](int percent) {
                    if (percent != view_.percent) set({UpdateState::Installing, view_.version, percent});
                },
                [this](bool ok) {
                    if (!ok) return set({UpdateState::Failed, {}, 0});
                    ctx_.platform.restart();  // boots the new version; it confirms itself once it runs
                });
        }
    }

private:
    bool busy() const { return view_.state == UpdateState::Checking || view_.state == UpdateState::Installing; }
    void set(UpdateView v) {
        view_ = std::move(v);
        ctx_.publish(view_);
    }

    Context& ctx_;
    UpdateView view_;
};

}  // namespace

std::unique_ptr<Feature> make_update(Context& ctx) { return std::make_unique<Updater>(ctx); }

}  // namespace app
