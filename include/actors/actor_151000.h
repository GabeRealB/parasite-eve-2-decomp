#ifndef ACTOR_151000_H
#define ACTOR_151000_H

#include "common.h"
#include "gameplay/1BC.h"
#include "main/task.h"

#include <psyq/libgte.h>

/// Work block of the overlay's enemy, allocated by its spawn handler
/// `func_actor_151000_80131F1C` with `memCalloc(0x4C0, 0)`, so the size is the
/// allocation. The handler publishes it in `D_actor_151000_8013D37C` as well as
/// the task's `Task::work`, and most of the enemy's code reaches it through the
/// global.
///
/// `light` and `color` become the model's light and colour matrices; `anim`,
/// `field_34C` and `slots` are handed to `func_800B3F84` to start the
/// animation. `state` is what the runner `func_actor_151000_80132084`
/// dispatches on: 1 restarts the clip in `animId` through
/// `func_actor_151000_801326AC` and 2 through `func_actor_151000_80132610`,
/// both then leaving it at 3, and both latch the clip into `field_47E`. `yaw` is
/// the heading last applied to the model's root coordinate, `travel` the frames
/// of walking left and `turnFrames` the frames of turning left. `stepRec` is
/// the last animation record the footstep check saw, and `footsteps` a flag the
/// message handler sets that makes the runner play footsteps.
typedef struct Actor151000Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       field_34C;
    /* 0x34D */ byte       pad_34D[0x12F];
    /* 0x47C */ s16        state;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ s16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        yaw;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        travel;
    /* 0x4B4 */ s16        turnFrames;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ GpAnimRec* stepRec;
    /* 0x4BC */ u8         footsteps;
    /* 0x4BD */ byte       pad_4BD[0x3];
} Actor151000Work;
STATIC_ASSERT_SIZEOF(Actor151000Work, 0x4C0);

/// Kept for `src/lib/actors_shared_8014a568.c` and
/// `src/lib/actors_shared_8014a79c.c`, which still include this header for
/// another carrier until the shared units are deleted.
extern Actor151000Work* ActorsShared80131f9cWork;

/// Payload of the placement opcode: the position the model's root coordinate
/// is moved to, and the yaw it is turned to.
typedef struct Actor151000Placement {
    /* 0x00 */ VECTOR pos;
    /* 0x10 */ byte   pad_10[2];
    /* 0x12 */ u16    yaw;
    /* 0x14 */ byte   pad_14[2];
} Actor151000Placement;
STATIC_ASSERT_SIZEOF(Actor151000Placement, 0x18);

#endif
