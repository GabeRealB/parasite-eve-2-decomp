#ifndef ACTOR_143000_H
#define ACTOR_143000_H

#include "common.h"

#include "main/task.h"

typedef struct Actor143000Work {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ u16  field_2;
    /* 0x04 */ s16  field_4;
    /* 0x06 */ byte pad_6[1];
    /* 0x07 */ s8   field_7;
    /* 0x08 */ byte pad_8[4];
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s8   field_12;
    /* 0x13 */ s8   field_13;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s16  field_16;
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
} Actor143000Work;
STATIC_ASSERT_SIZEOF(Actor143000Work, 0x1C);

typedef struct Actor143000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor143000Work* field_1C;
    /* 0x20 */ byte             pad_20[0xA];
    /* 0x2A */ s16              field_2A;
    /* 0x2C */ byte             pad_2C[4];
    /* 0x30 */ s32              field_30;
} Actor143000;

typedef struct Actor143000Spawn {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor143000Spawn;
STATIC_ASSERT_SIZEOF(Actor143000Spawn, 4);

/// One entry of the `D_actor_143000_80134580` list, a screen-space sprite rect
/// `func_actor_143000_80133334` draws as `(x, y, x + w, y + h)`. The list is
/// terminated by an entry whose `field_8` is -1.
typedef struct Actor143000Rect {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s8  field_A;
    /// Non-zero while the entry is live for `func_actor_143000_801325F0`;
    /// `func_actor_143000_801324C8` clears it on every entry at spawn.
    /* 0xB */ s8 field_B;
} Actor143000Rect;
STATIC_ASSERT_SIZEOF(Actor143000Rect, 0xC);

/// State table of the actor's `Task::callback`, `func_actor_143000_801335C8`:
/// eleven `TaskFunc` entries, one per `Task::state`, which that body copies
/// onto its stack before indexing. Entry 7 is the step-switch body
/// `func_actor_143000_80132A04`; the rest are the spawners and per-state
/// handlers in the sibling units.
extern TaskFuncTable11 D_actor_143000_80131E84;

void func_actor_143000_80132A04(Actor143000* arg0);
void func_actor_143000_80132D10(Actor143000* arg0);
void func_actor_143000_80133334(Actor143000Rect* arg0, u8 r, u8 g, u8 b);
void func_actor_143000_80133EE4(Task* arg0);

#endif
