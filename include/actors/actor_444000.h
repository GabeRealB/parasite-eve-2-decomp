#ifndef ACTOR_444000_H
#define ACTOR_444000_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Per-actor work block for the enemy task `D_actor_444000_80161878` points
/// at, reached through the `Task::idMap` slot (0x1C) rather than being a
/// `TaskIdMap` here.
///
/// `func_actor_444000_8013AFF8` allocates it with `Mem_Calloc(0xF24, 0)` and
/// stores the result in that slot, so the size below is the allocation rather
/// than a guess. `field_0` is the leading state word, which
/// `func_actor_444000_80143D7C` reads with `lhu` and range-checks and the
/// 0x7D9 message handler `func_actor_444000_80143F38` clears;
/// `func_actor_444000_80143490` writes the byte at 0xEAC. The remaining named
/// fields are the block `func_actor_444000_801435CC` re-arms when `field_4` is
/// set. Fill in the padding as the remaining functions are matched.
///
/// 0xC..0x784 is six back-to-back animation blocks, each a `GpAnimCtx` followed
/// by its own `GpAnimSlot[N]` and an N-entry 0x10-byte table -- the three
/// argument groups `func_actor_444000_8013AFF8` hands to `func_800B3F84`, which
/// is what anchors every offset here. They pair up (0/1, 2/3, 4/5) with eight
/// slots in the first pair and four in the others;
/// `func_actor_444000_80133F64` seeds the even member's slots while resetting
/// the odd member's.
typedef struct Actor444000Work {
    /* 0x000 */ u16        field_0; // state index
    /* 0x002 */ byte       pad_2[0x2];
    /* 0x004 */ s16        field_4; // reset request: non-zero makes func_actor_444000_801435CC re-arm the block
    /* 0x006 */ s16        field_6; // sub-state counter, cleared by that reset and compared against 0xA
    /* 0x008 */ byte       pad_8[0x4];
    /* 0x00C */ GpAnimCtx  anim0;
    /* 0x020 */ GpAnimSlot slots0[8];
    /* 0x160 */ byte       aux0[0x80]; // GpAnimCtx::field_8, one 0x10-byte record per slot
    /* 0x1E0 */ GpAnimCtx  anim1;
    /* 0x1F4 */ GpAnimSlot slots1[8];
    /* 0x334 */ byte       aux1[0x80];
    /* 0x3B4 */ GpAnimCtx  anim2;
    /* 0x3C8 */ GpAnimSlot slots2[4];
    /* 0x468 */ byte       aux2[0x40];
    /* 0x4A8 */ GpAnimCtx  anim3;
    /* 0x4BC */ GpAnimSlot slots3[4];
    /* 0x55C */ byte       aux3[0x40];
    /* 0x59C */ GpAnimCtx  anim4;
    /* 0x5B0 */ GpAnimSlot slots4[4];
    /* 0x650 */ byte       aux4[0x40];
    /* 0x690 */ GpAnimCtx  anim5;
    /* 0x6A4 */ GpAnimSlot slots5[4];
    /* 0x744 */ byte       aux5[0x40];
    /* 0x784 */ byte       pad_784[0x2C];
    /* 0x7B0 */ s8         field_7B0;
    /* 0x7B1 */ byte       pad_7B1[0x2];
    /* 0x7B3 */ s8         field_7B3;
    /* 0x7B4 */ byte       pad_7B4[0x2];
    /* 0x7B6 */ s16        field_7B6;
    /* 0x7B8 */ byte       pad_7B8[0x4];
    /* 0x7BC */ s16        field_7BC; // animation id the slot resets seed from
    /* 0x7BE */ s16        field_7BE; // GpAnimSlot::field_9 the resets seed with
    /* 0x7C0 */ s16        field_7C0;
    /* 0x7C2 */ byte       pad_7C2[0x6EA];
    /* 0xEAC */ s8         field_EAC;
    /* 0xEAD */ byte       pad_EAD[0x47];
    /* 0xEF4 */ s16        field_EF4;
    /* 0xEF6 */ s16        field_EF6;
    /* 0xEF8 */ byte       pad_EF8[0x2];
    /* 0xEFA */ s16        field_EFA;
    /* 0xEFC */ byte       pad_EFC[0x2];
    /* 0xEFE */ s16        field_EFE;
    /* 0xF00 */ byte       pad_F00[0x24];
} Actor444000Work;
STATIC_ASSERT_SIZEOF(Actor444000Work, 0xF24);

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_444000_80161860` points at, which is a different and much smaller
/// block than the enemy's `Actor444000Work` above.
///
/// `func_actor_444000_80132358` allocates it with `Mem_Calloc(0x34, 0)`,
/// `Mem_Set`s 0x34 bytes and parks it in that task's `Task::idMap` slot, so
/// the size is anchored; the same function stores the `Game_GetPtrSlot(3)`
/// task in `field_20` and publishes its owning task in
/// `D_actor_444000_80161860`. `field_20` is the target of every
/// `Gp_DispatchMsg` the leaf helpers send, and they null-check it first
/// (`func_actor_444000_801321FC`). `field_2C` is the action index
/// `func_actor_444000_80132054` switches on, with `field_2E` the sub-state
/// counter reset alongside it. `field_2A` is a one-shot flag guarding the sound
/// cue `func_actor_444000_80132608` enqueues.
typedef struct Actor444000EventWork {
    /* 0x00 */ byte  pad_0[0x20];
    /* 0x20 */ Task* field_20; // Game_GetPtrSlot(3) task, the Gp_DispatchMsg target
    /* 0x24 */ Task* field_24; // subordinate task, killed and cleared by func_actor_444000_80132694
    /* 0x28 */ byte  pad_28[0x2];
    /* 0x2A */ u16   field_2A; // one-shot flag: set once func_actor_444000_80132608 has played its cue
    /* 0x2C */ u16   field_2C; // action index, switched on by func_actor_444000_80132054
    /* 0x2E */ s16   field_2E; // cleared whenever field_2C is set
    /* 0x30 */ u16   field_30; // one-shot flag: set once func_actor_444000_80132778 has armed the death sequence
    /* 0x32 */ byte  pad_32[0x2];
} Actor444000EventWork;
STATIC_ASSERT_SIZEOF(Actor444000EventWork, 0x34);

/// Body/collision object the actor task carries at +0x20 (the `Task::spawnArg2`
/// slot). The halfword at 0x40 is the remaining HP, tested for `> 0` by
/// `func_actor_444000_80143D68` and topped back up by
/// `func_actor_444000_80143E68`; `field_8` and `field_14` are the two slots
/// `func_actor_444000_801435CC` touches.
typedef struct Actor444000Obj {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ u16  field_8; // high nibble selects the sound-event variant
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ s8   field_14;
    /* 0x15 */ byte pad_15[0x2B];
    /* 0x40 */ s16  field_40; // remaining HP
} Actor444000Obj;

/// Payload `func_actor_444000_801326DC` passes as `Gp_DispatchMsg`'s `arg2`
/// for message 0x7DA, which the slot-4 task forwards to the 0x7DB handlers.
/// The same four bytes as `AcropolisBridgeMsg7DA`: two id bytes followed by a
/// halfword the receiver switches on.
typedef struct Actor444000Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor444000Msg7DA;
STATIC_ASSERT_SIZEOF(Actor444000Msg7DA, 0x4);

/// The overlay's enemy task: the same layout as `Task`, named for the two
/// slots this overlay reaches through it. Not the event task
/// `D_actor_444000_80161860`, whose `idMap` holds an `Actor444000EventWork`.
typedef struct Actor444000 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor444000Work* field_1C;
    /* 0x20 */ Actor444000Obj*  field_20;
    /* 0x24 */ byte             pad_24[0x8];
    /* 0x2C */ void*            extra; // Task::extra, a TmdObject
} Actor444000;

s32 func_actor_444000_80143D68(Actor444000* arg0);
s32 func_actor_444000_80143F38(Actor444000* arg0);

#endif
