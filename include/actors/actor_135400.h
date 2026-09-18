#ifndef ACTOR_135400_H
#define ACTOR_135400_H

#include "common.h"

#include "actors/actors_shared_8013231c.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Per-actor state block for the `actor_135400` overlay's enemy: the
/// `Mem_Calloc(0x498, 0)` result `func_actor_135400_80132B60` stores in the
/// `Task::work` slot (0x1C) -- so this actor reuses that pointer field for its
/// own work block and it is *not* a `TaskIdMap` here. Reach it with
/// `(Actor135400Work*)task->work`. (`func_actor_135400_80132064`, in the
/// `actor_135400` unit, carves a different, 0x4C8-byte block for its own spawn
/// path; this is not that type.)
///
/// `lightMtx` / `colorMtx` are the model's own flat-light matrices:
/// `func_actor_135400_80132CB0` points the `TmdObject`'s `field_1C` /
/// `field_20` at them and fills them from the three `D_actor_135400_8013F904`
/// lights.
///
/// `field_43C` is the per-slot flag `func_actor_135400_80132D24` raises once it
/// has set the actor up; `field_43D` / `field_43E` latch the `field_4` / `field_0`
/// of the `GpAnimArg` that call was handed (`-1` until then), and `params` holds
/// the `D_actor_135400_80131EA0` defaults. The block opens with its own
/// animation context and the twenty 0x28-byte slots that follow it, the same
/// 0x334-byte prefix `Actor136100Work` carries: `func_actor_135400_801329B0`
/// ticks those slots through `Gp_AnimTickIndex` once `field_43C` is set.
typedef struct Actor135400Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       pad_334[0x108];
    /* 0x43C */ s8         field_43C;
    /* 0x43D */ s8         field_43D;
    /* 0x43E */ s8         field_43E;
    /* 0x43F */ byte       pad_43F[0x1];
    /* 0x440 */ MATRIX     lightMtx; // the model's `TmdObject::field_1C`
    /* 0x460 */ MATRIX     colorMtx; // the model's `TmdObject::field_20`
    /* 0x480 */ GpAnimArg  params;
    /* 0x494 */ s32        field_494;
} Actor135400Work;
STATIC_ASSERT_SIZEOF(Actor135400Work, 0x498);

/// Work block the actor's main task hangs off its `Task::work` slot (0x1C):
/// `func_actor_135400_80132064` allocates it (`Mem_Calloc(0x4C8, 0)`) for its
/// own spawn path, unlike the 0x498-byte `Actor135400Work` the sibling path
/// `func_actor_135400_80132B60` carves for the task it sets up. The same
/// function parks the 0x7D3 / 0x7D4 / 0x7D5 / 0x7DB handler table
/// `D_actor_135400_8013A4D0` in that task's `field_24`.
///
/// The block opens with its own animation context and the twenty 0x28-byte
/// slots that follow it, the same 0x474-byte prefix `Actor311900Anim` and
/// `Actor136100Work` carry: `func_actor_135400_80132650`, the tick's animation
/// setter, hands `func_800B3F84` the block itself, `work + 0x14` and
/// `work + 0x334`. `field_474` is the live flag that setter raises once it has
/// run the slots -- `func_actor_135400_801322A8` only ticks them while it is
/// set -- and `field_475` / `field_476` are the two bytes it latches out of the
/// animation request: `field_476` indexes `D_actor_135400_8013A4A8` for the
/// load and `field_475` is passed on as the slot functions' third argument.
///
/// `field_4B8` / `field_4BC` are the two part tasks the same spawn creates
/// through `Task_SpawnFromTable` (part 1 and part 2), each of which reparents
/// itself onto this task in `func_actor_135400_80132450`. `headAim` is the
/// on/off latch the command handler `func_actor_135400_801328DC` sets and
/// clears (its cases 2 and 3), and `headRate` the 0x000..0xFFF ramp toward the
/// slot-3 skeleton's head `func_800B0928` steps by. Only the fields decompiled
/// bodies reach are described.
typedef struct Actor135400MainWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       pad_334[0x140];
    /* 0x474 */ s8         field_474;
    /* 0x475 */ s8         field_475;
    /* 0x476 */ s8         field_476;
    /* 0x477 */ byte       pad_477[0x41];
    /* 0x4B8 */ Task*      field_4B8;
    /* 0x4BC */ Task*      field_4BC;
    /* 0x4C0 */ s32        headAim;
    /* 0x4C4 */ s32        headRate;
} Actor135400MainWork;
STATIC_ASSERT_SIZEOF(Actor135400MainWork, 0x4C8);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor205200Msg7DB` / `Actor342400Msg7DB`, whose
/// halfword at 0x2 is the only part this overlay's handler reads.
typedef struct Actor135400Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor135400Msg7DB;
STATIC_ASSERT_SIZEOF(Actor135400Msg7DB, 0x4);

/// Message 0x7DB handler, listed in the work block's `D_actor_135400_8013A4D0`
/// after the 0x7D3 / 0x7D4 / 0x7D5 ones. The payload halfword picks one of six
/// actions against the actor's part-task 2 (`Actor135400MainWork::field_4BC`):
/// 0 and 1 clear and raise bit 0x80 of that task's model `field_C` -- the drawn
/// flag -- 2 and 3 set and clear `headAim`, 4 hands the part task a
/// `spawnArg1` of 1, and 5 sets that to 3 and then clears the flag. Nothing
/// reads the opcode itself, hence `msgId`.
s32 func_actor_135400_801328DC(Task* task, s32 msgId, Actor135400Msg7DB* msg, s32 arg3);

/// The actors' ground shadow (`src/gameplay/3E9C.c`): `arg0` is the point the
/// quad is centred on, `arg1` its size -- the corner table is scaled by it --
/// and `arg2` the shade, negative to skip the draw. Declared per overlay, as
/// the sibling packages do.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// The two placements `func_actor_135400_80132064` starts the actor from, in
/// the `.rodata` at `D_actor_135400_80131E48`: a world translation followed by
/// the Euler angles handed to `RotMatrix`, the same block the 0x7D4 opcode
/// takes. The spawn copies the pair in one go and then hands the branch picked
/// by game flag 0x6C to `func_actor_135400_8013276C`.
typedef struct Actor135400Places {
    /* 0x00 */ ActorsShared8013231cArgs field_0;
    /* 0x18 */ ActorsShared8013231cArgs field_18;
} Actor135400Places;
STATIC_ASSERT_SIZEOF(Actor135400Places, 0x30);

/// The two placements, defined in `actor_135400.c` so they land in that unit's
/// `.rodata` right after the state tables.
extern const Actor135400Places D_actor_135400_80131E48;

/// The actor's two-entry `TaskDesc` table, indexed by `Task_SpawnFromTable`:
/// entry 1 is the model-bearing part task `func_actor_135400_80132450`
/// reparents, entry 2 its second part.
extern TaskDesc D_actor_135400_8013A4AC;

/// The handler table `func_actor_135400_80132064` parks in `Task::field_24`
/// (0x24): the 0x7D3 / 0x7D4 / 0x7D5 / 0x7DB bodies of `actor_135400_2`.
extern s32 D_actor_135400_8013A4D0;

/// Per-frame animation setter for the 0x7D3 opcode: plays the animation the
/// `GpAnimArg` names across the work block's twenty slots and latches
/// `field_474`. Built by `actor_135400_2`.
s32 func_actor_135400_80132650(Task* task, s32 anim, GpAnimArg* params, s32 arg3);

/// The 0x7D4 opcode's body: installs its argument block's translation and
/// Euler angles onto the root part's coordinate. Byte-identical to
/// `ActorsShared8013231c`, which the other packages link; built by
/// `actor_135400_2`.
s32 func_actor_135400_8013276C(Task* task, s32 anim, ActorsShared8013231cArgs* args, s32 arg3);

/// `Gp_DispatchMsg` handler for message 0x7D5: the 4-way model-mode switch
/// that keeps the part task's model in step. Built by `actor_135400_2`.
s32 func_actor_135400_801327E8(Task* task, s32 msgId, s32 mode, s32 arg3);

/// The main task's exit callback; runs the actor's teardown.
void func_actor_135400_80132614(Task* task);

/// Main-executable helper the spawn runs on the flag-clear path, once the
/// actor is placed. Unmatched, so declared here as the sibling packages do.
void func_80180414(s32 arg0);

#endif
