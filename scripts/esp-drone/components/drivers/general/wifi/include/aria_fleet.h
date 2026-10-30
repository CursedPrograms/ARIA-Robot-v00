// aria_fleet.h - ARIA in the DREAM Robotics fleet (aria_fleet.c)
#pragma once

#include "esp_netif.h"

// After WiFi is up, as AP + STA. sta: her station interface, for the fleet's network.
void ariaFleetStart(esp_netif_t *sta);
