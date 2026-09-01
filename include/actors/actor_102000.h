#ifndef ACTOR_102000_H
#define ACTOR_102000_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

typedef struct Actor02000Work {
    /* 0x000 */ byte pad_0[0x4E0];
    /* 0x4E0 */ s16  field_4E0;
    /* 0x4E2 */ byte pad_4E2[6];
    /* 0x4E8 */ s16  field_4E8;
    /* 0x4EA */ u16  field_4EA;
    /* 0x4EC */ byte pad_4EC[0x96];
    /* 0x582 */ u16  field_582;
    /* 0x584 */ byte pad_584[0x110];
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
    /* 0x6AA */ s16  field_6AA;
    /* 0x6AC */ byte pad_6AC[2];
    /* 0x6AE */ s16  field_6AE;
    /* 0x6B0 */ byte pad_6B0[2];
    /* 0x6B2 */ s16  field_6B2;
    /* 0x6B4 */ byte pad_6B4[4];
    /* 0x6B8 */ s16  field_6B8;
    /* 0x6BA */ byte pad_6BA[0x1A];
    /* 0x6D4 */ s16  field_6D4;
    /* 0x6D6 */ s16  field_6D6;
    /* 0x6D8 */ byte pad_6D8[2];
    /* 0x6DA */ s16  field_6DA;
    /* 0x6DC */ byte pad_6DC[2];
    /* 0x6DE */ s16  field_6DE;
    /* 0x6E0 */ s16  field_6E0;
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
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ u16               field_8;
    /* 0x0A */ byte              pad_A[0x32];
    /* 0x3C */ Actor02000Params* field_3C;
    /* 0x40 */ byte              pad_40[0xC];
    /* 0x4C */ u8                field_4C;
} Actor02000Spawn;

typedef struct Actor02000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor02000Work*  field_1C;
    /* 0x20 */ Actor02000Spawn* field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor02000Obj2C* field_2C;
    /* 0x30 */ s32              field_30;
} Actor02000;

#endif
