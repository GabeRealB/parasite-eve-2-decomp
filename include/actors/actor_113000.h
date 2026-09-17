#ifndef ACTOR_113000_H
#define ACTOR_113000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Work block this actor parks in `Task::idMap`. The per-frame state fields
/// (`0x474`..`0x4C8`) are the animation context its handlers walk; `light` /
/// `color` are the matrices the TMD object's `field_1C` / `field_20` are
/// republished from. The spawn handler seeds the two -1 words at 0x478 / 0x47C
/// and the halfword pair at 0x4C6 / 0x4C8; only those and the matrix pair are
/// spelled out, the prefix fields are unreferenced by the bodies matched so
/// far.
typedef struct Actor113000Work {
    /* 0x000 */ byte   pad_0[0x478];
    /* 0x478 */ s32    field_478; ///< -1 out of the spawn handler
    /* 0x47C */ s32    field_47C; ///< -1 out of the spawn handler
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ s16    field_4C0; ///< upload countdown reload; set to 1 alongside `field_4C4` by mode 3
    /* 0x4C2 */ u16    field_4C2; ///< upload countdown the per-frame state runs down, reloaded from `field_4C0` on underflow
    /* 0x4C4 */ s16    field_4C4; ///< upload step 1..3; set to 1 alongside `field_4C0` by mode 3
    /* 0x4C6 */ s16    field_4C6; ///< cleared by the spawn handler
    /* 0x4C8 */ s16    field_4C8; ///< -1 out of the spawn handler
    /* 0x4CA */ byte   pad_4CA[0x2];
} Actor113000Work;
STATIC_ASSERT_SIZEOF(Actor113000Work, 0x4CC);

#endif // ACTOR_113000_H
