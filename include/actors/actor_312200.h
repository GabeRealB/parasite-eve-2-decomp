#ifndef ACTOR_312200_H
#define ACTOR_312200_H

#include "common.h"

#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Private work block of the actor 312200 task, hanging off `Task::idMap`.
///
/// Only the fields the matched code touches are named so far: `yaw` at 0x08 is
/// the heading `func_actor_312200_801635CC` reads back from the root
/// coordinate, two halfwords earlier than `ActorsShared80169f74Work::yaw`. The
/// block is much larger - sibling `func_actor_312200_801636CC` writes halfwords
/// at 0x0, 0x88C, 0x892 and 0x8B4..0x8B8 of the same pointer - so the struct
/// stays open-ended.
///
/// The named tail is the actor state block shared by this family, laid out like
/// `Actor110600Work`: `field_4` is the live-actor flag every state handler tests
/// on entry, `field_88C` the work state, `field_892` / `field_896` the two
/// timers the state handlers arm, and `field_5C` the flag halfword whose bit 0
/// the callbacks test.
///
/// `field_8BC` is the display node the spawn handler
/// `func_actor_312200_80163178` builds in place and hands to `Gp_LinkObj` - the
/// `GpObj` whose `field_C` it points at a three-entry `GpRec18` table at 0x8DC.
/// `func_actor_312200_80163778` clears bit 0x8000 of that node's `flags`.
typedef struct Actor312200Work {
    /* 0x000 */ byte pad_0[0x4];
    /* 0x004 */ s16  field_4;
    /* 0x006 */ byte pad_6[0x2];
    /* 0x008 */ s16  yaw;
    /* 0x00A */ byte pad_A[0x52];
    /* 0x05C */ u16  field_5C;
    /* 0x05E */ byte pad_5E[0x82E];
    /* 0x88C */ s16  field_88C;
    /* 0x88E */ byte pad_88E[0x4];
    /* 0x892 */ s16  field_892;
    /* 0x894 */ byte pad_894[0x2];
    /* 0x896 */ s16  field_896;
    /* 0x898 */ byte pad_898[0x24];
    /// Display node: `GpObj` at 0x8BC, its `GpRec18` table at 0x8DC.
    /* 0x8BC */ GpObj field_8BC;
} Actor312200Work;

/// Placement opcode: the three longs of `placement->pos` are copied onto the
/// actor's root coordinate, the Euler angles are applied X / Y / Z, and the
/// resulting heading is read back out of the matrix Z-axis with `ratan2` and
/// cached in the work block.
s32 func_actor_312200_801635CC(Task* task, s32 arg1, ActorShared80169f74Placement* placement);

/// Per-tick state callback: on a live actor it re-enters state 2 with the
/// 0x896 timer armed at 0x10, then hands the tick to `func_actor_312200_801637CC`'s
/// anim/particle update. Once the 0x892 timer has run its 0x10 ticks with the
/// task's flag bit 0 set, it drops the state to 1 and shorts the timer to 4.
void func_actor_312200_801637CC(Task* task);

/// Show handler: on a live actor it sets the enemy's `node.field_4`, raises the
/// 0x80 draw bit of the model's `TmdObject::field_C`, clears
/// `GpEnemy::field_4D` and drops bit 0x8000 of the display node's `flags`.
void func_actor_312200_80163778(Task* task);

void func_actor_312200_80162FB4(Task* task);

#endif // ACTOR_312200_H
