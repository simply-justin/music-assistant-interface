#pragma once
// The core: owns all state, runs on one task. The UI talks to it only through Commands in and
// Updates out; the target build wires those to FreeRTOS queues (main.cpp).
#include <memory>
#include <vector>

#include "app/feature.hpp"

namespace app {

class App {
public:
    App(backend::Backend& backend, Settings& settings, std::function<void(Update)> publish);

    void start();
    void handle(const Command& command);
    void tick(uint32_t now_ms);

private:
    Context ctx_;
    std::vector<std::unique_ptr<Feature>> features_;
};

}  // namespace app
