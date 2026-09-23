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

/// One entry of the lists a `MineMesaLayout` points at: an x/y/z triple
/// padded to 8 bytes.
typedef struct MineMesaVec {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 pad;
} MineMesaVec;
STATIC_ASSERT_SIZEOF(MineMesaVec, 0x8);

/// A 12-byte record that is only ever copied whole, so its fields are unknown.
typedef struct MineMesaRecord {
    /* 0x0 */ u16 field_0[6];
} MineMesaRecord;
STATIC_ASSERT_SIZEOF(MineMesaRecord, 0xC);

/// Pointers into the room's layout data. The overlay holds a template
/// (`D_mine_mesa_801864A4`) and a live copy (`D_mine_mesa_8018700C`), which
/// `func_mine_mesa_8017EB54` fills from the template. `field_4` and `field_C`
/// have three entries each, and `field_8` has eight. The struct may extend
/// past `field_C`.
typedef struct MineMesaLayout {
    /* 0x0 */ s32             field_0;
    /* 0x4 */ MineMesaVec*    field_4;
    /* 0x8 */ MineMesaVec*    field_8;
    /* 0xC */ MineMesaRecord* field_C;
} MineMesaLayout;

extern MineMesaLayout D_mine_mesa_801864A4;
extern MineMesaLayout D_mine_mesa_8018700C;

extern GpSaveLoc     D_mine_mesa_80189B40;
extern s8            D_mine_mesa_80189B48;
extern MineMesaEvent D_mine_mesa_80189B60;

#endif // ROOMS_MINE_MESA_H
