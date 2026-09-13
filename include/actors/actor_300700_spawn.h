#ifndef ACTOR_300700_SPAWN_H
#define ACTOR_300700_SPAWN_H

#include "gameplay/3A34.h"
#include "main/session.h"

/// The 0x2F4-byte allocation `func_actor_300700_80161E80` makes with
/// `Mem_Calloc` and stores in the task's work slot, then fills with the three
/// `GpObj` render nodes (`Gp_LinkObj`, shapes 2/2/3) and their `GpRec18`
/// tables. `Actor300700Work` is the wider view the tick handlers use of the
/// same object.
typedef struct Actor300700SpawnWork {
    /* 0x000 */ byte    field_0[0x14];
    /* 0x014 */ byte    field_14[0xA0]; // four GpAnimSlots, `func_800B3F84` arg4
    /* 0x0B4 */ byte    field_B4[0x40]; // pose buffer, `func_800B3F84` arg3
    /* 0x0F4 */ MATRIX  field_F4;       // color matrix handed to the stream
    /* 0x114 */ MATRIX  field_114;      // light matrix handed to the stream
    /* 0x134 */ GpObj   obj134;
    /* 0x154 */ GpRec18 rec154;
    /* 0x16C */ GpObj   obj16C;
    /* 0x18C */ GpRec18 rec18C[4];
    /* 0x1EC */ GpObj   obj1EC;
    /* 0x20C */ GpRec18 rec20C;
    /* 0x224 */ void*   field_224;
    /* 0x228 */ u16     field_228;
    /* 0x22A */ u16     field_22A;
    /* 0x22C */ byte    pad_22C[0x80];
    /* 0x2AC */ s32     field_2AC;
    /* 0x2B0 */ s32     field_2B0;
    /* 0x2B4 */ s32     field_2B4;
    /* 0x2B8 */ byte    pad_2B8[0x1E];
    /* 0x2D6 */ u16     field_2D6;
    /* 0x2D8 */ byte    pad_2D8[4];
    /* 0x2DC */ u16     field_2DC;
    /* 0x2DE */ byte    pad_2DE[0x16];
} Actor300700SpawnWork;
STATIC_ASSERT_SIZEOF(Actor300700SpawnWork, 0x2F4);

#endif
