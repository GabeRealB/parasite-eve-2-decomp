#ifndef WEAPONS_TONFA_BATON_H
#define WEAPONS_TONFA_BATON_H

#include "main/task_types.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/coord.h"

/// 0x18-byte scratchpad block `func_tonfa_baton_8011DBFC` reserves for one
/// frame of the swing. `dir` receives the third column of the weapon's
/// coordinate matrix from `Gfx_MatrixCol2`; each axis is then scaled to
/// 1/84th and multiplied by the swing flag to give the per-frame translation
/// added to the coordinate.
typedef struct TonfaSwing {
    /* 0x00 */ s32     vx;
    /* 0x04 */ s32     vy;
    /* 0x08 */ s32     vz;
    /* 0x0C */ byte    pad_C[4];
    /* 0x10 */ SVECTOR dir;
} TonfaSwing;
STATIC_ASSERT_SIZEOF(TonfaSwing, 0x18);

/// The eight-segment swing trails, one array per end of the baton. Every entry
/// is parented to `gGfxViewCoord`.
extern GpCoord D_tonfa_baton_8012BBEC[8];
extern GpCoord D_tonfa_baton_8012BE6C[8];

/// Primitive/blend selector for the trail, seeded by state 0 from
/// `Task::spawnArg1` and passed to `func_tonfa_baton_8011D6B0` every frame.
extern s16 D_tonfa_baton_8012C0EC;

/// 0x2C-byte scratch `func_tonfa_baton_8011D6B0` carves off `G_SCRATCH_HEAD`
/// for one trail segment: `v` is the quad's four corners, taken from the
/// translation of the two trail coordinates at each end of the segment, `flag`
/// the `gte_stflg` of the projection (negative rejects the quad) and `otz` its
/// `gte_stszotz`, which picks the OT bucket the `POLY_G4` is linked into.
typedef struct _TonfaBeamScratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ s32     unused;
} TonfaBeamScratch;
STATIC_ASSERT_SIZEOF(TonfaBeamScratch, 0x2C);

void func_tonfa_baton_8011D1EC(Task* task);

#endif
