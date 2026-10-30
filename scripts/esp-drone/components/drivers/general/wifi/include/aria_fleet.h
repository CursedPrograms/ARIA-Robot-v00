// aria_fleet.h - ARIA in the DREAM Robotics fleet (aria_fleet.c)
#pragma once

#include "esp_netif.h"

// After WiFi is up, as AP + STA. sta: her station interface, for the fleet's network.
void ariaFleetStart(esp_netif_t *sta);

// wifi_esp32.c: a CRTP packet into the same queue as one from the app's UDP
bool wifiInjectPacket(const uint8_t *data, uint8_t size);

// aria_ble.c: flying over Bluetooth, on the ESP32-S3
void ariaBleStart(void);
