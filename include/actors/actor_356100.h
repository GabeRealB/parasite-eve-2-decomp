#ifndef ACTOR_356100_H
#define ACTOR_356100_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs off `Task::idMap`. `field_4` is
/// the live-actor flag `func_actor_356100_8016A1D8` tests, where
/// `Actor00100Work::field_4` sits. `field_B5C` / `field_B60` are the two
/// helper tasks the exit callback kills; same pair as `Actor01900Work` at
/// +0xC38 / +0xC3C, without the three `GpObj` nodes that teardown unlinks.
typedef struct Actor356100Work {
    /* 0x000 */ byte  pad_0[4];
    /* 0x004 */ s16   field_4;
    /* 0x006 */ byte  pad_6[0xB56];
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

/// `Task::exitCallback` teardown: kill the two helper tasks, drop the
/// enemy's `field_54` slot, then `Gp_DestroyEnemy`. Same shape as
/// `Actor01900_Fn0A6CC` without the three `Gp_UnlinkObj` calls.
void func_actor_356100_8016A158(Task* task);

/// When the work block's `field_4` flag is set, flags the enemy's link node
/// and raises bit 0x80 of the model's `field_C`. Same shape as
/// `ActorsShared80164c20` / `Actor00100_Fn0B4D8` without extra flag masks.
void func_actor_356100_8016A1D8(Actor356100* arg0);

#endif
