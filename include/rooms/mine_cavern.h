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
///
/// `light` and `color` are the two matrices the block itself supplies to the
/// model: `func_mine_cavern_801836D0` publishes `&work->light` / `&work->color`
/// into `TmdObject::field_1C` / `field_20`, which is what `Tmd_SetupDraw` loads
/// in place of `GsLIGHTWSMATRIX` and `D_80074080`.
typedef struct MineCavernWork {
    /* 0x000 */ MATRIX        light;
    /* 0x020 */ MATRIX        color;
    /* 0x040 */ GpObj         obj40;
    /* 0x060 */ byte          pad_60[0x60];
    /* 0x0C0 */ GpObj         objC0;
    /* 0x0E0 */ byte          pad_E0[0x18];
    /* 0x0F8 */ GsCOORDINATE2 coord;
    /* 0x148 */ u16           field_148;
    /* 0x14A */ byte          pad_14A[2];
} MineCavernWork;
STATIC_ASSERT_SIZEOF(MineCavernWork, 0x14C);

/// One tint of the cavern's darkness overlay: a `u8` RGB triple plus a zero
/// fourth byte. `func_mine_cavern_80182454` picks the entry by the number of
/// `GameFlag_GetNibble(0xE2)` bits set, so the rows run light to dark and the
/// full-screen wash deepens as that nibble fills in.
typedef struct MineCavernTint {
    /* 0x0 */ u8 r;
    /* 0x1 */ u8 g;
    /* 0x2 */ u8 b;
    /* 0x3 */ u8 pad;
} MineCavernTint;
STATIC_ASSERT_SIZEOF(MineCavernTint, 0x4);

/// The cavern's five tints: (0x1E,0x1E,0x1E), (0x19,0x19,0x19), (0x11,0x15,0x16),
/// (0x07,0x0F,0x10) and (0x00,0x09,0x0B).
extern MineCavernTint D_mine_cavern_8018E3E0[5];

#endif
