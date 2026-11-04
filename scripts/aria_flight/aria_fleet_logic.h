// aria_fleet_logic.h - the parts of ARIA's fleet code with no hardware in them, so they can be
// tested on a PC. The phrases are RIFT/Fleet/talk_bf.json: regenerate this when the phrasebook changes.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARIA_PORT 5014
#define ARIA_NAME_MAX 33
#define ARIA_SCAN_MAX 16
#define ARIA_TONE_GAP_MS 10
#define ARIA_VOICE_PCT 125      // a light, high voice
#define ARIA_BF_MAX 72

// The fleet's beacon, byte for byte (fleet_near.h)
typedef struct __attribute__((packed)) {
    char magic[4];      // "DRFL"
    uint8_t version;    // 1
    char name[12];
    uint16_t seq;
    char bf[ARIA_BF_MAX];
} AriaBeacon;

static const char *const ARIA_STATE_PARK = "++++++++[>++++++++++++++<-]>.---------------.<++++[>++++<-]>+.-------.";
static const char *const ARIA_STATE_GO = "++++++++[>+++++++++++++<-]>-.++++++++.";

static inline void ariaBeaconFill(AriaBeacon *b, bool moving, uint16_t seq)
{
    memset(b, 0, sizeof(*b));
    memcpy(b->magic, "DRFL", 4);
    b->version = 1;
    memcpy(b->name, "ARIA", 4);
    b->seq = seq;
    strncpy(b->bf, moving ? ARIA_STATE_GO : ARIA_STATE_PARK, ARIA_BF_MAX - 1);
}

// Flying from a browser. Only from her own WiFi (192.168.43.x, her AP), never over the fleet's network.
static inline bool ariaOnOwnWifi(uint32_t hostOrderIp)
{
    return (hostOrderIp & 0xFFFFFF00u) == 0xC0A82B00u;
}

// Who she can hear, from the fleet's beacons, smoothed as fleet_near.h does
#define ARIA_PEERS_MAX 12
#define ARIA_PEER_FORGET_MS 5000

typedef struct {
    char name[12];
    float rssi;
    uint32_t seenMs;
} AriaPeer;

static inline void ariaPeerHeard(AriaPeer *peers, const uint8_t *data, int len, int rssi, uint32_t nowMs)
{
    if (len < (int)offsetof(AriaBeacon, bf) || memcmp(data, "DRFL", 4) != 0 || data[4] != 1) {
        return;
    }
    char name[12];
    memcpy(name, data + 5, 11);
    name[11] = 0;
    if (!name[0] || strcmp(name, "ARIA") == 0) {
        return;
    }
    AriaPeer *free_ = NULL;
    for (int i = 0; i < ARIA_PEERS_MAX; i++) {
        if (peers[i].name[0] && strcmp(peers[i].name, name) == 0) {
            peers[i].rssi += (rssi - peers[i].rssi) * 0.3f;
            peers[i].seenMs = nowMs;
            return;
        }
        if (!peers[i].name[0] && !free_) {
            free_ = &peers[i];
        }
    }
    if (free_) {
        memcpy(free_->name, name, 12);
        free_->rssi = rssi;
        free_->seenMs = nowMs;
    }
}

// {"robots":[{"name":"WHIP","rssi":-52},...]}, as every robot's /near; forgets the silent ones
static inline void ariaNearJson(AriaPeer *peers, uint32_t nowMs, char *out, size_t size)
{
    size_t used = (size_t)snprintf(out, size, "{\"robots\":[");
    bool first = true;
    for (int i = 0; i < ARIA_PEERS_MAX; i++) {
        if (!peers[i].name[0]) {
            continue;
        }
        if (nowMs - peers[i].seenMs > ARIA_PEER_FORGET_MS) {
            peers[i].name[0] = 0;
            continue;
        }
        if (used + 40 < size) {
            used += (size_t)snprintf(out + used, size - used, "%s{\"name\":\"%s\",\"rssi\":%d}", first ? "" : ",",
                                     peers[i].name, (int)peers[i].rssi);
            first = false;
        }
    }
    snprintf(out + used, size - used, "]}");
}

// The ladder, highest first (RIFT/Fleet/fleet_ladder.py). She joins, she never hosts.
static const char *const ARIA_LADDER[] = {
    "RIFT", "DREAM", "NINA", "TINA",
    "NORACTL", "WHIPCTL", "KIDA01CTL", "KIDA00CTL", "ARMCTL",
    "RITA",
    "NORA", "WHIP", "KIDA01", "KIDA00", "ARM", "MILA", "NIKA",
};

// The highest ladder network among the names seen, or NULL
static inline const char *ariaBestNetwork(const char *const *seen, size_t count)
{
    for (size_t l = 0; l < sizeof(ARIA_LADDER) / sizeof(ARIA_LADDER[0]); l++) {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(seen[i], ARIA_LADDER[l]) == 0) {
                return ARIA_LADDER[l];
            }
        }
    }
    return NULL;
}

static const char *const ARIA_TALK[] = {
    "++++++++[>+++++++++++++<-]>.+.",
    "++++++++[>+++++++++++++<-]>.++++++++++.+++.<++++++[>---------<-]>.",
    "+++++++++++[>+++++++++++<-]>.<++++[>------<-]>.<++++[>++++++<-]>.",
    "++++++++[>+++++++++++++<-]>.+++++.<+++++[>---------<-]>-.",
    "+++++++++++[>+++++++++++<-]>+...",
    "++++++++[>++++++++++++++<-]>.----.-----------.<++++[>++++++<-]>.",
    "+++++++[>++++++++++++++<-]>.<++++[>++++++<-]>-.<++++[>-----<-]>.",
    "++++++++[>+++++++++++++<-]>.+.<++++++++[>---------<-]>.",
    "++++++++++[>+++++++++++<-]>+.----.<+++++++[>-------<-]>.<++++[>----<-]>-.",
    "+++++++++++[>+++++++++++<-]>.<++++[>------<-]>.<++++[>++++++<-]>.<++++++++[>-----------<-]>.",
    "++++++++++[>+++++++++++<-]>+.-------.<+++++++[>----------<-]>-.",
    "++++++++++[>+++++++++++<-]>.-----.+++++++++++.---------------.",
    "+++++++++++[>+++++++++++<-]>.<++++[>-----<-]>.++++++++++++++.<+++++++++[>---------<-]>-.",
    "+++++++[>++++++++++++++<-]>.<++++[>++++++<-]>-.<++++[>-----<-]>.<++++++[>-----------<-]>--.",
    "+++++++++[>+++++++++<-]>+.",
    "++++++++[>++++++++++<-]>.",
    "+++++++[>++++++++++++<-]>-."
};

// Brainfuck for utterance u, or NULL
static inline const char *ariaTalkProgram(int u)
{
    return u >= 0 && u < (int)(sizeof(ARIA_TALK) / sizeof(ARIA_TALK[0])) ? ARIA_TALK[u] : NULL;
}

static inline uint32_t ariaToneHz(char c)
{
    uint32_t hz = 0;
    switch (c) {
        case '+': hz = 1568; break;
        case '-': hz = 784; break;
        case '>': hz = 1319; break;
        case '<': hz = 1047; break;
        case '[': hz = 880; break;
        case ']': hz = 1175; break;
        case '.': hz = 2093; break;
        case ',': hz = 523; break;
    }
    return hz * ARIA_VOICE_PCT / 100;
}

static inline uint32_t ariaToneMs(char c)
{
    return c == '.' ? 80 : 28;
}
