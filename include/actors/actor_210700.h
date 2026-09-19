#ifndef ACTOR_210700_H
#define ACTOR_210700_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "main/task.h"
#include "main/tmd.h"

/// 0x540-byte work block allocated by `func_actor_210700_80149F90`
/// (`memCalloc(0x540, 0)`) and parked in that task's `Task::work` slot.
/// `func_actor_210700_8014A208` republishes the two matrices onto the model's
/// `TmdObject::lightMtx` / `field_20` -- the light/colour matrix pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`
/// -- so this actor lights itself out of its own block.
///
/// `Gp_AnimCtx` sits at the head of the block, as it does in `Actor335800Work`
/// and `Actor361100Work`: `func_actor_210700_8014A0AC` hands the block base
/// straight to `Gp_AnimTickIndex` for slots 1..0x13.
///
/// The four fields named below are the ones that init writes before the
/// state machine runs; everything else is still padding.
typedef struct Actor210700Work {
    /* 0x000 */ byte pad_0[0x474];
    /// Non-zero gates the per-frame animation tick loop.
    /* 0x474 */ s32    field_474;
    /* 0x478 */ s32    field_478; // init'd to -1
    /* 0x47C */ s32    field_47C; // init'd to -1
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
    /* 0x4C0 */ byte   pad_4C0[0x78];
    /* 0x538 */ s16    field_538; // set to 1 by the 0x7E0 image handler's mode 3
    /* 0x53A */ u16    field_53A; // upload countdown the state handler at 0x80149E30 runs down, reloaded from `field_538` on underflow
    /* 0x53C */ s16    field_53C; // set to 1 by the 0x7E0 image handler's mode 3
    /* 0x53E */ s16    field_53E; // init'd to -1, latched from the 0x7D5 handler's mode 2
} Actor210700Work;
STATIC_ASSERT_SIZEOF(Actor210700Work, 0x540);

/// Script-command payload of the overlay's id 0x7D3 handler
/// `func_actor_210700_8014A224`. `field_0` indexes the overlay's
/// `D_actor_210700_801585C8` animation table and is latched into
/// `Actor210700Work::field_47C`; `field_4` is latched into `field_478` as the
/// bank every animation slot 1..0x13 is then published with; a non-zero
/// `field_8` takes the `func_800B4114` publish path instead of the
/// `Gp_AnimResetSlot` reset.
///
/// Only the first three words are read, but the block is 0x18 bytes: the spawn
/// handler's frame puts its two payloads at +0x10 and +0x28 and its ray result
/// at +0x40, which needs a 0x18 stride, and the handler is registered in
/// `D_actor_210700_801585D8` as a `GpMsgHandler`, whose `arg2` block is sized
/// by the caller.
typedef struct _Actor210700Anim {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte pad_C[0xC];
} Actor210700Anim;
STATIC_ASSERT_SIZEOF(Actor210700Anim, 0x18);

/// The overlay's script-command table, parked in `Task::msgTable`: 0x7D3
/// `func_actor_210700_8014A224`, 0x7D4 `ActorsShared801334c4`, 0x7D5
/// `func_actor_210700_8014A3D4`, 0x7E0 `func_actor_210700_8014A4B0`.
extern GpMsgEntry D_actor_210700_801585D8[];

void func_actor_210700_80149E30(GpActorWork* arg0);
void func_actor_210700_8014A208(Task* arg0);
s32  func_actor_210700_8014A224(Task* task, s32 arg1, Actor210700Anim* args, s32 arg3);

#endif
