#ifndef ROOMS_MINE_CAVERN_H
#define ROOMS_MINE_CAVERN_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block a mine_cavern task parks at `Task::idMap`. Only the `GpObj`
/// display node at +0x40 is known so far, and the sibling
/// `func_mine_cavern_80183890` is what gives it away: it clears
/// `obj40.flags` with `andi 0x7FFF` at +0x5E and then passes `&obj40` (a
/// `+0x40` on the same base pointer) to `Gp_UnlinkObj`, the way
/// `func_mine_cavern_80183860` does on its own exit path.
typedef struct MineCavernWork {
    /* 0x00 */ byte  pad_0[0x40];
    /* 0x40 */ GpObj obj40;
} MineCavernWork;

#endif
