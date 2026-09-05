#ifndef M4A1_BAYONET_H
#define M4A1_BAYONET_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// The two translations the bayonet's coordinate frames sit at inside their
/// parent (the muzzle): `[0]` `(0, 0x300, 0x40)` is the blade tip used while
/// the thrust is winding up and `[1]` `(0, 0x180, 0x40)` the hilt. The second
/// element is also labelled `D_m4a1_bayonet_8011DED0` because the sweep state
/// addresses it as a symbol of its own.
extern SVECTOR D_m4a1_bayonet_8011DEC8[2];

/// `D_m4a1_bayonet_8011DEC8[1]` under its own label. State 0 reaches the hilt
/// through the array base already in a register, the sweep state loads this
/// symbol on its own, and both spellings are needed to match - so the pair
/// stays in the split data, where one address can carry two names.
extern SVECTOR D_m4a1_bayonet_8011DED0;

/// The blade's motion trail: eight tip and eight hilt coordinate frames,
/// parented to `Gfx_ViewCoord`. The sweep state overwrites slot
/// `GpEffWork::field_22 & 7` each frame and the ribbon is drawn between the
/// two rings.
extern GsCOORDINATE2 D_m4a1_bayonet_8012D398[8];
extern GsCOORDINATE2 D_m4a1_bayonet_8012D618[8];

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

/// Draws the blade trail as seven Gouraud quads, one per trail slot, walking
/// backwards from `slot`. Each quad spans the tip and hilt coordinates of two
/// adjacent slots and fades out along the ribbon: the leading edge is scaled
/// by `0x40 - 9 * i` and the trailing edge by nine less. `flags` is the trail
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade.
void func_m4a1_bayonet_8011D69C(s16 slot, s16 flags);

#endif
