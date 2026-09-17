#ifndef ROOMS_NEO_ARK_SAVANNA_ZONE_H
#define ROOMS_NEO_ARK_SAVANNA_ZONE_H

#include "common.h"

#include "gameplay/1A8.h"
#include "main/task.h"

/// Event parameters copied to the room's pending event `D_..._8018099C`, which
/// the room's own event state machine runs: `field_0` is the cap command handed
/// to `Gp_RunCapCmd`, `field_4` the stage sound, `field_8` the game flag checked
/// and set when the event starts, and `field_A` the helper-spawn switch.
typedef struct NeoArkSavannaZoneEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} NeoArkSavannaZoneEvent;
STATIC_ASSERT_SIZEOF(NeoArkSavannaZoneEvent, 0xC);

extern TaskDesc               D_neo_ark_savanna_zone_8017F9A0;
extern GpSaveLoc              D_neo_ark_savanna_zone_80180990;
extern s8                     D_neo_ark_savanna_zone_80180998;
extern NeoArkSavannaZoneEvent D_neo_ark_savanna_zone_8018099C;

#endif // ROOMS_NEO_ARK_SAVANNA_ZONE_H
