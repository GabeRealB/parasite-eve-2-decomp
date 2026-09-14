#ifndef ACTOR_401000_H
#define ACTOR_401000_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Private work block of the actor 401000 task, hanging off `Task::idMap`.
///
/// Only the fields the decompiled code touches are named, so the struct is
/// deliberately open-ended: the three `GpObj` display nodes the teardown hands
/// back to `Gp_UnlinkObj`, and the two child tasks it kills. The nodes sit
/// 8 bytes later than the 0x8C8/0xA08/0xB48 triple on actor 01900/401800, with
/// the same 0x140 stride.
typedef struct Actor401000Work {
    /* 0x000 */ byte  pad_0[0x8D0];
    /* 0x8D0 */ GpObj field_8D0;
    /* 0x8F0 */ byte  pad_8F0[0x120];
    /* 0xA10 */ GpObj field_A10;
    /* 0xA30 */ byte  pad_A30[0x120];
    /* 0xB50 */ GpObj field_B50;
    /* 0xB70 */ byte  pad_B70[0xAC];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xC1C */ Task* field_C1C;
    /* 0xC20 */ Task* field_C20;
} Actor401000Work;

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, drop the enemy's `field_54` slot, then `Gp_DestroyEnemy`.
void func_actor_401000_8013DA78(Task* task);

#endif // ACTOR_401000_H
