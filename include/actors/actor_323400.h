#ifndef ACTOR_323400_H
#define ACTOR_323400_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs behind `Task::idMap`; only the
/// slots its handlers touch are known. `field_4` is the live flag the
/// dispatcher `func_actor_323400_801644C4` raises when the work block's
/// animation state changes and the re-init handler below clears. The
/// `field_828` / `field_832` / `field_83E` / `field_840` group is the
/// animation-state set the `Actor00100Work` family keeps at the same offsets,
/// written in the same order by the sibling `func_actor_323000_80164C58`.
typedef struct Actor323400Work {
    /* 0x000 */ byte pad_0[4];
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x822];
    /* 0x828 */ s16  field_828;
    /* 0x82A */ byte pad_82A[0x8];
    /* 0x832 */ s16  field_832;
    /* 0x834 */ byte pad_834[0xA];
    /* 0x83E */ s16  field_83E;
    /* 0x840 */ s16  field_840;
} Actor323400Work;

/// Per-frame tick of the live actor, run once the work block's animation
/// slots are set up.
void func_actor_323400_80163B58(Task* task);

/// Re-init handler (table `D_actor_323400_80161E24`, index 1): when the work
/// block's `field_4` flag is set, clears the enemy's link-node flag, drops the
/// model's root `field_C` and reallocates its buffers, then writes the
/// animation-state slots. Either way the tick above runs. Reached as
/// `fns[state](task->spawnArg2, task)`, like the other handler tables in this
/// family.
void func_actor_323400_80164BD0(GpEnemy* enemy, Task* task);

#endif
