#ifndef ACTOR_102400_H
#define ACTOR_102400_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Overlay-local view of the 0x1C work block. `field_134` is the halfword flag
/// the state handlers at +0x1A10 toggle to pick a direction; the two part
/// coordinates below move apart while it is set and back while it is clear.
typedef struct Actor102400Work {
    /* 0x000 */ byte pad_0[0x134];
    /* 0x134 */ s16  field_134;
} Actor102400Work;

/// `field_8` is the actor's `GsCOORDINATE2` array: entry 2 and entry 3 are the
/// two part coordinates this overlay repositions.
typedef struct Actor102400Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} Actor102400Obj2C;

typedef struct Actor102400 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor102400Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor102400Obj2C* field_2C;
} Actor102400;

void func_actor_102400_80134EB8(Actor102400* arg0);

#endif
