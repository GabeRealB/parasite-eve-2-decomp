#ifndef ACTOR_403900_H
#define ACTOR_403900_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

/// Per-instance work block the actor's task holds in the 0x1C slot of
/// `Actor403900`. Only the fields the overlay's cue body
/// `func_actor_403900_801347F4` reaches are spelled out so far; the block is
/// 0x71C bytes, the same shape the other enemy overlays give their work
/// blocks.
///
/// `field_6C0` is the animation id the cue body's state 0 arms, `field_6CE`
/// is the cue state itself, and `field_6D4` is the countdown state 1 ticks
/// down: it is rolled from the `Gp_LcgState` LCG when the animation is
/// reseeded, and the frame it runs out queues the actor's cue.
///
/// `field_6C6` is the flinch countdown the hit handler arms when a hit lands.
/// While it is non-zero a hit is already being flinched, so the cue body
/// raises the hit descriptor `field_494` and the pending flags `field_49A`
/// only when it is zero - the same pair the hurt states store directly.
typedef struct Actor403900Work {
    /* 0x000 */ byte pad_0[0x494];
    /// Hit descriptor: the damage amount `field_716` with the tag bits
    /// 0x30000 OR'd in.
    /* 0x494 */ s32 field_494;
    /* 0x498 */ s16 field_498;
    /// Hit-pending flags, raised together with `field_494`; bit 0x8000 is the
    /// flag the hit handler clears when it consumes the descriptor.
    /* 0x49A */ u16  field_49A;
    /* 0x49C */ byte pad_49C[0x11E];
    /// Flag word the settle states 3 and 4 clear bit 0x4000 of, beside the
    /// matching word at 0x5DA.
    /* 0x5BA */ u16  field_5BA;
    /* 0x5BC */ byte pad_5BC[0x1E];
    /// Second flag word, cleared by state 3 only.
    /* 0x5DA */ u16  field_5DA;
    /* 0x5DC */ byte pad_5DC[0x18];
    /// Head of the actor's first collision-record table; the settle states
    /// branch on its `field_4` before handing the table to
    /// `Gp_ClearRec18Occupied`.
    /* 0x5F4 */ GpRec18 field_5F4;
    /* 0x60C */ byte    pad_60C[0x38];
    /// Head of a second collision-record table, cleared by state 5.
    /* 0x644 */ GpRec18 field_644;
    /* 0x65C */ byte    pad_65C[0x60];
    /// Sound event id the cue body queues: the overlay's cue word
    /// `D_actor_403900_8013846C` with the `GpEnemy` work id's high nibble in
    /// bits 8-11. Stored back to the block and re-read from there as the first
    /// argument of `SndEvt_EnqueueType6`.
    /* 0x6BC */ s32 field_6BC;
    /// Animation id the cue reseeds at.
    /* 0x6C0 */ s16  field_6C0;
    /* 0x6C2 */ byte pad_6C2[4];
    /// Flinch countdown, armed by the hit handler and ticked down a frame at a
    /// time; see the block comment.
    /* 0x6C6 */ s16 field_6C6;
    /// Cleared on the frame the wait state rolls a new countdown.
    /* 0x6C8 */ s16  field_6C8;
    /* 0x6CA */ byte pad_6CA[2];
    /// Cleared when the cue expires.
    /* 0x6CC */ s16 field_6CC;
    /// Cue state: 0 arms the animation and the countdown, 1 waits the
    /// countdown out and fires the cue, then drops back to 0.
    /* 0x6CE */ s16 field_6CE;
    /* 0x6D0 */ s16 field_6D0;
    /* 0x6D2 */ s16 field_6D2;
    /// Countdown the reseed rolls from the LCG (0x2D..0x4C) and state 1 ticks
    /// down; read back as an unsigned halfword and sign-extended through a
    /// 16-bit shift, so it runs out on the frame it passes 0xFFFF.
    /* 0x6D4 */ u16 field_6D4;
    /* 0x6D6 */ s16 field_6D6;
    /* 0x6D8 */ s16 field_6D8;
    /// Timer pair the reseed arms alongside `field_6DE`.
    /* 0x6DA */ s16 field_6DA;
    /* 0x6DC */ s16 field_6DC;
    /// Third timer the reseed arms; written last of the three.
    /* 0x6DE */ s16 field_6DE;
    /// Fourth timer, cleared alongside the trio above when the countdown runs
    /// out.
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
    /// Latch that sends the wait state straight to state 2 with no countdown;
    /// cleared again on the frame it is taken.
    /* 0x6E4 */ s16  field_6E4;
    /* 0x6E6 */ byte pad_6E6[2];
    /// Latch that sends the wait state to an LCG-drawn state 3-5 offset and
    /// reposts the actor's target instead of rolling a countdown.
    /* 0x6E8 */ s16  field_6E8;
    /* 0x6EA */ byte pad_6EA[2];
    /// Sequence mode, cleared by the wait state.
    /* 0x6EC */ s16 field_6EC;
    /// Set by the wait state when it takes the `field_6E8` branch, cleared
    /// otherwise.
    /* 0x6EE */ s16  field_6EE;
    /* 0x6F0 */ byte pad_6F0[0x1C];
    /// Escalation counter the settle states walk up to 8; the wait state's
    /// countdown is scaled by `16 - field_70C`, so a higher count is a
    /// shorter wait.
    /* 0x70C */ s16 field_70C;
    /// Which settle state last ran (1, 2 or 3); state 2 picks the next state
    /// from it.
    /* 0x70E */ s16 field_70E;
    /// Run length of repeats of the same settle state; state 2 biases its LCG
    /// draw by it and the settle states walk it back down.
    /* 0x710 */ s16  field_710;
    /* 0x712 */ byte pad_712[4];
    /// Damage amount the hit handlers OR into `field_494`.
    /* 0x716 */ s16  field_716;
    /* 0x718 */ byte pad_718[4];
} Actor403900Work;
STATIC_ASSERT_SIZEOF(Actor403900Work, 0x71C);

/// Display object hung off `Actor403900::field_2C`; `field_8` is the actor's
/// per-part attach coordinate array, which the sound helpers take as the
/// `GpObj38*` light object they project.
typedef struct Actor403900Obj2C {
    /* 0x00 */ byte     pad_0[8];
    /* 0x08 */ GpObj38* field_8;
} Actor403900Obj2C;

/// Actor context handed to this overlay's callbacks: `field_1C` is the work
/// block above, `field_20` the `GpEnemy` the spawner left in the task's
/// `Task::spawnArg2` slot, and `field_2C` the display object. Same shape as
/// the other actor overlays' contexts.
typedef struct Actor403900 {
    /* 0x00 */ byte              pad_0[0x1C];
    /* 0x1C */ Actor403900Work*  field_1C;
    /* 0x20 */ GpEnemy*          field_20;
    /* 0x24 */ byte              pad_24[8];
    /* 0x2C */ Actor403900Obj2C* field_2C;
} Actor403900;

/// The game's shared 32-bit LCG state: every draw is
/// `Gp_LcgState = Gp_LcgState * 5 + 0x71357911`, read back from the global,
/// with the caller taking the bits it wants out of the high half.
extern u32 Gp_LcgState;

/// Parks the actor's target position off the player; see its definition.
void func_actor_403900_80132E34(Actor403900* arg0);

#endif /* ACTOR_403900_H */
