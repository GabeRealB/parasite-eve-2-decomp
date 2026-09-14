#ifndef ACTORS_SHARED_80134178_H
#define ACTORS_SHARED_80134178_H

#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"

/// Partial view of the work block the actor keeps in `Task::idMap`. `field_4`
/// is the halfword flag this body tests; only its offset is known.
typedef struct ActorShared80134178Work {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ s16 field_4;
} ActorShared80134178Work;

/// Tears the actor's model down once the work block's flag is set: `node.field_4`
/// is the `GpLinkNode` slot byte `Gp_ClearNodeSlots` sets to release the node,
/// and `TmdObject.field_C` carries the deferred-kill bit `Task_Kill` uses.
///
/// Shared by `actor_123200` and `actor_223600`.
void ActorsShared80134178(GpEnemy* arg0, Task* arg1);

#endif
