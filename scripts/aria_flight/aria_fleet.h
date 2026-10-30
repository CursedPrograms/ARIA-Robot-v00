// aria_fleet.h - what ARIA adds on top of CF-Drone.
#pragma once

#include <stdint.h>

#define ARIA_BLE 1                      // fly her over Bluetooth: 0 saves about 700 KB

extern volatile int ariaTalkLed;

void ariaLinkFrame(const uint8_t *frame);   // link_aria.ino: any core
void ariaFleetStart();                      // aria_fleet.cpp
void ariaBleStart();                        // aria_ble.cpp
