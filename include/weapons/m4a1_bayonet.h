#ifndef M4A1_BAYONET_H
#define M4A1_BAYONET_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgs.h>
#include "main/coord.h"

/// The blade's motion trail: eight tip and eight hilt coordinate frames,
/// parented to `gGfxViewCoord`. The sweep state overwrites slot
/// `GpEffWork::age & 7` each frame and the ribbon is drawn between the
/// two rings.
extern GpCoord D_m4a1_bayonet_8012D398[8];
extern GpCoord D_m4a1_bayonet_8012D618[8];

/// 0x2C-byte scratch `func_m4a1_bayonet_8011D69C` carves off `G_SCRATCH_HEAD`
/// for one ribbon segment: `v` is the quad's four corners, taken from the
/// translation of the two trail coordinates at each end of the segment, `flag`
/// the `gte_stflg` of the projection (negative rejects the quad) and `otz` its
/// `gte_stszotz`, which picks the OT bucket the `POLY_G4` is linked into.
typedef struct _M4a1BayonetBeamScratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ s32     unused;
} M4a1BayonetBeamScratch;
STATIC_ASSERT_SIZEOF(M4a1BayonetBeamScratch, 0x2C);

#endif
