#ifndef ACTORS_SHARED_80138EFC_H
#define ACTORS_SHARED_80138EFC_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

/// Motion sub-object at 0x8C of `ActorsShared80138efcWork`, ending at the id
/// byte at 0xB88 that follows it. Fields at 0x02 / 0x06 are read as a pair by
/// `func_actor_104900_80136F8C`, and the flag word at 0x10 is tested a bit at a
/// time by the dispatcher and by the handlers.
typedef struct ActorsShared80138efcMotion {
    /* 0x000 */ u16  field_0;
    /* 0x002 */ u16  field_2;
    /* 0x004 */ u16  field_4;
    /* 0x006 */ u16  field_6;
    /* 0x008 */ byte pad_8[0x8];
    /// Bit 0 arms `field_BA9` in `func_actor_104900_80134780`; bit 1 is the one
    /// the body at 0x80138E34 tests before it arms its next state.
    /* 0x010 */ u16  flags;
    /* 0x012 */ byte pad_12[0x92A];
    /// Two `GpObj` list nodes. The 0x80138A2C body masks the 0xC000 pair out of
    /// both `flags`; `func_actor_104900_801366E8` ORs it back into the second on
    /// the frame `field_B8C` reaches 0x23, next to a `Gp_PackObjPair` result in
    /// `field_18`.
    /* 0x93C */ GpObj objs[2];
    /* 0x97C */ byte  pad_97C[0x180];
} ActorsShared80138efcMotion;
STATIC_ASSERT_SIZEOF(ActorsShared80138efcMotion, 0xAFC);

/// Work block the body at 0x80138EFC is handed in `$a2`, as five actor slots
/// lay it out.
///
/// The dispatcher `func_actor_104900_80134780` copies its 26-entry handler
/// table onto the stack and then calls `table[work->state](enemy, task, work)`,
/// reading the index with `lb` from 0xBA7 - which is why that field is named
/// rather than numbered. The same slot allocates the block with
/// `Mem_Calloc(0xBCC, 0)` in `func_actor_104900_8013279C` and parks it in
/// `Task::idMap` (0x1C), so the size below is that allocation rather than a
/// guess; other slots carry the body with a differently sized block, and only
/// the fields this handler touches are laid out here.
///
/// The siblings in the same unit reach into the same run: `func_actor_104900_80138B5C`
/// reads 0xBA0 as a byte and 0xB94 / 0xB96 as halfwords, `func_actor_104900_80138E34`
/// and `func_actor_104900_80138F68` use 0xB9C..0xBAE the way this one does, and
/// `func_actor_104900_80138D58` stores a halfword at 0xB8C.
typedef struct ActorsShared80138efcWork {
    /* 0x000 */ byte pad_0[0x8C];
    /// Motion sub-object embedded at 0x8C. Its halfwords at 0x00 / 0x02 / 0x06
    /// are the ones `func_actor_104900_80136F8C` compares against the motion id
    /// in `field_BA4` and walks, and its flag word at 0x10 (0x9C absolute) is
    /// what the dispatcher reads bit 0 of to arm `field_BA9`. Only that much of
    /// it is laid out; the body at 0x80138E34 tests bit 1 of the same word, and
    /// keeps the base in a register rather than folding 0x9C into the access.
    /* 0x08C */ ActorsShared80138efcMotion motion;
    /// Actor id, `spawnArg->field_8 >> 12`, which the slot's setup body at
    /// 0x8013279C stores as a word and mirrors into `D_actor_104900_80147490`.
    /// The 0x80138D58 handler reads its low byte and shifts it into bits 8..15
    /// of the `SndEvt_EnqueueType6` id.
    /* 0xB88 */ u8   field_B88;
    /* 0xB89 */ byte pad_B89[0x3];
    /// Countdown a state arms and decrements per frame: `func_actor_104900_80138D58`
    /// posts 0x64 into it and acts when it reaches zero, and this unit's
    /// 0x80138E34 arms 0xA.
    /* 0xB8C */ s16 field_B8C;
    /// Walking offset `func_actor_104900_80138B5C` steps 0x30 back toward zero
    /// from either end of the +-0x30 band, one frame at a time.
    /* 0xB8E */ s16  field_B8E;
    /* 0xB90 */ byte pad_B90[0x2];
    /* 0xB92 */ s16  field_B92;
    /// Decay counters the 0x80138B5C body subtracts from - 0x400 for the axis
    /// pair and 0x100 for the next two - clamping each at zero.
    /* 0xB94 */ s16  field_B94;
    /* 0xB96 */ s16  field_B96;
    /* 0xB98 */ s16  field_B98;
    /* 0xB9A */ s16  field_B9A;
    /* 0xB9C */ s16  field_B9C;
    /* 0xB9E */ byte pad_B9E[0x6];
    /// Motion id armed for the frame; every sibling writes a different pair
    /// here (0xB/0xE here, 0x15/0x16 next door, 5 in the setup handler).
    /* 0xBA4 */ s8 field_BA4;
    /// Set alongside `field_BA4` to ask for the motion to be restarted.
    /* 0xBA5 */ s8 field_BA5;
    /// Set when the trigger at `field_BA9` fires.
    /* 0xBA6 */ s8 field_BA6;
    /// Index into the dispatcher's 26-entry handler table, read there with
    /// `lb` and multiplied by 4.
    /* 0xBA7 */ s8 state;
    /// Run-once latch: 0 means the state has not started yet. Read signed for
    /// the test but re-read unsigned for the increment, hence the `(u8)` cast
    /// at the one place it is stepped.
    /* 0xBA8 */ s8 field_BA8;
    /// Trigger this handler consumes: nonzero sets `field_BA6`, resets the
    /// countdown at `field_B9C` and selects state 0xF.
    /* 0xBA9 */ s8   field_BA9;
    /* 0xBAA */ byte pad_BAA[0x1];
    /// Compared against 1 (`lbu`) by the 0x80138B5C body, which skips its whole
    /// decay block while it is set.
    /* 0xBAB */ u8   field_BAB;
    /* 0xBAC */ byte pad_BAC[0x2];
    /* 0xBAE */ u8   field_BAE;
    /// Armed alongside `state` by the 0x80138E34 body, which the dispatcher's
    /// trigger then compares against. The 0x80138B5C body gates the `field_B8E`
    /// step on it (`lbu`).
    /* 0xBAF */ u8   field_BAF;
    /* 0xBB0 */ byte pad_BB0[0x8];
    /// Sound variant bit the slot's setup body at 0x8013279C picks from the
    /// spawn record, 0 or 1. `func_actor_104900_80138D58` and the bodies at
    /// 0x80132D78 / 0x80136230 shift it into bit 22 of the id they hand
    /// `SndEvt_EnqueueType6`.
    /* 0xBB8 */ u8   field_BB8;
    /* 0xBB9 */ byte pad_BB9[0x10];
    /// Read as a byte and compared against 1, then against `field_BA9`: the
    /// 0x80138A2C body only runs its restart path when both are 1.
    /* 0xBC9 */ u8   field_BC9;
    /* 0xBCA */ byte pad_BCA[0x2];
} ActorsShared80138efcWork;
STATIC_ASSERT_SIZEOF(ActorsShared80138efcWork, 0xBCC);

/// Block `func_actor_104900_80134780` hands every state handler in `$a3`, one
/// frame of the actor's own stack. The dispatcher fills `pan` and `depth` from
/// the model's `GpObj38` (0x50 into `Task::field_2C->field_8`) right before the
/// indirect call, storing each as a halfword; the handlers read the low byte,
/// so the pair is laid out as bytes here. Which of them writes what is per
/// handler - the 0x80138D58 body writes only 0x64.
typedef struct ActorsShared80138efcArg {
    /* 0x00 */ byte pad_0[0x60];
    /* 0x60 */ s8   pan;
    /* 0x61 */ byte pad_61[0x1];
    /* 0x62 */ s8   depth;
    /* 0x63 */ byte pad_63[0x1];
    /* 0x64 */ s8   field_64;
} ActorsShared80138efcArg;
STATIC_ASSERT_SIZEOF(ActorsShared80138efcArg, 0x65);

/// Arms the motion pair for the current sub-state when `field_BA8` is still
/// clear, and switches to state 0xF when `field_BA9` is set.
void ActorsShared80138efc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);

#endif // ACTORS_SHARED_80138EFC_H
