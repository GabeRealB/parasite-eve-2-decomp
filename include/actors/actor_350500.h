#ifndef ACTOR_350500_H
#define ACTOR_350500_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_350500_801623CC` (`memCalloc(0x4C8)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_350500_8016247C` republishes the two matrices
/// onto `TmdObject::lightMtx` / `colorMtx`, so the actor draws with its own
/// lighting rather than the default pair.
///
/// The size is the allocation. The init seeds the three `sb` bytes at
/// 0x43D/0x43E/0x4C5 to -1 and clears the three words at 0x4A0..0x4A8.
typedef struct Actor350500Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];  // the slot array `func_800B3F84` is handed
    /* 0x30C */ byte       poses[0x130]; // pose buffer `func_800B3F84` is handed
    /* 0x43C */ s8         field_43C;    // animation-tick enable
    /* 0x43D */ s8         field_43D;    // current animation id
    /* 0x43E */ s8         field_43E;    // current bank index
    /* 0x43F */ s8         field_43F;    // animation id the approach step plays on arrival
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ VECTOR3    target;    // world position the actor walks to
    /* 0x48C */ byte       pad_48C[0x4];
    /* 0x490 */ VECTOR3    step;      // per-frame world-space delta the accumulators take
    /* 0x49C */ byte       pad_49C[0x4];
    /* 0x4A0 */ s32        field_4A0; // 16.16 accumulators; only the high half reaches the coordinate
    /* 0x4A4 */ s32        field_4A4;
    /* 0x4A8 */ s32        field_4A8;
    /* 0x4AC */ byte       pad_4AC[0x4];
    /* 0x4B0 */ SVECTOR    limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4B8 */ u16        field_4B8; // placement rotation
    /* 0x4BA */ u16        field_4BA; // placement yaw the final turn steers toward
    /* 0x4BC */ u16        field_4BC;
    /* 0x4BE */ byte       pad_4BE[0x2];
    /* 0x4C0 */ u16        field_4C0; // selects the idle or the walk tick handler
    /* 0x4C2 */ u16        field_4C2; // index into the walk-step table
    /* 0x4C4 */ s8         field_4C4; // variant the two-case message handler latches
    /* 0x4C5 */ s8         field_4C5; // frames until the model buffers are freed; -1 disables
    /* 0x4C6 */ byte       pad_4C6[0x2];
} Actor350500Work;
STATIC_ASSERT_SIZEOF(Actor350500Work, 0x4C8);

/// Payload of the two-case message handler `func_actor_350500_80162ABC`:
/// only the halfword at 0x2 is read, selecting the variant it latches into
/// `Actor350500Work::field_4C4`.
typedef struct Actor350500Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor350500Msg;
STATIC_ASSERT_SIZEOF(Actor350500Msg, 0x4);

/// Overlay of the `GsCOORDINATE2` at `TmdObject::coords`, the actor's root
/// part. Offset 0x44 (libgs `param`) holds the Euler angles the placement
/// and face-the-target steps write and hand straight to `RotMatrix`.
typedef struct Actor350500Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor350500Coord;
STATIC_ASSERT_SIZEOF(Actor350500Coord, 0x4C);

/// Position and rotation a placement message carries.
typedef struct Actor350500Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor350500Placement;
STATIC_ASSERT_SIZEOF(Actor350500Placement, 0x18);

void func_actor_350500_8016245C(Task* arg0);
void func_actor_350500_8016247C(Task* arg0);
void func_actor_350500_80162508(Task* task);
s32  func_actor_350500_80162960(Task* task, s32 msgId, Actor350500Placement* args);
s32  func_actor_350500_801629DC(Task* task, s32 arg1, s32 mode);
s32  func_actor_350500_80162ABC(Task* arg0, s32 arg1, Actor350500Msg* arg2);

#endif
