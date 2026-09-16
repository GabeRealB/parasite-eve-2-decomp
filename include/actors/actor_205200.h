#ifndef ACTOR_205200_H
#define ACTOR_205200_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "main/task.h"

typedef struct Actor205200Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
    /* 0x0C */ s16            field_C;
} Actor205200Obj2C;

/// Work block this overlay hangs off `Actor205200.field_1C` (the task's
/// `Task::idMap` slot, which is not a `TaskIdMap` here). The two display nodes
/// at +0x47C and +0x4E4 are the ones the exit callback
/// `func_actor_205200_8014C924` hands back to `Gp_UnlinkObj`. `field_74` is the
/// effect timer: `func_actor_205200_8014B9D4` only tests it, as the signed
/// halfword it is, while `func_actor_205200_8014BA94` counts it down through the
/// family's unsigned `(u16)` view.
typedef struct Actor205200Work {
    /* 0x000 */ byte     pad_0[0x2E];
    /* 0x02E */ s16      field_2E;
    /* 0x030 */ byte     pad_30[0x38];
    /* 0x068 */ GpEffArg field_68; // `func_800FDB18` argument record
    /* 0x070 */ byte     pad_70[0x2];
    /* 0x072 */ s16      field_72; // raised to 2 with `Task::state` when the parent's 0x7DB flag is set
    /* 0x074 */ s16      field_74; // effect timer, reloaded every 0x40 ticks
    /* 0x076 */ byte     pad_76[0x406];
    /* 0x47C */ GpObj    field_47C;
    /* 0x49C */ byte     pad_49C[0x48];
    /* 0x4E4 */ GpObj    field_4E4;
    /* 0x504 */ byte     pad_504[0x80];
    /* 0x584 */ s16      field_584; // sub-state `func_actor_205200_8014C67C` dispatches on: 0 runs the idle handler, 1 the charge handler
    /* 0x586 */ s16      field_586; // sub-state of the charge handler `func_actor_205200_8014C748`, which arms it to 1 and clears it again
    /* 0x588 */ s16      field_588; // non-zero while the attack body `func_actor_205200_8014C0C0` is running; the body clears it when it finishes
    /* 0x58A */ byte     pad_58A[0xA];
    /* 0x594 */ s16      field_594;
    /* 0x596 */ s16      field_596; // selects the shared tick `func_actor_205200_8014C67C` runs: zero goes to `func_8017EBA4`, non-zero to `func_80181930`
} Actor205200Work;

/// The task itself, named for the actor it drives. `field_8` is `Task::parent`
/// - the actor whose work block keeps the 0x7DB flag `field_2E`.
typedef struct Actor205200 {
    /* 0x00 */ byte              pad_0[0x8];
    /* 0x08 */ struct _Task*     field_8;
    /* 0x0C */ byte              pad_C[0x10];
    /* 0x1C */ Actor205200Work*  field_1C;
    /* 0x20 */ byte              pad_20[0xC];
    /* 0x2C */ Actor205200Obj2C* field_2C;
    /* 0x30 */ s32               field_30;
} Actor205200;

/// Owning context. The update entry point does not touch it, but the exit
/// callback `func_actor_205200_8014C924` unlinks the `GpLinkNode` at +0x10.
typedef struct Actor205200Ctx {
    /* 0x00 */ byte       pad_0[0x10];
    /* 0x10 */ GpLinkNode node;
} Actor205200Ctx;

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor342400Msg7DB`, whose halfword at 0x2 is the
/// only part the handler below reads. Senders seed it from a `Task`'s
/// `spawnArg1` halfword -- `Gp_DispatchMsg` in `3CD8.c` sends `D_801155A0`.
typedef struct Actor205200Msg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} Actor205200Msg7DB;
STATIC_ASSERT_SIZEOF(Actor205200Msg7DB, 0x4);

/// Message 0x7DB handler, listed in the overlay's `D_actor_205200_801567D0`
/// next to the shared 0x7D5 one. A non-zero payload halfword sets
/// `Actor205200Work.field_594`, the flag `func_actor_205200_8014C59C` tests to
/// push the actor to state 2. Nothing reads the opcode itself, hence `arg1`.
s32 func_actor_205200_8014C9A0(Actor205200* arg0, s32 arg1, Actor205200Msg7DB* arg2);

/// The other 0x7DB handler, listed in `D_actor_205200_8014CA78` (the table at
/// 0x801567D0 routes the same opcode to `func_actor_205200_8014C9A0`). Same
/// payload, different flag: a non-zero halfword raises
/// `Actor205200Work.field_2E` unless it is already set.
s32 func_actor_205200_8014B94C(Actor205200* arg0, s32 arg1, Actor205200Msg7DB* arg2);

void func_actor_205200_8014C59C(Actor205200Ctx* arg0, Actor205200* arg1);
void func_actor_205200_8014C924(Actor205200Ctx* arg0, Actor205200* arg1);

/// The live states' shared body: moves and draws the actor. `arg1` is the value
/// the state handlers pass through (`1` from `func_actor_205200_8014B9D4`).
void func_actor_205200_8014B048(Actor205200* arg0, s32 arg1);

/// Counts the effect timer down and queues effect 7 every 0x40 ticks.
void func_actor_205200_8014BA94(Actor205200* arg0);

#endif
