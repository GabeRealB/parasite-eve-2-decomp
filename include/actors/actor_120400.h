#ifndef ACTOR_120400_H
#define ACTOR_120400_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the parent task, allocated by `func_actor_120400_80131E5C`
/// (`memCalloc(0x504)`) and parked in its `Task::work` slot -- that slot is
/// not a `TaskIdMap` here. The init seeds the two `sb` bytes at 0x475/0x476
/// and the `sh` at 0x500 to -1 and clears the three step accumulators.
///
/// The size is the allocation. The fields below are the ones the parent's
/// handlers touch: the animation bank and its 0x14 `GpAnimSlot`s, the pose
/// buffer, the light/colour matrix pair its model is pointed at, the placement
/// the 0x7DD handler stores, the walk velocity and its accumulators, the
/// arrival threshold and the two motion indices.
typedef struct Actor120400MainWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];  // the slot array `func_800B3F84` is handed
    /* 0x334 */ byte       poses[0x140]; // pose buffer `func_800B3F84` is handed
    /* 0x474 */ s8         field_474;    // set once the slots have been seeded and ticked
    /* 0x475 */ s8         field_475;    // animation id the slot loops are seeded with
    /* 0x476 */ s8         field_476;    // bank id, latched from the preset's `field_0`
    /* 0x477 */ s8         field_477;    // preset byte later presets pass as `field_4`
    /* 0x478 */ MATRIX     light;
    /* 0x498 */ MATRIX     color;
    /* 0x4B8 */ VECTOR3    target; // placement position, the point the walk heads for
    /* 0x4C4 */ byte       pad_4C4[0x4C8 - 0x4C4];
    /* 0x4C8 */ VECTOR3    step;   // per-frame 16.16 velocity, added to the accumulators
    /* 0x4D4 */ byte       pad_4D4[0x4D8 - 0x4D4];
    /* 0x4D8 */ s32        field_4D8;
    /* 0x4DC */ s32        field_4DC;
    /* 0x4E0 */ s32        field_4E0;
    /* 0x4E4 */ byte       pad_4E4[0x4E8 - 0x4E4];
    /* 0x4E8 */ SVECTOR    limit;     // arrival threshold; 0x7FFF on all three disables it
    /* 0x4F0 */ u16        field_4F0; // placement rotation, copied verbatim
    /* 0x4F2 */ u16        field_4F2;
    /* 0x4F4 */ u16        field_4F4;
    /* 0x4F6 */ byte       pad_4F6[0x4F8 - 0x4F6];
    /* 0x4F8 */ s16        field_4F8; // motion handler the tick runs: 0 idle, 1 the walk sequence
    /* 0x4FA */ s16        field_4FA; // step of the walk sequence, index into `D_actor_120400_80131E3C`
    /* 0x4FC */ byte       pad_4FC[0x500 - 0x4FC];
    /* 0x500 */ s16        field_500; // frames until the model's buffers are freed; negative when idle
    /* 0x502 */ byte       pad_502[0x504 - 0x502];
} Actor120400MainWork;
STATIC_ASSERT_SIZEOF(Actor120400MainWork, 0x504);

/// The `GsCOORDINATE2` at `TmdObject::coords` seen with the Euler angles libgs
/// keeps in `param` at 0x44: the actor hands those straight to `RotMatrix`.
typedef struct Actor120400Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor120400Coord;
STATIC_ASSERT_SIZEOF(Actor120400Coord, 0x4C);

/// Animation source indexed by the bank id the presets latch:
/// `D_actor_120400_8013E744[work->field_476]` is the bank handed to
/// `func_800B3F84`.
extern void* D_actor_120400_8013E744[];

/// Animation preset: `field_0` is the bank id, `field_4` the animation id,
/// `field_8` selects restarting the slots through `func_800B4114` over
/// resetting them, and `field_C` is the restart's last argument.
typedef struct Actor120400AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor120400AnimPreset;
STATIC_ASSERT_SIZEOF(Actor120400AnimPreset, 0x14);

/// Position and Euler angles carried by messages 0x7D4 and 0x7DD: 0x7D4 places
/// the root coordinate there, 0x7DD stores them in the parent block as the
/// walk's destination and final heading.
typedef struct Actor120400Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor120400Placement;
STATIC_ASSERT_SIZEOF(Actor120400Placement, 0x18);

/// Optional start animation for `func_actor_120400_80132398`: the preset's
/// `field_4` and the `field_477` byte. Absent, the defaults are 0x10 and 1.
typedef struct Actor120400SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor120400SpawnAnim;
STATIC_ASSERT_SIZEOF(Actor120400SpawnAnim, 0x8);

/// Spawn, tick and teardown handlers of the parent task, dispatched by
/// `func_actor_120400_80132748`.
extern TaskFuncTable3 D_actor_120400_80131E30;

/// Spawn, tick and teardown handlers of the two child tasks, dispatched by
/// `func_actor_120400_8013254C`.
extern TaskFuncTable3 D_actor_120400_80131E24;

/// Steps of the parent's walk sequence, indexed by
/// `Actor120400MainWork::field_4FA`.
extern TaskFuncTable4 D_actor_120400_80131E3C;

/// The constant local-space offset the walk rotates into its velocity:
/// straight ahead along the root part's own +Z.
extern VECTOR D_actor_120400_80131E4C;

void func_actor_120400_8013254C(Task* task);
void func_actor_120400_801326B0(Task* task);
void func_actor_120400_801327B4(Task* task);
void func_actor_120400_801327D4(Task* task);
void func_actor_120400_801327F0(Task* arg0);
void func_actor_120400_801327F8(Task* task);
void func_actor_120400_80132860(Task* task);
void func_actor_120400_80132920(Task* task);

/// Draws the ground-effect quad `func_800EA1A8` filled in, at the splash id
/// the caller selects and the current ground-shade row `Gp_State1C->groundShade`.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

s32 func_actor_120400_80132398(Task* task, s32 arg1, Actor120400Placement* place, Actor120400SpawnAnim* anim);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

s32 func_actor_120400_80132AA0(Task* task, s32 arg1, Actor120400AnimPreset* msg, s32 arg3);
s32 func_actor_120400_80132BBC(Task* task, s32 arg1, Actor120400Placement* args);
s32 func_actor_120400_80132C38(Task* task, s32 arg1, s32 mode, s32 arg3);

#endif
