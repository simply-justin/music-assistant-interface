#pragma once

namespace board {

// Checks the ESP32-C6 co-processor's ESP-Hosted version and, when it is a major version behind,
// updates it from the image embedded in this firmware (on its own task; restarts when done).
// Call after Wi-Fi is initialised.
void update_coprocessor_if_needed();

}  // namespace board
