#ifndef ROOMS_MINE_CAVERN_H
#define ROOMS_MINE_CAVERN_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block a mine_cavern task parks at `Task::idMap`, allocated with
/// `Mem_Calloc(0x14C, 0)` by the state-0 handler `func_mine_cavern_80182E34`
/// (and by `func_mine_cavern_801836D0`). It carries two `GpObj` display nodes:
/// the `+0x40` one is what `func_mine_cavern_80183890` is given away by - it
/// clears `obj40.flags` with `andi 0x7FFF` at +0x5E and then passes `&obj40` (a
/// `+0x40` on the same base pointer) to `Gp_UnlinkObj`, the way
/// `func_mine_cavern_80183860` does on its own exit path - and
/// `func_mine_cavern_801838F4` reaches the second by `+0xC0`, clearing its
/// flags at +0xDE the same way. `field_148` is the counter that function ticks
/// and switch-dispatches on (against 0x3C) and that `func_mine_cavern_80183890`
/// clears on its way out.
///
/// `coord` is the block's own display coordinate. `func_mine_cavern_80183AD4`
/// resets it - identity rotation, parked at (0, -0x320, 0) - and hangs the
/// model's own coordinate (`TmdObject::field_8`) under it as `sub`, which is
/// what leaves the model's positions relative to that spot.
typedef struct MineCavernWork {
    /* 0x000 */ byte          pad_0[0x40];
    /* 0x040 */ GpObj         obj40;
    /* 0x060 */ byte          pad_60[0x60];
    /* 0x0C0 */ GpObj         objC0;
    /* 0x0E0 */ byte          pad_E0[0x18];
    /* 0x0F8 */ GsCOORDINATE2 coord;
    /* 0x148 */ u16           field_148;
    /* 0x14A */ byte          pad_14A[2];
} MineCavernWork;
STATIC_ASSERT_SIZEOF(MineCavernWork, 0x14C);

#endif
