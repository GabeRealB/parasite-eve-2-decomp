#ifndef PE_COMBUSTION_H
#define PE_COMBUSTION_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// 0x1C-byte scratch block `func_combustion_80130184` takes from
/// `G_SCRATCH_HEAD`. `vec` is the flame coordinate's `workm.t[]`, projected
/// through `GsWSMATRIX` with one `RTPS`: `flag` is that projection's
/// `gte_stflg` (a negative value drops the quad), `otz` its `gte_stszotz` and
/// `sx` / `sy` its `gte_stsxy`. `dx` / `dy` are the two rotated half-diagonals
/// the quad's corners are offset by.
typedef struct CombustionQuadScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} CombustionQuadScratch;
STATIC_ASSERT_SIZEOF(CombustionQuadScratch, 0x1C);

/// One 8-byte row of `D_combustion_80130980`, indexed by `GpEffWork.field_20`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the burn scales with the combo counter).
/// `field_0` / `field_2` are the per-frame Y / Z drift added to the flame
/// overlay `GpEffWork.field_12` / `field_14`. `field_4` is the last
/// `GpEffWork.field_22` tick that still spawns flames, and `field_6` is the
/// last tick of the burn as a whole; it is also the pad-rumble duration
/// `Gp_SpawnPadLerp` is given when the effect starts.
typedef struct CombustionStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} CombustionStep;
STATIC_ASSERT_SIZEOF(CombustionStep, 0x8);

/// Three combustion intensities, weakest first.
extern CombustionStep D_combustion_80130980[];
/// The `SndEvt_EnqueueType6` id for each `D_combustion_80130980` row.
extern s32 D_combustion_80130998[];
/// The effect coordinate's world Y at ignition, saved by
/// `func_combustion_8012EF34` before it re-bases the coordinate on the player.
extern s32 D_combustion_801309A4;

/// Links one frame of the small combustion flame at `arg0`'s world position.
/// The position is projected through `GsWSMATRIX` by a single `RTPS` and the
/// quad is dropped when that sets a negative `gte_stflg`. `arg1 % 6` picks one
/// of the six 0x20-wide texture frames on tpage 0x29 (CLUT 0x4282), and `arg2`
/// sizes it: the corners sit `arg2 * 31 / otz` from the projected centre.
/// Same 0x18-byte scratch and axis-aligned quad as `func_combustion_8012FF0C`.
void func_combustion_8012F5EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

/// Links one frame of the combustion flame at `arg0`'s world position. The
/// position is projected through `GsWSMATRIX` by a single `RTPS` and the quad
/// is dropped when that sets a negative `gte_stflg`. `arg1` picks one of the
/// eight 0x18-wide texture frames on tpage 0x28 (CLUT 0x430D), and `arg2`
/// sizes it: the corners sit `arg2 * 23 / otz` from the projected centre, so
/// the sprite shrinks with depth. Same 0x18-byte scratch and axis-aligned
/// quad as gameplay `Gp_EffSprTask8D`.
void func_combustion_8012FF0C(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);

/// Projects `arg0`'s world position and queues one textured `POLY_FT4` billboard
/// quad around it: a square of half-diagonal `arg2 * 0x37 / (otz + 1)` rotated by
/// `arg3`, with the two diagonals a quarter turn apart. `arg1`'s low bit picks
/// the frame - odd draws the tinted semi-transparent flame core, even the
/// additive outer flame.
void func_combustion_80130184(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

#endif /* PE_COMBUSTION_H */
