#ifndef ROOMS_NEO_ARK_PAVILION_H
#define ROOMS_NEO_ARK_PAVILION_H

#include "common.h"

#include "gameplay/1A8.h"

/// Event parameters copied to the room's pending event `D_..._80187A20`, which
/// the room's own event state machine (`func_neo_ark_pavilion_8017E854`) runs.
/// `field_0` is the cap command handed to `Gp_RunCapCmd`, `field_4` the stage
/// sound, `field_8` the game flag checked and set when the event starts, and
/// `field_A` the helper-spawn switch.
typedef struct NeoArkPavilionEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} NeoArkPavilionEvent;
STATIC_ASSERT_SIZEOF(NeoArkPavilionEvent, 0xC);

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern GpSaveLoc           D_neo_ark_pavilion_80187A14;
extern s8                  D_neo_ark_pavilion_80187A1C;
extern NeoArkPavilionEvent D_neo_ark_pavilion_80187A20;

#endif // ROOMS_NEO_ARK_PAVILION_H
