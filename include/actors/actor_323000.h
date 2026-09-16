#ifndef ACTOR_323000_H
#define ACTOR_323000_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Head of the work block this overlay hangs off `Task::idMap`. Only the
/// halfwords the handlers touch are known: `field_4` is the live-actor flag
/// every state handler tests, and `field_6` / `field_828` / `field_82E` /
/// `field_832` / `field_83E` / `field_840` are the animation-state slots the
/// `Actor00100Work` family keeps at the same offsets, where
/// `Actor00100_Fn0B52C` writes them in the same order.
typedef struct Actor323000Work {
    /* 0x000 */ byte pad_0[4];
    /* 0x004 */ s16  field_4;
    /// Frame counter `func_actor_323000_80163A30` advances; zeroed by the
    /// re-init handler below.
    /* 0x006 */ s16  field_6;
    /* 0x008 */ byte pad_8[0x820];
    /* 0x828 */ s16  field_828;
    /* 0x82A */ byte pad_82A[4];
    /* 0x82E */ s16  field_82E;
    /* 0x830 */ byte pad_830[2];
    /* 0x832 */ s16  field_832;
    /* 0x834 */ byte pad_834[0xA];
    /* 0x83E */ s16  field_83E;
    /* 0x840 */ s16  field_840;
} Actor323000Work;

/// Per-frame tick of the live actor, run once the work block's animation
/// slots are set up.
void func_actor_323000_80163A30(Task* task);

/// Re-init handler (table `D_actor_323000_80161E24`, index 2): when the work
/// block's `field_4` flag is set, flags the enemy's link node, drops the
/// model's root `field_C` and reallocates its buffers, then writes the
/// animation-state slots and clears the frame counter. Either way the tick
/// above runs. Reached as `fns[state](task->spawnArg2, task)`, like the other
/// handler tables in this family.
void func_actor_323000_80164C58(GpEnemy* enemy, Task* task);

#endif
