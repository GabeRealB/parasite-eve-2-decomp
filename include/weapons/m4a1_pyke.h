#ifndef M4A1_PYKE_H
#define M4A1_PYKE_H

#include "common.h"
#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "main/session.h"
#include "gameplay/3CD8.h"

/// 0x38 block the flying dart's spawn state allocates with `memCalloc` and
/// parks in `Task::work`. It leads with the `GpObj` list node
/// `func_m4a1_pyke_8011E4AC` hands back to `Gp_UnlinkObj` on teardown; `rec` is
/// the single-entry `GpRec18` collision table `obj.ctx.recs` points at, and its
/// `flags` is set to 2 (the last-element bit) instead of going through
/// `Gp_InitRec18Table`.
typedef struct M4a1PykeBeam {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[1];
} M4a1PykeBeam;
STATIC_ASSERT_SIZEOF(M4a1PykeBeam, 0x38);

/// 0x30-byte scratch from `G_SCRATCH_HEAD` used by `func_m4a1_pyke_8011E168`
/// for the dart's ground splash. `vec` holds the four corners of the unit quad
/// `D_80111E38`, scaled to the splash half-size, rotated flat into view space
/// by `gGfxViewCoord.workm` and translated to `pos`; `sxy` is where they project
/// to on screen, `vec[0]` through a single `RTPS` and the rest through one
/// `RTPT`. Same shape as the gameplay `GpQuadScratch`, but with `otz` and
/// `flag` kept on the stack instead of in the block.
typedef struct M4a1PykeSplashScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ DVECTOR sxy[4];
} M4a1PykeSplashScratch;
STATIC_ASSERT_SIZEOF(M4a1PykeSplashScratch, 0x30);

#endif
