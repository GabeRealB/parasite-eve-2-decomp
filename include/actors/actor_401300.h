#ifndef ACTOR_401300_H
#define ACTOR_401300_H

#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Private work block of the actor 401300 task, hanging off `Task::idMap`.
///
/// Only the field the matched code touches is named so far: the halfword the
/// teardown-ish `func_actor_401300_80141EF8` tests before it stamps the
/// enemy's `field_40` with the -999 sentinel. The block is a good deal
/// larger - sibling `func_actor_401300_80141C88` reads animation state at
/// 0x89C..0xC0E of the same pointer - so the struct stays open-ended.
typedef struct Actor401300Work {
    /* 0x000 */ byte pad_0[0xC8A];
    /* 0xC8A */ s16  field_C8A;
} Actor401300Work;

void func_actor_401300_80141EF8(Task* task);

#endif // ACTOR_401300_H
