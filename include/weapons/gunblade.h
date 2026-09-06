#ifndef WEAPONS_GUNBLADE_H
#define WEAPONS_GUNBLADE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3FB8.h"
#include "main/task.h"

/// 0x68-byte scratch `func_gunblade_8011E040` carves off `G_SCRATCH_HEAD`.
/// `coord` is the sound source handed to `Gp_PickNearestRec18` and
/// `Gp_PlayObjSfx` (the lock-on target's position is written into its
/// `workm.t`), `dir` receives the blade's forward column from
/// `Gfx_MatrixCol2`, and `step` is that column scaled down by 136 - the
/// per-axis camera shake added to the muzzle coordinate while the slash's
/// recoil timer runs.
typedef struct _GunbladeScratch {
    /* 0x00 */ GsCOORDINATE2 coord;
    /* 0x50 */ VECTOR        step;
    /* 0x60 */ SVECTOR       dir;
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

/// Translation of the gunblade beam's two coordinate frames inside the muzzle
/// frame: `[0]` is the near end (`0, 0x60, 0x80`) and `[1]` the far end
/// (`0, 0x60, 0x380`). `func_gunblade_8011D1E4` seeds the task's own coord
/// from `[0]` and the stack coord from `[1]`.
///
/// `D_gunblade_8011E70C` is that same far-end vector under its own name. State
/// 0 reaches it as `D_gunblade_8011E704[1]`, so the address is derived from the
/// array base already in a register; state 1 loads the symbol on its own. Both
/// spellings are needed to match, and one object cannot carry two C names, so
/// the pair stays in the split data.
extern SVECTOR D_gunblade_8011E704[1];
extern SVECTOR D_gunblade_8011E70C;

/// The eight-segment beam trails, one array per end of the blade. Every entry
/// is parented to `Gfx_ViewCoord`.
extern GsCOORDINATE2 D_gunblade_8012E254[8];
extern GsCOORDINATE2 D_gunblade_8012E4D4[8];

/// The running beam task and its `GpEffWork`, cached on entry to state 0 so
/// `func_gunblade_8011E008` can reach them from outside the task. The work
/// pointer is cleared again when the `Gp_State1C` block is released.
extern Task*      D_gunblade_8012E244;
extern GpEffWork* D_gunblade_8012E248;

/// Draws the beam as seven Gouraud quads, one per trail slot, walking backwards
/// from `slot`. Each quad spans the near and far trail coordinates of two
/// adjacent slots and fades out along the trail: the leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `flags` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade.
void func_gunblade_8011D70C(s16 slot, s16 flags);

void func_gunblade_8011E008(s32 arg0);

#endif
