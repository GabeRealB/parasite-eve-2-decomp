#ifndef ROOMS_MINE_MESA_H
#define ROOMS_MINE_MESA_H

#include "common.h"

#include "gameplay/1A8.h"

/// Event parameters copied to the room's pending event. The room's event
/// state machine (`func_mine_mesa_8017D670`) runs the cap command in `field_0`
/// and the stage sound in `field_4`; `field_8` is the game flag checked and set
/// when the event starts, and `field_A` tells the state machine whether to
/// spawn its helper task.
typedef struct MineMesaEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} MineMesaEvent;
STATIC_ASSERT_SIZEOF(MineMesaEvent, 0xC);

extern GpSaveLoc     D_mine_mesa_80189B40;
extern s8            D_mine_mesa_80189B48;
extern MineMesaEvent D_mine_mesa_80189B60;

#endif // ROOMS_MINE_MESA_H
