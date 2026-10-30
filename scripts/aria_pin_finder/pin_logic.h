// pin_logic.h - the pin finder's sums, with no hardware in them, tested in scripts/aria_flight_test.
#pragma once

#include <stdbool.h>

#define BATTERY_DIVIDER (43.0f / 33.0f)   // 10k over 33k: battery.ino

typedef struct {
    const char *name;
    int sck, miso, mosi, cs;
} SpiMap;

typedef struct {
    const char *name;
    int sda, scl;
} I2cMap;

// The WHO_AM_I answers of the gyros she might have, or NULL. Only the first two can run CF-Drone
static inline const char *gyroName(int id)
{
    switch (id) {
        case 0x70: return "MPU6500";
        case 0x71: return "MPU9250";
        case 0x68: return "MPU6050";
        case 0x19: return "MPU6886";
        case 0x98: return "ICM20689";
        default: return 0;
    }
}

static inline bool pinIsListed(int pin, const int *pins, int count)
{
    for (int i = 0; i < count; i++) {
        if (pins[i] == pin) {
            return true;
        }
    }
    return false;
}

static inline int pulseDuty(float share, int bits)
{
    if (share > 0.3f) {
        share = 0.3f;                     // a pulse never goes higher, whatever's asked
    }
    return (int)(share * ((1 << bits) - 1));
}

static inline float batteryFromPin(float pinVolts)
{
    return pinVolts * BATTERY_DIVIDER;
}

static inline bool looksLikeBattery(float volts)
{
    return volts >= 3.0f && volts <= 4.35f;
}
