// aria_link_logic.h - turning her app's flight frames into CF-Drone's stick inputs. No hardware in it,
// so scripts/aria_flight_test can run it on a PC.
//
// Every way of flying her (WiFi UDP, the browser, Bluetooth) ends up as the same 14 bytes: roll, pitch,
// yaw (little-endian floats) and thrust (uint16). Wire directions, the same as ESP-Drone's: roll + is
// right, pitch - is forward, yaw + is clockwise, thrust 0..60000.
//
// The rules the app was written against:
//   - thrust lock: she won't arm until she has seen a thrust of 0, and only from a low thrust;
//   - a thrust of 0 held for a moment disarms her;
//   - frames stop: she levels at 0.5 s and starts a controlled descent at 1 s.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define ARIA_FRAME_BYTES 14
#define ARIA_CRTP_BYTES 16          // header, the frame, checksum
#define ARIA_MAX_TILT 15.0f         // degrees
#define ARIA_MAX_YAW 120.0f         // degrees a second
#define ARIA_FULL_THRUST 60000
#define ARIA_MAX_THRUST 48000       // 80%, the app's top limit
#define ARIA_CF_TILT_MAX 30.0f      // CF-Drone's stick range: control.ino TILT_MAX
#define ARIA_CF_YAW_MAX 300.0f      // and YAWRATE_MAX
#define ARIA_ARM_THROTTLE 0.10f     // the app climbs from 0, so anything higher is a stick left up
#define ARIA_LEVEL_MS 500
#define ARIA_LOST_MS 1000
#define ARIA_DISARM_MS 300

typedef struct {
    float roll, pitch, yaw;         // wire values, capped
    uint16_t thrust;
    uint32_t frameMs;
    uint32_t zeroMs;                // when thrust went to 0 while armed
    bool heard;                     // a frame has ever arrived
    bool zeroing;                   // zeroMs is running
    bool unlocked;                  // seen thrust 0 since she last stopped
    bool pilot;                     // this link is flying her
} AriaLink;

typedef struct {
    bool drive;                     // write the sticks
    bool refresh;                   // and count this as the pilot being there
    bool arm, disarm;
    bool lost;                      // she has no pilot any more: bring her down
    float roll, pitch, yaw, throttle;
} AriaLinkOut;

static inline float ariaCap(float v, float top)
{
    if (!(v == v)) return 0;        // NaN
    return v > top ? top : v < -top ? -top : v;
}

// One frame. The page and app cap these too, but those are only suggestions.
static inline void ariaLinkTake(AriaLink *l, const uint8_t *in, uint32_t nowMs)
{
    float v[3];
    uint16_t thrust;
    memcpy(v, in, sizeof(v));
    memcpy(&thrust, in + sizeof(v), sizeof(thrust));
    l->roll = ariaCap(v[0], ARIA_MAX_TILT);
    l->pitch = ariaCap(v[1], ARIA_MAX_TILT);
    l->yaw = ariaCap(v[2], ARIA_MAX_YAW);
    l->thrust = thrust > ARIA_MAX_THRUST ? ARIA_MAX_THRUST : thrust;
    l->frameMs = nowMs;
    l->heard = true;
}

// A CRTP setpoint from her app or ESP-Drone's: out gets the frame. Port 3 channel 0, last byte the sum
static inline bool ariaCrtpFrame(const uint8_t *p, int len, uint8_t *out)
{
    if (len != ARIA_CRTP_BYTES || (p[0] & 0xF3) != 0x30) {
        return false;
    }
    uint8_t sum = 0;
    for (int i = 0; i < len - 1; i++) {
        sum += p[i];
    }
    if (sum != p[len - 1]) {
        return false;
    }
    memcpy(out, p + 1, ARIA_FRAME_BYTES);
    return true;
}

// Once a loop. armed is what the flight controller says, so anything else stopping her (low battery,
// upside down, the web page) also takes the pilot away until thrust goes back to 0.
static inline AriaLinkOut ariaLinkStep(AriaLink *l, bool armed, uint32_t nowMs)
{
    AriaLinkOut o;
    memset(&o, 0, sizeof(o));
    if (!l->heard) {
        return o;
    }
    if (!armed) {
        l->pilot = false;
        l->zeroing = false;
    }
    const uint32_t age = nowMs - l->frameMs;
    if (age >= ARIA_LOST_MS) {
        o.lost = l->pilot;
        l->unlocked = false;
        return o;
    }
    o.drive = true;
    o.throttle = (float)l->thrust / ARIA_FULL_THRUST;
    if (age >= ARIA_LEVEL_MS) {
        return o;                   // level, thrust held, and no longer counted as a pilot
    }
    o.refresh = true;
    o.roll = l->roll / ARIA_CF_TILT_MAX;
    o.pitch = -l->pitch / ARIA_CF_TILT_MAX;
    o.yaw = l->yaw / ARIA_CF_YAW_MAX;

    if (l->thrust == 0) {
        if (!armed) {
            l->unlocked = true;
        } else if (l->pilot) {
            if (!l->zeroing) {
                l->zeroing = true;
                l->zeroMs = nowMs;
            } else if (nowMs - l->zeroMs >= ARIA_DISARM_MS) {
                o.disarm = true;
            }
        }
        return o;
    }
    l->zeroing = false;
    if (armed) {
        l->pilot = true;
        l->unlocked = false;        // the next start needs its own 0
    } else if (l->unlocked && o.throttle <= ARIA_ARM_THROTTLE) {
        o.arm = true;
    } else {
        l->unlocked = false;
    }
    return o;
}
