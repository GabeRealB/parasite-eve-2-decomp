#ifndef PE_ENERGYBALL_H
#define PE_ENERGYBALL_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `SndEvt` ids for the energy ball, indexed by `GpEffWork.field_20`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the sound scales with the combo
/// counter). The first three are the charge loop `func_energyball_8012EF48`
/// starts with `SndEvt_EnqueueType6`; the last three are the matching `...0001`
/// variants.
extern s32 D_energyball_8013117C[];

/// Sixteen 8-bit draws from `Gp_LcgState`, refilled once per cast by
/// `func_energyball_8012EF48` and consumed by the GTE pass in
/// `func_energyball_80130B54` as the per-vertex jitter of the ball's surface.
extern s16 D_energyball_801311A0[];

/// 0x1C-byte scratch block `func_energyball_8013035C` takes from
/// `G_SCRATCH_HEAD`. `vec` is the effect coordinate's `workm.t[]` truncated to
/// s16 and projected through `GsWSMATRIX` with one `RTPS`: `flag` is the
/// `gte_stflg` of that projection (a negative value drops the quad), `otz` its
/// `gte_stszotz` and `sx` / `sy` its `gte_stsxy`. `dx` / `dy` hold the current
/// `(arg2 * 55 / otz) * rsin|rcos(angle) >> 12` offsets that are added to and
/// subtracted from `sx` / `sy` to build the four quad corners; only their low
/// halves are read back. Same layout as `FlareQuadScratch` and the gameplay
/// `GpFxQuadScratch`, which are built the same way.
typedef struct EnergyQuadScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} EnergyQuadScratch;
STATIC_ASSERT_SIZEOF(EnergyQuadScratch, 0x1C);

/// Links one frame of the energy ball's core sprite at `arg0`'s world
/// position. The position is projected through `GsWSMATRIX` by a single `RTPS`
/// and the quad is dropped when that sets a negative `gte_stflg`. `arg1` is the
/// effect's frame counter and its low bit alternates the two looks: odd frames
/// draw the raw, semi-transparent 0x428B cell, even frames the 0x428C cell
/// tinted `(0x40, 0xC0, 0x60)`. `arg3` spins the quad and `arg2` sizes it: the
/// corners sit `arg2 * 55 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the sprite shrinks with depth.
void func_energyball_8013035C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Overlay copy of `Gp_DrawRing` with a flat tint: an eight-segment gouraud
/// ring at `arg0`'s world position, radius `arg1 * 64 / otz`, lit only at the
/// centre with `(arg2 / 2, arg2, arg2 / 2)`, each wedge given the
/// semi-transparent tpage of `Gp_AddTpageShift`.
void func_energyball_8012FFD0(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `func_energyball_801307D4`
/// for the ground-plane quad. `vec` holds the four corners of the unit quad
/// `D_80111E38`, scaled to the caller's half-size, rotated flat by
/// `Gfx_ViewWorldMtx` and shifted onto `arg0->workm.t`; `sxy0`..`sxy3` are the
/// four screen corners the single `RTPS` + `RTPT` pair projects them to.
/// `otz` and `flag` are function locals, not fields of this block.
typedef struct EnergyGroundScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ DVECTOR sxy0;
    /* 0x24 */ DVECTOR sxy1;
    /* 0x28 */ DVECTOR sxy2;
    /* 0x2C */ DVECTOR sxy3;
} EnergyGroundScratch;
STATIC_ASSERT_SIZEOF(EnergyGroundScratch, 0x30);

/// Draws a ground-plane quad at `arg0`'s `workm` translation: the unit quad
/// `D_80111E38` is scaled to `arg1` half-size (Y stays 0), rotated flat by
/// `Gfx_ViewWorldMtx`, then projected through `GsWSMATRIX`. One `RTPS` plus
/// one `RTPT` project the four corners; a negative `gte_stflg` drops the
/// quad. The texture is the two-frame tpage-0x28 strip at rows 0x38..0x57,
/// the frame picked by the low bit of `Display_State.field_8`, tinted
/// `(0x20, 0x30, 0x20)`.
void func_energyball_801307D4(GsCOORDINATE2* arg0, s32 arg1);

#endif /* PE_ENERGYBALL_H */
