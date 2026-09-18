#ifndef ACTOR_120400_H
#define ACTOR_120400_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_120400_80131E5C` (`Mem_Calloc(0x504)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here, just as with `Actor335800MainWork` / `Actor350700MainWork`,
/// which this body is a two-child copy of: the init seeds the two `sb` bytes at
/// 0x475/0x476 and the `sh` at 0x500 to -1 and clears the three words at
/// 0x4D8..0x4E0, then spawns its two children from `D_actor_120400_8013E748`.
/// `ActorsShared80132f24` republishes the light/colour matrix pair onto the
/// parent's `TmdObject::lightMtx` / `field_20` from 0x478 / 0x498, exactly as it
/// does for those two.
///
/// The size is the allocation, and the fields below are the ones the init and
/// the placement handler touch: the animation bank at 0x000 and its 0x14
/// `GpAnimSlot`s, the pose buffer at 0x334, the `sb` bytes at 0x474..0x477, the
/// light/colour matrix pair at 0x478 / 0x498, and the placement the handler
/// stores at 0x4B8..0x4C0 / 0x4F0..0x4F4.
typedef struct Actor120400MainWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];  // the slot array `func_800B3F84` is handed
    /* 0x334 */ byte       poses[0x140]; // pose buffer `func_800B3F84` is handed
    /* 0x474 */ s8         field_474;
    /* 0x475 */ s8         field_475;    // animation id the slot loops are seeded with
    /* 0x476 */ s8         field_476;    // bank id, latched from the preset's `field_0`
    /* 0x477 */ s8         field_477;    // preset byte the placement handler passes as `field_4`
    /* 0x478 */ byte       pad_478[0x4B8 - 0x478];
    /* 0x4B8 */ s32        field_4B8;    // placement position, copied verbatim
    /* 0x4BC */ s32        field_4BC;
    /* 0x4C0 */ s32        field_4C0;
    /* 0x4C4 */ byte       pad_4C4[0x4C8 - 0x4C4];
    /* 0x4C8 */ VECTOR3    step; // per-frame 16.16 step, added to the accumulators
    /* 0x4D4 */ byte       pad_4D4[0x4D8 - 0x4D4];
    /* 0x4D8 */ s32        field_4D8;
    /* 0x4DC */ s32        field_4DC;
    /* 0x4E0 */ s32        field_4E0;
    /* 0x4E4 */ byte       pad_4E4[0x4F0 - 0x4E4];
    /* 0x4F0 */ u16        field_4F0; // placement rotation, copied verbatim
    /* 0x4F2 */ u16        field_4F2;
    /* 0x4F4 */ u16        field_4F4;
    /* 0x4F6 */ byte       pad_4F6[0x4F8 - 0x4F6];
    /* 0x4F8 */ s16        field_4F8;
    /* 0x4FA */ s16        field_4FA;
    /* 0x4FC */ byte       pad_4FC[0x500 - 0x4FC];
    /* 0x500 */ s16        field_500;
    /* 0x502 */ byte       pad_502[0x504 - 0x502];
} Actor120400MainWork;
STATIC_ASSERT_SIZEOF(Actor120400MainWork, 0x504);

/// Animation source the placement handler indexes by the bank id it just
/// latched: `D_actor_120400_8013E744[work->field_476]` is the bank handed to
/// `func_800B3F84`. Its callers' copies are `D_actor_335800_8016EAD8` and
/// `D_actor_317000_8016CF40`.
extern void* D_actor_120400_8013E744[];

/// Animation preset the placement handler builds on the stack and then reads
/// back through a pointer -- the shape that keeps the five words live in a
/// frame slot instead of registers. `field_0` is the bank id, `field_4` the
/// animation id (0x10 when the optional start animation is absent) with
/// `field_477` taking its companion byte (1 when absent), `field_8` 1,
/// `field_C` 5 and `field_10` 1.
typedef struct Actor120400AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor120400AnimPreset;
STATIC_ASSERT_SIZEOF(Actor120400AnimPreset, 0x14);

/// Spawn placement `func_actor_120400_80132398` copies into the parent block:
/// the position into `field_4B8..field_4C0`, the rotation into
/// `field_4F0..field_4F4`.
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

/// The empty handler that fills entry 0 of the parent's per-frame handler pair:
/// `func_actor_120400_80132050` selects `{func_actor_120400_801327F0,
/// ActorsShared801327f8}[work->field_4F8]`, so the pair's address is also the
/// switch the placement handler leaves in `field_4F8`. Its counterparts in the
/// sibling actors are `func_actor_350700_801633F8` / `func_actor_335800_80163B70`.
void func_actor_120400_801327F0(Task* arg0);

/// Draws the ground-effect quad `func_800EA1A8` filled in, at the splash id
/// the caller selects and the current ground-shade row `Gp_State1C->field_8`.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Placement handler for the parent block: stores the spawn position and
/// rotation, then applies a start preset. Returns 0.
s32 func_actor_120400_80132398(Task* task, s32 arg1, Actor120400Placement* place, Actor120400SpawnAnim* anim);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// `Gp_DispatchMsg` handler for message 0x7D5, the entry after 0x7D4 in the
/// actor's handler table `D_actor_120400_8013E76C`: the same four-way model
/// switch `ActorsShared80162bc4` performs, over this overlay's own work block.
/// `mode` drives the `TmdObject` parked in `Task::extra` -- bit 0x80 marks the
/// actor hidden and bit 0x4 the display buffers being live:
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, latch `mode` into `Actor120400MainWork::field_500`, raise 0x4
///   mode 3  show, raise 0x4
///
/// Any other mode returns 1; the four known ones return 0. `arg1` is unused --
/// the dispatch passes four arguments. Built by `actor_120400_6`.
s32 func_actor_120400_80132C38(Task* task, s32 arg1, s32 mode, s32 arg3);

#endif
