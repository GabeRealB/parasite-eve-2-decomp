#ifndef PE_ENERGYBALL_H
#define PE_ENERGYBALL_H

#include "common.h"

#include <psyq/libgs.h>
#include "main/coord.h"
#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "overlay.h"

/// `SndEvt` ids for the energy ball, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the sound scales with the combo
/// counter). The first three are the charge loop `func_energyball_8012EF48`
/// starts with `SndEvt_EnqueueType6`; the last three are the matching `...0001`
/// variants.
extern s32 D_energyball_8013117C[];

/// One 4-byte row of `D_energyball_80131194`, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the full size the ball grows
/// to before it is launched (`GpEffWork.angle`; half of it is the linked
/// `GpObj.radius`, twice it the burst's final size) and `field_2` the
/// per-frame growth step, also the initial upward speed while charging.
typedef struct EnergyBallStep {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
} EnergyBallStep;
STATIC_ASSERT_SIZEOF(EnergyBallStep, 4);

/// Collision block allocated by `func_energyball_8012F180` (`memCalloc(0x38)`)
/// and stored in `Task::work`: `obj` is linked on list 1 with `ctx.recs`
/// pointing at the one-element `rec` table (terminator `field_0 = 2`).
typedef struct EnergyBallWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec;
} EnergyBallWork;
STATIC_ASSERT_SIZEOF(EnergyBallWork, 0x38);

/// Three energy ball charge levels, weakest first.
extern EnergyBallStep D_energyball_80131194[];

/// Sixteen 8-bit draws from `Gp_LcgState`, refilled once per cast by
/// `func_energyball_8012EF48` and consumed by the GTE pass in
/// `func_energyball_80130B54` as the per-vertex jitter of the ball's surface.
extern s16 D_energyball_801311A0[];

/// Links one frame of the energy ball's core sprite at `arg0`'s world
/// position. The position is projected through `GsWSMATRIX` by a single `RTPS`
/// and the quad is dropped when that sets a negative `gte_stflg`. `arg1` is the
/// effect's frame counter and its low bit alternates the two looks: odd frames
/// draw the raw, semi-transparent 0x428B cell, even frames the 0x428C cell
/// tinted `(0x40, 0xC0, 0x60)`. `arg3` spins the quad and `arg2` sizes it: the
/// corners sit `arg2 * 55 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the sprite shrinks with depth.
void func_energyball_8013035C(GpCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Overlay copy of `Gp_DrawRing` with a flat tint: an eight-segment gouraud
/// ring at `arg0`'s world position, radius `arg1 * 64 / otz`, lit only at the
/// centre with `(arg2 / 2, arg2, arg2 / 2)`, each wedge given the
/// semi-transparent tpage of `Gp_AddTpageShift`.
void func_energyball_8012FFD0(GpCoord* arg0, s16 arg1, s16 arg2);

/// Draws a ground-plane quad at `arg0`'s `workm` translation: the unit quad
/// `D_80111E38` is scaled to `arg1` half-size (Y stays 0), rotated flat by
/// `Gfx_ViewWorldMtx`, then projected through `GsWSMATRIX`. One `RTPS` plus
/// one `RTPT` project the four corners; a negative `gte_stflg` drops the
/// quad. The texture is the two-frame tpage-0x28 strip at rows 0x38..0x57,
/// the frame picked by the low bit of `gDisplayState.animFrame`, tinted
/// `(0x20, 0x30, 0x20)`.
void func_energyball_801307D4(GpCoord* arg0, s32 arg1);

/// Draws the energy ball's surface: two 16-vertex rings of the same radius
/// sit `arg1 * 2` apart in `arg0`'s local Y, are rotated by its `workm` and
/// offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one semi-transparent `POLY_FT4`, tinted
/// `(arg2 >> 1, arg2, arg2 >> 1)`.
void func_energyball_80130B54(GpCoord* arg0, s16 arg1, s16 arg2);

#endif /* PE_ENERGYBALL_H */
