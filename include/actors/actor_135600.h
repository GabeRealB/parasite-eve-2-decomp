#ifndef ACTOR_135600_H
#define ACTOR_135600_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/gfx.h"
#include "main/task.h"

/// Work block of the `actor_135600` enemy task, the `memCalloc(0x50C, 0)`
/// result `func_actor_135600_80132234` parks in `Task::work` -- that slot is
/// not a `TaskIdMap` here. The setup handler seeds `field_475` / `field_476`
/// with -1 (the animation and "no id" sentinels `func_actor_135600_801330A8`
/// fills) and `field_508` with -1, zeroes the three `field_4D8` / `field_4DC` /
/// `field_4E0` 16.16 accumulators, and parks the three children it spawns in
/// `field_4FC` / `field_500` / `field_504`.
typedef struct Actor135600Work {
    /* 0x000 */ byte  pad_000[0x474];
    /* 0x474 */ s8    field_474; // non-zero while the animation slots tick
    /* 0x475 */ s8    field_475;
    /* 0x476 */ s8    field_476;
    /* 0x477 */ byte  pad_477[0x4C8 - 0x477];
    /* 0x4C8 */ s32   field_4C8; // per-frame deltas the accumulators take
    /* 0x4CC */ s32   field_4CC;
    /* 0x4D0 */ s32   field_4D0;
    /* 0x4D4 */ byte  pad_4D4[0x4D8 - 0x4D4];
    /* 0x4D8 */ s32   field_4D8;
    /* 0x4DC */ s32   field_4DC;
    /* 0x4E0 */ s32   field_4E0;
    /* 0x4E4 */ byte  pad_4E4[0x4F8 - 0x4E4];
    /* 0x4F8 */ s16   field_4F8; // selects which of the two handlers the tick runs
    /* 0x4FA */ byte  pad_4FA[0x4FC - 0x4FA];
    /* 0x4FC */ Task* field_4FC;
    /* 0x500 */ Task* field_500;
    /* 0x504 */ Task* field_504;
    /* 0x508 */ s32   field_508;
    /* 0x50C */ byte  pad_50C[0];
} Actor135600Work;
STATIC_ASSERT_SIZEOF(Actor135600Work, 0x50C);

/// Animation preset the 0x7D3 handler `func_actor_135600_801330A8` applies to
/// the work block. `field_0` is the animation id it compares against
/// `Actor135600Work::field_476` and latches there when the two differ,
/// `field_4` the byte it copies into `field_475`, `field_8` selects the reset
/// path and `field_C` is the per-slot argument that path passes on, so the
/// caller stores a word where the callee reads a byte. The five-word shape,
/// and the trailing `field_10` no callee reads, are `Actor113100AnimPreset`'s:
/// the setup handlers of this family build the same block.
typedef struct Actor135600AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor135600AnimPreset;
STATIC_ASSERT_SIZEOF(Actor135600AnimPreset, 0x14);

/// The world translation and Euler angles the actor's 0x7D4 handler places it
/// at, the same block `ActorsShared8013231cArgs` is.
typedef struct Actor135600PlaceArgs {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor135600PlaceArgs;
STATIC_ASSERT_SIZEOF(Actor135600PlaceArgs, 0x18);

/// Setup handler (state 0) of `D_actor_135600_80131E24`. It allocates the
/// 0x50C-byte work block, clears the "no id yet" sentinels and spawns the
/// actor's three children from `D_actor_135600_8013B0C4` -- indices 1 and 2 are
/// models that get the texture page and CLUT of the area record the actor's own
/// location key resolves to, index 3 has no model and is only parked. It then
/// hands the parent to `ActorsShared80132f24`, places it at (0xA6E, 0, 0x5F0)
/// yawed 0x400, applies the 0x7D3 animation preset, publishes the message table
/// `D_actor_135600_8013B0F4`, installs the exit callback and steps to state 1.
void func_actor_135600_80132234(Task* task);

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries.  The same shape `ActorsShared801639a8MatWords` has.
typedef struct Actor135600MatrixWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor135600MatrixWords;

typedef union Actor135600Matrix {
    MATRIX                 mat;
    Actor135600MatrixWords ident;
} Actor135600Matrix;
STATIC_ASSERT_SIZEOF(Actor135600Matrix, 0x20);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Non-zero while a non-gameplay mode holds the actors, and every carrier of
/// this per-frame body skips its whole update while it is set.
extern u8 D_801153F4;

/// Draws the ground-effect quad `func_800EA1A8` filled in, at the splash id
/// the caller selects and the current ground-shade row `Gp_State1C->field_8`.
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// The empty handler that fills entry 0 of the parent's per-frame handler pair
/// `{func_actor_135600_80132DF8, ActorsShared801327f8}`, selected by
/// `Actor135600Work::field_4F8`. Defined in `actor_135600_3.c`.
void func_actor_135600_80132DF8(Task* arg0);

/// Per-frame tick handler (state 1) of `D_actor_135600_80131E24`, the body
/// `func_actor_120400_80132050` and `func_actor_350700_80162D5C` also run.
/// Draws the actor's ground shadow unless the model suppresses it, then --
/// only while `D_801153F4` says gameplay is running -- dispatches the handler
/// pair `field_4F8` selects, advances the root coordinate by the high halves
/// of the three 16.16 accumulators at 0x4D8 (re-zeroing each low half), ticks
/// the 19 animation slots when `field_474` is set, republishes the collision
/// coordinate while the session flag at 0x4D is set, and counts `field_508`
/// down, releasing the model's buffers as it passes zero.
void func_actor_135600_801324D0(Task* task);

/// Recomputes `coord`'s world matrix (`Gp_UpdateCoord`) and then projects a
/// pair of offsets along the actor's local Z, returning the signed `ratan2` of
/// the difference between the two projections - the actor's screen-space angle.
/// `arg1` sets how far the far offset sits down that axis, at 70/4096 of a unit
/// per count, so the 0x800 the tick handler latches once the angle reaches
/// 0x1F5 is 35 units.
s32 func_actor_135600_80131E68(GsCOORDINATE2* coord, s32 arg1);

#endif // ACTOR_135600_H
