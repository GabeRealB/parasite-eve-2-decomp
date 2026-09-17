#ifndef ACTOR_123200_H
#define ACTOR_123200_H

#include "common.h"

#include "main/task.h"

/// The work block of `actor_123200`'s instances as these two readers see it:
/// `field_174` is the motion state, `field_4A` the current animation id (low
/// ten bits), and `field_220` latches the last trigger id reported.
/// `field_0` is the display mode's recorded state, written by
/// `func_actor_123200_80133E30`.
typedef struct Actor123200Work {
    /* 0x000 */ s16  field_0;
    /* 0x002 */ s16  field_2;
    /* 0x004 */ s16  field_4; // non-zero restarts the model (`func_actor_123200_80133820`)
    /* 0x006 */ u16  field_6; // frames since the restart branch last ran
    /* 0x008 */ byte pad_8[0x42];
    /* 0x04A */ u16  field_4A;
    /* 0x04C */ byte pad_4C[0xC];
    /* 0x058 */ u16  field_58;
    /* 0x05A */ byte pad_5A[0x116];
    /* 0x170 */ s16  field_170; // motion state `func_actor_123200_801332E0` switches on
    /* 0x172 */ s16  field_172;
    /* 0x174 */ s16  field_174;
    /* 0x176 */ byte pad_176[0x3A];
    /* 0x1B0 */ s16  field_1B0;
    /* 0x1B2 */ s16  field_1B2;
    /* 0x1B4 */ s16  field_1B4;
    /* 0x1B6 */ byte pad_1B6[0x6A];
    /* 0x220 */ u16  field_220;
} Actor123200Work;

/// Caller-owned context the actor also keeps a pointer to; `field_14` is the
/// flag the state handlers raise and clear, the same shape as `Actor104000Ctx`
/// and `Actor521100Ctx`.
typedef struct Actor123200Ctx {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s8   field_14;
} Actor123200Ctx;

/// In states 2 and 3, reports 0x400C0001 the first time the animation id in
/// `field_4A` reaches one of the state's trigger ids (latched in `field_220`);
/// in state 5, 0x400C0005 while bit 2 of `field_58` is set. Returns 0
/// otherwise.
s32 func_actor_123200_80133450(Actor123200Work* arg0);

/// Display handler in the same message-table family as the shared
/// `ActorsShared80164844` / `ActorsShared8013d268` bodies. `arg2` selects the
/// mode: 0 hides the display object (`TmdObject.field_C` bit 0x80), 1 clears
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
