#ifndef ACTOR_356100_H
#define ACTOR_356100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs off `Task::idMap`. `field_4` is
/// the live-actor flag `func_actor_356100_8016A1D8` tests, where
/// `Actor00100Work::field_4` sits. `field_0` / `field_5A` / `field_68` are the
/// same state, clip-id and flag halfwords `Actor01900Work` keeps at those
/// offsets; `field_974` is the `field_5A & 0x3FF` snapshot
/// `func_actor_356100_8016A468` stores (same role as `Actor01900Work.field_8B4`).
/// The halfwords at 0x978..0x982 are the same animation-state slots
/// `Actor01900_Fn0A7C0` writes at 0x898..0x8A2.
/// `field_B5C` / `field_B60` are the two helper tasks the exit callback
/// kills; same pair as `Actor01900Work` at +0xC38 / +0xC3C, without the three
/// `GpObj` nodes that teardown unlinks.
typedef struct Actor356100Work {
    /* 0x000 */ s16   field_0;
    /* 0x002 */ byte  pad_2[2];
    /* 0x004 */ s16   field_4;
    /* 0x006 */ byte  pad_6[0x54];
    /* 0x05A */ u16   field_5A;
    /* 0x05C */ byte  pad_5C[0xC];
    /* 0x068 */ u16   field_68;
    /* 0x06A */ byte  pad_6A[0x90A];
    /* 0x974 */ s32   field_974;
    /* 0x978 */ s16   field_978;
    /* 0x97A */ s16   field_97A;
    /* 0x97C */ byte  pad_97C[2];
    /* 0x97E */ s16   field_97E;
    /* 0x980 */ byte  pad_980[2];
    /* 0x982 */ s16   field_982;
    /* 0x984 */ byte  pad_984[0x1D8];
    /* 0xB5C */ Task* field_B5C;
    /* 0xB60 */ Task* field_B60;
} Actor356100Work;

/// Per-task actor context: `field_1C` is the work block above (the same
/// pointer `Task::idMap` holds), `field_20` the `GpEnemy` in
/// `Task::spawnArg2`, and `field_2C` the actor's `TmdObject`. Same shape as
/// `Actor01900` / `Actor401000`. The sibling teardown
/// `func_actor_356100_8016A158` reaches those same slots as a `Task*`.
typedef struct Actor356100 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor356100Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor356100;

void func_actor_356100_80163508(Actor356100* arg0);

/// `Task::exitCallback` teardown: kill the two helper tasks, drop the
/// enemy's `field_54` slot, then `Gp_DestroyEnemy`. Same shape as
/// `Actor01900_Fn0A6CC` without the three `Gp_UnlinkObj` calls.
void func_actor_356100_8016A158(Task* task);

/// When the work block's `field_4` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `Actor00100_Fn0B4D8` without extra flag masks.
void func_actor_356100_8016A1D8(Actor356100* arg0);

/// When the work block's `field_4` flag is set, clears the enemy's link node,
/// reallocates the model buffers and writes the 0x978..0x982 animation
/// slots; otherwise clears the model's root `flg`. Same shape as
/// `Actor01900_Fn0A7C0` without the two `GpObj` flag masks.
void func_actor_356100_8016A21C(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 3`.
void func_actor_356100_8016A2AC(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A340(Actor356100* arg0);

/// Same shape as `func_actor_356100_8016A21C` with `field_97E = 0xB`.
void func_actor_356100_8016A3D4(Actor356100* arg0);

/// Message 0x3FF payload `func_actor_356100_8016A468` sends the slot-3 task.
/// `field_0` points at `D_actor_356100_80173228`; the function overwrites
/// `field_4` with 2 before the dispatch.
extern GpAnimArg D_actor_356100_80173244;

/// When the work block's `field_4` flag is set, writes the 0x978..0x982
/// animation slots, sends message 0x3FF then 0x3F9 at slot 3, and snapshots
/// `field_5A & 0x3FF` into `field_974`. Bit 1 of `field_68` forces `field_0`
/// to 0xE.
void func_actor_356100_8016A468(Actor356100* arg0);

#endif
