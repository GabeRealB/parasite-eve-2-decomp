#ifndef ACTOR_123200_H
#define ACTOR_123200_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

#include <psyq/libgte.h>

/// The work block of `actor_123200`'s instances as these two readers see it:
/// `field_174` is the motion state, `field_4A` the current animation id (low
/// ten bits), and `field_220` latches the last trigger id reported.
/// `field_0` is the display mode's recorded state, written by
/// `func_actor_123200_80133E30`.
typedef struct Actor123200Work {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ s16        field_2;
    /* 0x004 */ s16        field_4; // non-zero restarts the model (`func_actor_123200_80133820`)
    /* 0x006 */ u16        field_6; // frames since the restart branch last ran
    /* 0x008 */ s16        field_8;
    /* 0x00A */ byte       pad_A[0x2];
    /* 0x00C */ GpAnimCtx  anim;     // `func_800B3F84` arg0
    /* 0x020 */ GpAnimSlot slots[1]; // slots 1..5 continue past here, overlapping the fields below
    /* 0x048 */ byte       pad_48[0x2];
    /* 0x04A */ u16        field_4A; // low ten bits: animation id (`slots[1].field_2`)
    /* 0x04C */ byte       pad_4C[0xC];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; // `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;   // motion state `func_actor_123200_801332E0` switches on
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174;
    /* 0x176 */ u16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ byte       pad_17A[0x4];
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ byte       pad_180[0x18];
    /* 0x198 */ u16        field_198;
    /* 0x19A */ u16        field_19A;
    /* 0x19C */ byte       pad_19C[0xC];
    /// World X/Y/Z of the model's coordinate, narrowed to 16 bits as the spawn
    /// handler samples them through `Actor123200CoordPos`.
    /* 0x1A8 */ u16    field_1A8;
    /* 0x1AA */ u16    field_1AA;
    /* 0x1AC */ u16    field_1AC;
    /* 0x1AE */ byte   pad_1AE[0x2];
    /* 0x1B0 */ s16    field_1B0;
    /* 0x1B2 */ s16    field_1B2;
    /* 0x1B4 */ s16    field_1B4;
    /* 0x1B6 */ byte   pad_1B6[0x6];
    /* 0x1BC */ MATRIX field_1BC; // installed at `TmdObject.lightMtx` by `func_actor_123200_8013352C`
    /* 0x1DC */ MATRIX field_1DC; // installed at `TmdObject.colorMtx`
    /* 0x1FC */ byte   pad_1FC[0x20];
    /// Model scale `func_actor_123200_80133BA0` puts on `field_1BC` through
    /// `ScaleMatrix`; 0x1000 is 1.0 and skips the scale entirely. Picked from
    /// the context's `field_8` nibble by `func_actor_123200_80133EDC`.
    /* 0x21C */ s16  field_21C;
    /* 0x21E */ byte pad_21E[0x2];
    /* 0x220 */ u16  field_220;
    /* 0x222 */ byte pad_222[0xA];
} Actor123200Work;
STATIC_ASSERT_SIZEOF(Actor123200Work, 0x22C);

/// `GsCOORDINATE2.coord.t[]` seen as three unsigned halfwords, so
/// `func_actor_123200_8013352C` samples each world coordinate with `lhu`. The
/// same narrowing `GpCoordXZ` does for X and Z, extended to Y.
typedef struct Actor123200CoordPos {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  x;
    /* 0x1A */ byte pad_1A[2];
    /* 0x1C */ u16  y;
    /* 0x1E */ byte pad_1E[2];
    /* 0x20 */ u16  z;
    /* 0x22 */ byte pad_22[2];
} Actor123200CoordPos;
STATIC_ASSERT_SIZEOF(Actor123200CoordPos, 0x24);

/// Overlay-wide record the spawn handler points at the instance's coordinate,
/// tagging it with a 0x100 weight and a mode of 1. Nothing matched reads it
/// back yet.
typedef struct Actor123200Anchor {
    /* 0x0 */ GsCOORDINATE2* coord;
    /* 0x4 */ s16            field_4;
    /* 0x6 */ s16            field_6;
} Actor123200Anchor;
STATIC_ASSERT_SIZEOF(Actor123200Anchor, 0x8);

extern Actor123200Anchor D_actor_123200_80137248;

/// Pair source the spawn handler installs at `GpEnemy::field_50`.
extern GpPairSrcE D_actor_123200_80134208;

/// Animation source `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_123200_80137154[];

/// Message table the spawn handler publishes as `Task::msgTable`.
extern u8 D_actor_123200_80137214[];

/// Caller-owned context the actor also keeps a pointer to; `field_8` carries
/// the hard-mode nibble its sound id is tagged with, and `field_14` is the flag
/// the state handlers raise and clear, the same shape as `Actor104000Ctx` and
/// `Actor521100Ctx`.
typedef struct Actor123200Ctx {
    /* 0x00 */ byte pad_0[0x8];
    /* 0x08 */ u16  field_8;
    /* 0x0A */ byte pad_A[0xA];
    /* 0x14 */ s8   field_14;
} Actor123200Ctx;

/// Handler signature of the display table `D_actor_123200_80131E24`: the shared
/// `ActorsShared80134178`, this overlay's per-frame step
/// `func_actor_123200_80133820` and `func_actor_123200_801339F0`.
typedef void (*Actor123200StateFn)(Actor123200Ctx* arg0, Task* arg1);

/// The three handlers `func_actor_123200_80133BA0` copies onto its stack and
/// dispatches on the work block's display mode `field_0`.
typedef struct Actor123200StateTable {
    /* 0x00 */ Actor123200StateFn fn[3];
} Actor123200StateTable;

extern Actor123200StateTable D_actor_123200_80131E24;

/// Per-frame tick: refreshes the model's coordinate and colour, scales the part
/// matrix from `field_21C`, handles the render mode in `D_801153F4`, dispatches
/// the display-mode handler in `field_0` and plays the sound that handler
/// reports.
void func_actor_123200_80133BA0(Actor123200Ctx* arg0, Task* arg1);

/// In states 2 and 3, reports 0x400C0001 the first time the animation id in
/// `field_4A` reaches one of the state's trigger ids (latched in `field_220`);
/// in state 5, 0x400C0005 while bit 2 of `field_58` is set. Returns 0
/// otherwise.
s32 func_actor_123200_80133450(Actor123200Work* arg0);

/// Spawn state: allocates the 0x22C work block, publishes it as `Task::work`,
/// reparents the model to `gGfxViewCoord`, seeds its animation slots and hangs
/// the enemy's display node off part 2 of the model's coordinate array. The
/// context's top `field_8` nibble biases the three timers in `field_176`,
/// `field_198` and `field_19A` -- up when its low bit is set, down by half of
/// it otherwise.
void func_actor_123200_8013352C(GpEnemy* enemy, Task* task);

/// Display handler in the same message-table family as the shared
/// `ActorsShared80164844` / `ActorsShared8013d268` bodies. `arg2` selects the
/// mode: 0 hides the display object (`TmdObject.flags` bit 0x80), 1 clears
/// `field_C` and so shows it, 2 sets bit 0x4, and 3 and 4 both clear `field_C`
/// and then set bit 0x4. Modes 0 and 1 reinstate the object's buffers through
/// `Tmd_AllocBuffers` and restart the work block's `field_0` at 1; modes 2, 3
/// and 4 restart it at 0. `arg1` is unused; it exists because the dispatch
/// passes three arguments.
s32 func_actor_123200_80133E30(Task* task, s32 arg1, s32 arg2);

/// Per-frame handler. A pending restart on the work block's `field_4` re-arms
/// the model and returns; otherwise `field_6` counts frames and, unless the
/// game is frozen, the model is stepped 5/0x1000 forward along its facing
/// between a 0xC-byte scratch reservation and the animation update.
void func_actor_123200_80133820(Actor123200Ctx* arg0, Task* task);

#endif
