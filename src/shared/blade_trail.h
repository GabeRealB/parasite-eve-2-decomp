/* The swoosh the melee weapons (Gunblade, M4A1 bayonet, tonfa baton) leave
 * behind a swing. Two 8-entry rings of coordinates record the blade's base and
 * tip each frame, and the draw helper joins seven adjacent ring slots, walking
 * back from the newest, into Gouraud quads that fade along the trail, coloured
 * by a packed 2-bit-per-channel tint.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BLADE_TRAIL_H
#define SRC_SHARED_BLADE_TRAIL_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// 0x2C-byte scratch `bladeTrailDraw` carves off the scratch stack for
/// one beam segment: `v` is the quad's four corners, taken from the
/// translation of the two trail coordinates at each end of the segment, `flag`
/// the `gte_stflg` of the projection (negative rejects the quad) and `otz` its
/// `gte_stszotz`, which picks the OT bucket the `POLY_G4` is linked into.
typedef struct BladeTrailScratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ s32     unused;
} BladeTrailScratch;
STATIC_ASSERT_SIZEOF(BladeTrailScratch, 0x2C);

void bladeTrailDraw(s16 slot, s16 flags);

#endif /* SRC_SHARED_BLADE_TRAIL_H */
