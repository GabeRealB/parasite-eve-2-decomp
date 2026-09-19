#ifndef ACTOR_213000_H
#define ACTOR_213000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"

#include "main/task.h"

/// Work block this actor parks in `Task::work` (`memCalloc(0x4C4)`), the
/// 0x4C4-byte shape `Actor335800MainWork` has. It opens with the animation
/// context the block itself is handed to as (`func_800B3F84` takes the block
/// address), the 0x14 0x28-byte slots immediately above it and the 0x140-byte
/// table `func_800B3F84` also takes at 0x334 -- so the index the slot walkers
/// run to, 0x14, is the slot count. `field_474` / `field_477` drive the
/// animation teardown, and `light` / `color` are the matrices the TMD object's
/// `field_1C` / `field_20` are republished from. `field_4BC` / `field_4C0`
/// hold the two model tasks the visibility switch picks between; the TMD
/// object is then reached through their `Task::extra`. The spawn handler seeds
/// the three `s8` bytes at 0x475/0x476/0x477 to -1 and clears the word beside
/// them.
typedef struct Actor213000Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140];
    /* 0x474 */ s8         field_474; // nonzero while the animation context is live
    /* 0x475 */ s8         field_475;
    /* 0x476 */ s8         field_476;
    /* 0x477 */ s8         field_477; // counts down to the buffer teardown
    /* 0x478 */ s32        field_478;
    /* 0x47C */ MATRIX     light;
    /* 0x49C */ MATRIX     color;
    /* 0x4BC */ Task*      field_4BC; // model task modes 0 / 1 act on
    /* 0x4C0 */ Task*      field_4C0; // model task modes 2 / 3 act on
} Actor213000Work;
STATIC_ASSERT_SIZEOF(Actor213000Work, 0x4C4);

/// Overlay of `GsCOORDINATE2` at `TmdObject::coords`, one slot per model part.
/// `func_actor_213000_8014A35C` clears three of them and links each to the
/// spawner's slot nine higher, which is what puts the extra `sub` field at
/// 0x4C past the four bytes `Actor511000Coord` stops at: offsets 0x44 and 0x48
/// (libgs `param` and `super`) are the Euler angles the animation writers fill
/// and then hand straight to `RotMatrix`, the same reuse that header documents.
typedef struct Actor213000Coord {
    /* 0x00 */ s32            flg;
    /* 0x04 */ MATRIX         coord;
    /* 0x24 */ MATRIX         workm;
    /* 0x44 */ SVECTOR        rot;
    /* 0x4C */ GsCOORDINATE2* sub; // spawner's slot 9 higher, see above
} Actor213000Coord;
STATIC_ASSERT_SIZEOF(Actor213000Coord, 0x50);

/// The message body the visibility handler reads its mode from: the kind and
/// the mode halfword pair the other actor headers carry at 0x0 / 0x2.
typedef struct Actor213000Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor213000Msg;
STATIC_ASSERT_SIZEOF(Actor213000Msg, 0x4);

/// Animation preset `func_actor_213000_8014A70C` applies: `field_0` indexes
/// the animation bank table (`D_actor_213000_80157DDC` here,
/// `D_actor_335800_8016EAD8` there) and is latched into the work block's
/// `field_476`, re-seeding the slot array whenever it changes; `field_4` is
/// the animation id stored into `field_475`; `field_8` picks between
/// `func_800B4114` and `Gp_AnimResetSlot`. The same record
/// `Actor335800AnimPreset` and `Actor361100AnimPreset` are read as, except
/// that this handler ignores `field_C`: the sender's blend length reaches
/// `func_800B4114` as a literal 6 instead.
typedef struct Actor213000AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor213000AnimPreset;
STATIC_ASSERT_SIZEOF(Actor213000AnimPreset, 0x10);

s32 func_actor_213000_8014A980(Task* task, s32 arg1, Actor213000Msg* msg);

s32 func_actor_213000_8014A70C(Task* task, s32 arg1, Actor213000AnimPreset* msg);

#endif // ACTOR_213000_H
