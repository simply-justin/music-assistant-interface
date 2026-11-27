# ESP32-C6 co-processor firmware

`esp_hosted_cp_esp32c6.bin` is the Wi-Fi co-processor firmware for the ESP32-C6 on the
Waveshare board: ESP-Hosted 3.1.0, SDIO, built with ESP-IDF 5.5.6 from the `coprocessor_ota/cp`
example that ships with the esp_hosted component (4 MB flash, two OTA slots).

The panel firmware embeds it and, at start-up, updates the C6 over the existing SDIO link when
the C6 runs an older ESP-Hosted major version (the boards ship with firmware that reports 0.0.0).
See `main/board_cp.cpp`.

Rebuild it when the esp_hosted version in `dependencies.lock` changes; host and co-processor must
use the same version:

```sh
cp -r managed_components/espressif__esp_hosted/examples/ota/coprocessor_ota/cp /tmp/cp
# pin the same esp_hosted version as dependencies.lock in /tmp/cp/main/idf_component.yml
cd /tmp/cp && idf.py set-target esp32c6 && idf.py build
cp build/eh_cp_ota_coprocessor_ota.bin <repo>/main/coprocessor/esp_hosted_cp_esp32c6.bin
```
