#ifndef ACTOR_401300_H
#define ACTOR_401300_H

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Private work block of the actor 401300 task, hanging off `Task::idMap`.
///
/// Only the fields the matched code touches are named so far: the three `GpObj`
/// display nodes `func_actor_401300_80141758` hands back to `Gp_UnlinkObj`, the
/// two child tasks it kills, and the halfword the teardown-ish
/// `func_actor_401300_80141EF8` tests before it stamps the enemy's `field_40`
/// with the -999 sentinel. The block is a good deal larger - sibling
/// `func_actor_401300_80141C88` reads animation state at 0x89C..0xC0E of the
/// same pointer - so the struct stays open-ended.
///
/// The display nodes do *not* sit at the same addresses as the same-shaped
/// teardown of actor 01900/401800, which keeps its three at 0x8C8/0xA08/0xB48.
typedef struct Actor401300Work {
    /* 0x000 */ byte  pad_0[0x970];
    /* 0x970 */ GpObj field_970;
    /* 0x990 */ byte  pad_990[0x120];
    /* 0xAB0 */ GpObj field_AB0;
    /* 0xAD0 */ byte  pad_AD0[0x120];
    /* 0xBF0 */ GpObj field_BF0;
    /* 0xC10 */ byte  pad_C10[0x7A];
    /* 0xC8A */ s16   field_C8A;
    /* 0xC8C */ byte  pad_C8C[0x80];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xD0C */ Task* field_D0C;
    /* 0xD10 */ Task* field_D10;
} Actor401300Work;

void func_actor_401300_80141758(Task* task);

void func_actor_401300_80141EF8(Task* task);

#endif // ACTOR_401300_H
