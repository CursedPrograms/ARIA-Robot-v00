// ARIA's link rules and fleet logic on a PC: g++ -std=c++17 -mno-ms-bitfields -I../aria_flight -I../aria_pin_finder test.cpp -o test
#include <cstdio>
#include <cstring>

#include "aria_fleet_logic.h"
#include "aria_link_logic.h"
#include "pin_logic.h"

static int failed = 0;
#define CHECK(x) do { if (!(x)) { printf("FAILED line %d: %s\n", __LINE__, #x); failed++; } } while (0)

static void frame(uint8_t *out, float roll, float pitch, float yaw, uint16_t thrust)
{
    float v[3] = {roll, pitch, yaw};
    memcpy(out, v, sizeof(v));
    memcpy(out + sizeof(v), &thrust, sizeof(thrust));
}

static AriaLinkOut step(AriaLink *l, bool armed, uint32_t now, float roll, float pitch, float yaw, uint16_t thrust)
{
    uint8_t f[ARIA_FRAME_BYTES];
    frame(f, roll, pitch, yaw, thrust);
    ariaLinkTake(l, f, now);
    return ariaLinkStep(l, armed, now);
}

static void silentUntilHeard()
{
    AriaLink l = {};
    AriaLinkOut o = ariaLinkStep(&l, false, 5000);
    CHECK(!o.drive && !o.arm && !o.lost);
}

static void armsOnlyAfterZero()
{
    AriaLink l = {};
    CHECK(!step(&l, false, 100, 0, 0, 0, 3000).arm);    // never seen 0
    CHECK(!step(&l, false, 110, 0, 0, 0, 0).arm);
    CHECK(step(&l, false, 120, 0, 0, 0, 3000).arm);
}

static void refusesAStickLeftUp()
{
    AriaLink l = {};
    step(&l, false, 100, 0, 0, 0, 0);
    CHECK(!step(&l, false, 110, 0, 0, 0, 30000).arm);
    CHECK(!step(&l, false, 120, 0, 0, 0, 3000).arm);    // must go back to 0 first
    step(&l, false, 130, 0, 0, 0, 0);
    CHECK(step(&l, false, 140, 0, 0, 0, 3000).arm);
}

static void directionsMatchTheApp()
{
    AriaLink l = {};
    // App: roll right, forward (wire pitch negative), clockwise
    AriaLinkOut o = step(&l, false, 100, 15, -15, 120, 0);
    CHECK(o.roll > 0.49f && o.roll < 0.51f);
    CHECK(o.pitch > 0.49f && o.pitch < 0.51f);          // CF-Drone: + is forward
    CHECK(o.yaw > 0.39f && o.yaw < 0.41f);              // CF-Drone: + is clockwise
}

static void capsWhateverArrives()
{
    AriaLink l = {};
    AriaLinkOut o = step(&l, false, 100, 90, 90, 900, 65000);
    CHECK(o.roll < 0.51f);
    CHECK(o.pitch > -0.51f);
    CHECK(o.yaw < 0.41f);
    CHECK(o.throttle < 0.81f);
    float nan = 0.0f / 0.0f;
    o = step(&l, false, 110, nan, nan, nan, 0);
    CHECK(o.roll == 0 && o.pitch == 0 && o.yaw == 0);
}

static void zeroThrustDisarms()
{
    AriaLink l = {};
    step(&l, false, 100, 0, 0, 0, 0);
    step(&l, true, 110, 0, 0, 0, 10000);                // now flying
    CHECK(!step(&l, true, 200, 0, 0, 0, 0).disarm);
    CHECK(!step(&l, true, 400, 0, 0, 0, 0).disarm);
    CHECK(step(&l, true, 510, 0, 0, 0, 0).disarm);
}

static void aBlipOfZeroDoesNot()
{
    AriaLink l = {};
    step(&l, false, 100, 0, 0, 0, 0);
    step(&l, true, 110, 0, 0, 0, 10000);
    step(&l, true, 200, 0, 0, 0, 0);
    step(&l, true, 300, 0, 0, 0, 10000);
    CHECK(!step(&l, true, 450, 0, 0, 0, 0).disarm);
    CHECK(!step(&l, true, 600, 0, 0, 0, 0).disarm);
}

static void silenceLevelsThenLands()
{
    AriaLink l = {};
    step(&l, false, 100, 0, 0, 0, 0);
    step(&l, true, 110, 10, -10, 50, 20000);
    AriaLinkOut o = ariaLinkStep(&l, true, 400);
    CHECK(o.refresh && o.roll != 0);
    o = ariaLinkStep(&l, true, 700);                    // 0.59 s: level, thrust held
    CHECK(o.drive && !o.refresh && o.roll == 0 && o.pitch == 0 && o.yaw == 0 && o.throttle > 0.3f);
    CHECK(!o.lost);
    o = ariaLinkStep(&l, true, 1200);                   // 1.09 s
    CHECK(o.lost && !o.drive);
    // She's landed: no pilot any more, and a stale frame can't start her
    o = ariaLinkStep(&l, false, 12000);
    CHECK(!o.lost && !o.arm);
    CHECK(!step(&l, false, 12100, 0, 0, 0, 3000).arm);
}

static void aFailsafeStopNeedsZeroAgain()
{
    AriaLink l = {};
    step(&l, false, 100, 0, 0, 0, 0);
    step(&l, true, 110, 0, 0, 0, 20000);
    CHECK(!step(&l, false, 200, 0, 0, 0, 2000).arm);    // low battery stopped her, throttle down a bit
    step(&l, false, 300, 0, 0, 0, 0);
    CHECK(step(&l, false, 310, 0, 0, 0, 2000).arm);
}

static void crtpChecksum()
{
    uint8_t p[ARIA_CRTP_BYTES] = {0x30}, out[ARIA_FRAME_BYTES];
    frame(p + 1, 1, 2, 3, 400);
    uint8_t sum = 0;
    for (int i = 0; i < ARIA_CRTP_BYTES - 1; i++) sum += p[i];
    p[ARIA_CRTP_BYTES - 1] = sum;
    CHECK(ariaCrtpFrame(p, ARIA_CRTP_BYTES, out));
    CHECK(memcmp(out, p + 1, ARIA_FRAME_BYTES) == 0);
    CHECK(!ariaCrtpFrame(p, ARIA_CRTP_BYTES - 1, out));
    p[3] ^= 1;
    CHECK(!ariaCrtpFrame(p, ARIA_CRTP_BYTES, out));
    p[3] ^= 1;
    p[0] = 0x31;
    CHECK(!ariaCrtpFrame(p, ARIA_CRTP_BYTES, out));
}

static void fleetBits()
{
    AriaBeacon b;
    ariaBeaconFill(&b, true, 7);
    CHECK(sizeof(AriaBeacon) == 91);
    CHECK(memcmp(b.magic, "DRFL", 4) == 0 && b.seq == 7);

    AriaPeer peers[ARIA_PEERS_MAX] = {};
    AriaBeacon whip;
    ariaBeaconFill(&whip, false, 1);
    memset(whip.name, 0, sizeof(whip.name));
    memcpy(whip.name, "WHIP", 4);
    ariaPeerHeard(peers, (const uint8_t *)&whip, sizeof(whip), -50, 1000);
    char json[200];
    ariaNearJson(peers, 2000, json, sizeof(json));
    CHECK(strstr(json, "WHIP") != NULL);
    ariaNearJson(peers, 9000, json, sizeof(json));
    CHECK(strstr(json, "WHIP") == NULL);

    const char *seen[] = {"MILA", "NINA", "other"};
    CHECK(strcmp(ariaBestNetwork(seen, 3), "NINA") == 0);
    CHECK(ariaOnOwnWifi(0xC0A82B05u));
    CHECK(!ariaOnOwnWifi(0xC0A80405u));
}

static void pinFinder()
{
    CHECK(strcmp(gyroName(0x70), "MPU6500") == 0);
    CHECK(gyroName(0xFF) == NULL);
    const int pins[] = {4, 5};
    CHECK(pinIsListed(5, pins, 2) && !pinIsListed(6, pins, 2));
    CHECK(pulseDuty(0.15f, 10) == 153);
    CHECK(pulseDuty(1.0f, 10) == 306);                  // capped, whatever's asked
    CHECK(batteryFromPin(3.2f) > 4.1f && batteryFromPin(3.2f) < 4.25f);
    CHECK(looksLikeBattery(3.7f) && !looksLikeBattery(0.4f));
}

int main()
{
    silentUntilHeard();
    armsOnlyAfterZero();
    refusesAStickLeftUp();
    directionsMatchTheApp();
    capsWhateverArrives();
    zeroThrustDisarms();
    aBlipOfZeroDoesNot();
    silenceLevelsThenLands();
    aFailsafeStopNeedsZeroAgain();
    crtpChecksum();
    fleetBits();
    pinFinder();
    printf(failed ? "%d FAILED\n" : "all passed\n", failed);
    return failed != 0;
}
