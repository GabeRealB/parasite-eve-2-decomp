#ifndef PE_PEPPER_SPRAY_H
#define PE_PEPPER_SPRAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// 0x1C-byte scratch block `func_pepper_spray_8012F21C` takes from
/// `G_SCRATCH_HEAD` for the spray nozzle quad. `vec` is the effect
/// coordinate's `workm.t[]` truncated to s16 and projected through
/// `GsWSMATRIX` with one `RTPS`: `flag` is the `gte_stflg` of that projection
/// (a negative value drops the quad), `otz` its `gte_stszotz` and `sx` / `sy`
/// its `gte_stsxy`. `dx` / `dz` hold the current
/// `(arg1 * 0x37 / otz) * rsin|rcos(angle) >> 12` offsets that are added to
/// and subtracted from `sx` / `sy` to build the four corners; only their low
/// halves are read back.
typedef struct PepperSprayNozzleScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     dx;
    /* 0x08 */ s32     dz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ SVECTOR vec;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} PepperSprayNozzleScratch;
STATIC_ASSERT_SIZEOF(PepperSprayNozzleScratch, 0x1C);

/// 0x28-byte scratch block `func_pepper_spray_8012F634` takes from
/// `G_SCRATCH_HEAD` for one spray quad. `v` is built in the effect
/// coordinate's frame, rotated by its `workm` and translated by its `t`, then
/// projected through `GsWSMATRIX`; `flag` is the `gte_stflg` of that
/// projection (a negative value drops the quad) and `otz` is its
/// `gte_stszotz`, incremented by 1 before it picks the OT bucket.
typedef struct PepperSprayScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     flag;
    /* 0x08 */ SVECTOR v[4];
} PepperSprayScratch;
STATIC_ASSERT_SIZEOF(PepperSprayScratch, 0x28);

/// The six spray-cone yaws, refilled once per cast by
/// `func_pepper_spray_8012EF34` from `Gp_LcgState`: entry `i` is a 0x400-wide
/// draw offset into the quadrant `i & 3`, so the six quads fan around the
/// nozzle. `func_pepper_spray_8012F634` draws one quad per entry every frame.
extern s16 D_pepper_spray_8012FB9C[6];

/// Links the pepper-spray nozzle quad at `arg0`'s world position. The position
/// is projected through `GsWSMATRIX` by a single `RTPS` and the quad is
/// dropped when that sets a negative `gte_stflg`. `arg1` sizes it and `arg2`
/// spins it: the corners sit `arg1 * 0x37 / otz` from the projected centre
/// along `arg2` and `arg2 + 0x400`, so the nozzle shrinks with depth. The
/// texture is the fixed 0x37 x 0x37 patch at (0x70, 0xC8) on tpage 0x29, drawn
/// semi-transparent and unshaded.
void func_pepper_spray_8012F21C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

/// Draws the pepper-spray cone as one Gouraud quad: three corners on a 0x100
/// circle around `arg1` (at `-0xC0`, `0`, `+0xC0`) and one tip twice as far
/// out and 0x200 towards the camera, all in `arg0`'s `workm` frame. `arg2` is
/// the spray brightness; only the corner along `arg1` is lit, with half of
/// `arg2` in red and green and all of it in blue.
void func_pepper_spray_8012F634(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

#endif /* PE_PEPPER_SPRAY_H */
