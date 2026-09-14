#ifndef ACTOR_403200_H
#define ACTOR_403200_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

typedef struct Actor403200Obj Actor403200Obj;

/// Per-actor state block for the `actor_403200` overlay.
///
/// `func_actor_403200_80138AFC` allocates it with `Mem_Calloc(0xF24, 0)` and
/// stores the result in the `Task::idMap` slot (0x1C), which this enemy actor
/// reuses for its own work block, so it is *not* a `TaskIdMap` here. Reach it
/// with `(Actor403200Work*)task->idMap`. The size below is the allocation, not
/// a guess.
typedef struct Actor403200Work {
    /// State index. `func_actor_403200_8013FB54` indexes the local copy of
    /// `D_actor_403200_80132154` with it, and whenever it differs from
    /// `field_2` it flags the change in `field_4` and re-arms the `field_6`
    /// counter. The same three fields and that same test appear in the sibling
    /// actor overlays that share this dispatcher.
    /* 0x000 */ s16 field_0;
    /// The state index `func_actor_403200_8013FB54` ran on the previous tick,
    /// so it can spot the change.
    /* 0x002 */ s16 field_2;
    /// Set on the tick the dispatcher sees a state change, cleared on every
    /// other tick. `func_actor_403200_8014123C` reads it to re-arm `field_6`
    /// once more, which the dispatcher has already done.
    /* 0x004 */ s16 field_4;
    /// Per-state counter: cleared on a state change, otherwise incremented
    /// (saturating at 0x7FFF). State handlers fire one-shot cues on the ticks
    /// it reaches a given value.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[0x50];
    /// The trailing halfword of the animation slot the per-frame body tests for
    /// the `field_0 = 0xA` re-arm. Same slot as
    /// `Actor444000Work::slots0[1].field_10`.
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x74A];
    /// Escort pose index, written 3 by the re-arm path of the per-frame body
    /// and cleared once the shared countdown below has run out. Same slot and
    /// role as `Actor444000Work::field_7A4`.
    /* 0x7A4 */ s16  field_7A4;
    /* 0x7A6 */ byte pad_7A6[0xA];
    /* 0x7B0 */ s8   field_7B0;
    /* 0x7B1 */ byte pad_7B1[0x2];
    /* 0x7B3 */ s8   field_7B3;
    /* 0x7B4 */ byte pad_7B4[0x3F];
    /// Cleared by the state-change reset to mark the work block as re-armed.
    /// Same slot and role as `Actor444000Work::field_7F3`.
    /* 0x7F3 */ u8   field_7F3;
    /* 0x7F4 */ byte pad_7F4[0x6A0];
    /// Yaw the upkeep tick walks toward `field_E96` in steps of 0x32, snapping
    /// once the two are within 0x33 of each other. Same slot and role as
    /// `Actor444000Work::field_E94`.
    /* 0xE94 */ s16 field_E94;
    /// Yaw target the re-arm path arms to 0xC80. Same slot and role as
    /// `Actor444000Work::field_E96`.
    /* 0xE96 */ s16 field_E96;
    /// Companion value handed to the follow helper alongside `field_E94`. Same
    /// slot and role as `Actor444000Work::field_E98`.
    /* 0xE98 */ s16  field_E98;
    /* 0xE9A */ byte pad_E9A[0x12];
    /// Screen-shake level `func_actor_403200_80138284` drives, and the level
    /// armed last tick in `field_EAD`; a change from the armed level starts a
    /// shake. Same slots, types and role as `Actor444000Work::field_EAC`.
    /* 0xEAC */ u8   field_EAC;
    /* 0xEAD */ u8   field_EAD;
    /* 0xEAE */ u8   field_EAE;
    /* 0xEAF */ s8   field_EAF;
    /* 0xEB0 */ byte pad_EB0[0x1C];
    /// The escorts the state-change reset walks to push the host's
    /// `TmdObject::field_C` onto each escort's own model object; the same
    /// seven-slot run as `Actor444000Work::field_ECC`.
    /* 0xECC */ GpEnemy* field_ECC[7];
    /// The two escorts the upkeep tick drops once their HP has run out, by
    /// clearing the slot when `GpEnemy::field_40` is not positive. Same slots
    /// and role as `Actor444000Work::field_EE8`.
    /* 0xEE8 */ GpEnemy* field_EE8;
    /* 0xEEC */ GpEnemy* field_EEC;
    /* 0xEF0 */ byte     pad_EF0[0x4];
    /* 0xEF4 */ s16      field_EF4;
    /* 0xEF6 */ s16      field_EF6;
    /* 0xEF8 */ byte     pad_EF8[0x2];
    /// Armed to 1 by the per-frame body's re-arm path. Same slot and role as
    /// `Actor444000Work::field_EFA`.
    /* 0xEFA */ s16  field_EFA;
    /* 0xEFC */ byte pad_EFC[0xA];
    /// Cleared by the per-frame body once `field_6` has passed 0x14. Same slot
    /// and role as `Actor444000Work::field_F06`.
    /* 0xF06 */ s16  field_F06;
    /* 0xF08 */ byte pad_F08[0xE];
    /// Re-armed to 2 by the upkeep handler `func_actor_403200_80141A94` once
    /// the `field_F1C` countdown has run out. Same slot and role as
    /// `Actor444000Work::field_F16`.
    /* 0xF16 */ s16  field_F16;
    /* 0xF18 */ byte pad_F18[0x2];
    /// Free-running counter bumped on every heal tick by
    /// `func_actor_403200_80141A94`.
    /* 0xF1A */ u8   field_F1A;
    /* 0xF1B */ byte pad_F1B[0x1];
    /// Countdown, decremented while positive; when it reaches zero the handler
    /// re-arms `field_F16`.
    /* 0xF1C */ s8   field_F1C;
    /* 0xF1D */ byte pad_F1D[0x7];
} Actor403200Work;
STATIC_ASSERT_SIZEOF(Actor403200Work, 0xF24);

s16 func_actor_403200_801344C4(Actor403200Obj* arg0, s16 arg1);

/// The actor's per-frame body: runs the animation resets and the collision /
/// damage ticks. Takes the task, and reaches the work block through its
/// `idMap` slot, as `func_actor_403200_8014123C` does.
void func_actor_403200_80133DD8(Task* task);

#endif
