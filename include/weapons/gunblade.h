#ifndef WEAPONS_GUNBLADE_H
#define WEAPONS_GUNBLADE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/effects.h"

#include "main/coord.h"
#include "main/task_types.h"

/// 0x68-byte scratch `func_gunblade_8011E040` carves off `G_SCRATCH_HEAD`.
/// `coord` is the sound source handed to `Gp_PickNearestRec18` and
/// `Gp_PlayObjSfx` (the lock-on target's position is written into its
/// `workm.t`), `dir` receives the blade's forward column from
/// `Gfx_MatrixCol2`, and `step` is that column scaled down by 136 - the
/// per-axis camera shake added to the muzzle coordinate while the slash's
/// recoil timer runs.
typedef struct _GunbladeScratch {
    /* 0x00 */ GpCoord coord;
    /* 0x50 */ VECTOR  step;
    /* 0x60 */ SVECTOR dir;
} GunbladeScratch;
STATIC_ASSERT_SIZEOF(GunbladeScratch, 0x68);

/// 0x2C-byte scratch `func_gunblade_8011D70C` carves off `G_SCRATCH_HEAD` for
/// one beam segment: `v` is the quad's four corners, taken from the
/// translation of the two trail coordinates at each end of the segment, `flag`
/// the `gte_stflg` of the projection (negative rejects the quad) and `otz` its
/// `gte_stszotz`, which picks the OT bucket the `POLY_G4` is linked into.
typedef struct _GunbladeBeamScratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ s32     unused;
} GunbladeBeamScratch;
STATIC_ASSERT_SIZEOF(GunbladeBeamScratch, 0x2C);

/// The eight-segment beam trails, one array per end of the blade. Every entry
/// is parented to `gGfxViewCoord`.
extern GpCoord D_gunblade_8012E254[8];
extern GpCoord D_gunblade_8012E4D4[8];

/// The running beam task and its `GpEffWork`, cached on entry to state 0 so
/// `func_gunblade_8011E008` can reach them from outside the task. The work
/// pointer is cleared again when the `Gp_State1C` block is released.
extern Task*      D_gunblade_8012E244;
extern GpEffWork* D_gunblade_8012E248;

void func_gunblade_8011E008(s32 arg0);

#endif
