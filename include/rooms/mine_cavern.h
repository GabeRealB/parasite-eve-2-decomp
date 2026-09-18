#ifndef ROOMS_MINE_CAVERN_H
#define ROOMS_MINE_CAVERN_H

#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"

/// Work block a mine_cavern task parks at `Task::work`, allocated with
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
/// model's own coordinate (`TmdObject::coords`) under it as `sub`, which is
/// what leaves the model's positions relative to that spot.
///
/// `light` and `color` are the two matrices the block itself supplies to the
/// model: `func_mine_cavern_801836D0` publishes `&work->light` / `&work->color`
/// into `TmdObject::lightMtx` / `field_20`, which is what `Tmd_SetupDraw` loads
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

/// One of the per-view objects the cavern's sprite-table record points at,
/// carrying the byte this room's `func_mine_cavern_8017E3A0` writes. The
/// room's five pointers sit at 0x28 / 0x34 / 0x100 / 0x10C / 0x118 and the byte
/// is at a different offset in each of the objects they name -- 0x2C, 0x34,
/// 0x2C, 0x1C and 0x24 -- because the views are different-sized blocks. That
/// byte is the same flag the sibling rooms carrying this sprite table write
/// (`MineForkedTunnelViewA.field_1C` / `ViewB.field_2C`, `DwtwSprtViewState`):
/// non-zero leaves the view's sprites out of the ordering table, zero draws
/// them. All five are written to one value at a time, 1 by the cap-event flag
/// nibble 0xC7 and 0 otherwise (`func_mine_cavern_8017DDFC`).
typedef struct MineCavernSprtView1C {
    /* 0x00 */ byte pad_0[0x1C];
    /* 0x1C */ u8   field_1C;
} MineCavernSprtView1C;
STATIC_ASSERT_SIZEOF(MineCavernSprtView1C, 0x1D);

typedef struct MineCavernSprtView24 {
    /* 0x00 */ byte pad_0[0x24];
    /* 0x24 */ u8   field_24;
} MineCavernSprtView24;
STATIC_ASSERT_SIZEOF(MineCavernSprtView24, 0x25);

typedef struct MineCavernSprtView2C {
    /* 0x00 */ byte pad_0[0x2C];
    /* 0x2C */ u8   field_2C;
} MineCavernSprtView2C;
STATIC_ASSERT_SIZEOF(MineCavernSprtView2C, 0x2D);

typedef struct MineCavernSprtView34 {
    /* 0x00 */ byte pad_0[0x34];
    /* 0x34 */ u8   field_34;
} MineCavernSprtView34;
STATIC_ASSERT_SIZEOF(MineCavernSprtView34, 0x35);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at: a
/// room-sized block, far larger than the 0xC-byte `GpSprtRec` the table's
/// element type declares, so the room reaches its tail through a cast (as the
/// water tower's `DwtwSprtRec` and the forked tunnel's `MineForkedTunnelSprtRec`
/// do). The tail is a run of per-view pointers; this room's five sit at 0x28,
/// 0x34, 0x100, 0x10C and 0x118.
typedef struct MineCavernSprtRec {
    /* 0x000 */ byte                  pad_0[0x28];
    /* 0x028 */ MineCavernSprtView2C* field_28;
    /* 0x02C */ byte                  pad_2C[0x8];
    /* 0x034 */ MineCavernSprtView34* field_34;
    /* 0x038 */ byte                  pad_38[0xC8];
    /* 0x100 */ MineCavernSprtView2C* field_100;
    /* 0x104 */ byte                  pad_104[0x8];
    /* 0x10C */ MineCavernSprtView1C* field_10C;
    /* 0x110 */ byte                  pad_110[0x8];
    /* 0x118 */ MineCavernSprtView24* field_118;
} MineCavernSprtRec;
STATIC_ASSERT_SIZEOF(MineCavernSprtRec, 0x11C);

#endif
