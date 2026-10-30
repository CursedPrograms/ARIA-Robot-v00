// aria_fleet_logic.h - the parts of ARIA's fleet code with no hardware in them, so they can be
// tested on a PC. The phrases are RIFT/Fleet/talk_bf.json: regenerate this when the phrasebook changes.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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

// The ladder, highest first (RIFT/Fleet/fleet_ladder.py). She joins, she never hosts.
static const char *const ARIA_LADDER[] = {
    "RIFT", "DREAM", "NINA", "TINA",
    "NORACTL", "WHIPCTL", "KIDA01CTL", "KIDA00CTL", "ARMCTL",
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
