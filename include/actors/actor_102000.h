#ifndef ACTOR_102000_H
#define ACTOR_102000_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor02000Work {
    /* 0x000 */ byte pad_0[0x694];
    /* 0x694 */ s16  field_694;
    /* 0x696 */ byte pad_696[2];
    /* 0x698 */ s16  field_698;
    /* 0x69A */ byte pad_69A[2];
    /* 0x69C */ s16  field_69C;
    /* 0x69E */ s16  field_69E;
    /* 0x6A0 */ byte pad_6A0[2];
    /* 0x6A2 */ s16  field_6A2;
    /* 0x6A4 */ s16  field_6A4;
    /* 0x6A6 */ s16  field_6A6;
    /* 0x6A8 */ s16  field_6A8;
    /* 0x6AA */ byte pad_6AA[4];
    /* 0x6AE */ s16  field_6AE;
    /* 0x6B0 */ byte pad_6B0[2];
    /* 0x6B2 */ s16  field_6B2;
    /* 0x6B4 */ byte pad_6B4[0x26];
    /* 0x6DA */ s16  field_6DA;
} Actor02000Work;

/// `field_8` is the actor's `GsCOORDINATE2` array; entry 0 is the actor's own
/// coordinate.
typedef struct Actor02000Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
} Actor02000Obj2C;

/// Overlay-local view of the spawn parameter block (`Task::spawnArg2`).
/// `field_3C` points at the per-enemy parameter record whose byte 1 scales the
/// state-1 dwell timer.
typedef struct Actor02000Params {
    /* 0x0 */ byte pad_0[1];
    /* 0x1 */ u8   field_1;
} Actor02000Params;

typedef struct Actor02000Spawn {
    /* 0x00 */ byte              pad_0[0x3C];
    /* 0x3C */ Actor02000Params* field_3C;
} Actor02000Spawn;

typedef struct Actor02000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor02000Work*  field_1C;
    /* 0x20 */ Actor02000Spawn* field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor02000Obj2C* field_2C;
} Actor02000;

#endif
