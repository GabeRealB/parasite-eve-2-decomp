#ifndef ROOMS_MINE_REFUGE_H
#define ROOMS_MINE_REFUGE_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// `Task::spawnArg2` of the cap (cutscene) task this room family spawns.
/// `field_0` is the area id forced for the duration of the scene (negative =
/// keep the current one); `field_1` selects the cap slot / command;
/// `field_2` skips straight to the abort state; `field_3` is the cap file to
/// load. The four s32s are sound-event ids, and `field_14` / `field_16` are
/// the `func_800E6D4C` fade pair.
typedef struct {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} MineRefugeCapScript;

STATIC_ASSERT_SIZEOF(MineRefugeCapScript, 0x18);

/// 0x1C-byte scratch block `func_mine_refuge_80180710` takes from
/// `G_SCRATCH_HEAD`. `otz`, `flag` and `sx`/`sy` receive the projection of the
/// glow's centre; `rOuter` and `rInner` are its two on-screen radii, derived
/// from that `otz`. Nothing in the function touches the leading bytes.
typedef struct {
    u8  _pad0[8];
    s32 otz;
    s32 flag;
    s32 rOuter;
    s32 rInner;
    u16 sx;
    u16 sy;
} MineRefugeGlowScratch;

STATIC_ASSERT_SIZEOF(MineRefugeGlowScratch, 0x1C);

extern MineRefugeCapScript D_mine_refuge_80182AE0;

void func_mine_refuge_8017F460(Task* task);
void func_mine_refuge_80180014(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_MINE_REFUGE_H
