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
/// animation. `field_480` is the clip the slots are started on, latched into
/// `field_47E` once they are. `yaw` is the heading last applied to the model's
/// root coordinate, `field_4B8` the last animation record the footstep check
/// saw, and `field_4BC` a flag the message handler sets.
typedef struct Actor151000Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       field_34C;
    /* 0x34D */ byte       pad_34D[0x12F];
    /* 0x47C */ s16        field_47C;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ u16        field_480;
    /* 0x482 */ byte       pad_482[0x2C];
    /* 0x4AE */ u16        yaw;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        field_4B2;
    /* 0x4B4 */ s16        field_4B4;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ GpAnimRec* field_4B8;
    /* 0x4BC */ u8         field_4BC;
    /* 0x4BD */ byte       pad_4BD[0x3];
} Actor151000Work;
STATIC_ASSERT_SIZEOF(Actor151000Work, 0x4C0);

/// The enemy's work block, published by its spawn handler and by its task
/// body.
extern Actor151000Work* D_actor_151000_8013D37C;

/// The enemy's task, published by its spawn handler so the visibility opcode
/// can reach its model.
extern Task* D_actor_151000_8013D380;

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

/// Message payload the message handler takes: only the halfword at 0x2 is
/// read.
typedef struct Actor151000Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor151000Msg;

void func_actor_151000_80131F1C(GpEnemy* enemy, Task* task);
void func_actor_151000_80132084(Task* task);
void func_actor_151000_801323F4(Task* task);
void func_actor_151000_80132450(GpEnemy* enemy, Task* task);
void func_actor_151000_801324D4(Task* task);
void func_actor_151000_801324FC(Task* task);
void func_actor_151000_801325C4(void);
void func_actor_151000_80132610(void);
void func_actor_151000_801326AC(void);
s32  func_actor_151000_801327C8(Task* task, s32 arg1, s32 arg2);
s32  func_actor_151000_80132810(Task* task, s32 arg1, Actor151000Placement* placement);
s32  func_actor_151000_8013288C(Task* task, s32 arg1, Actor151000Msg* msg);
void func_actor_151000_80132A38(Task* task);

#endif
