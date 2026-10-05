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

/// Packed trail tints: red at bit 8, green at bit 4 and blue at bit 0.
///
/// Each channel is an unshifted multiplier in 0..3; the intervening bits are
/// zero. The drawer multiplies these channels by each edge's fading brightness.
enum {
    BLADE_TRAIL_TINT_BLUE_WHITE   = 0x112, // Red 1, green 1, blue 2
    BLADE_TRAIL_TINT_YELLOW_WHITE = 0x331  // Red 3, green 3, blue 1
};

/// Scratch-stack workspace for one quad of a blade trail.
///
/// `_bladeTrailDraw` reserves one block and reuses it for each of the seven
/// quads between the blade's base and tip rings. The corners are those frames'
/// world translations, narrowed to signed 16-bit coordinate units. Corner 0 is
/// the newer base and is projected on its own. Corners 1, 2 and 3 are the newer
/// tip, the older base and the older tip, projected together. Screen positions
/// are stored straight into the gouraud quad, so the block keeps none of them.
///
/// `projectionFlags` is the GTE flag word of that three-vertex transform. A
/// negative word skips the quad. Otherwise `otz` is SZ3 / 4 of the same
/// transform, the older tip's screen depth, and it selects the ordering-table
/// bucket and the blend packet's depth.
///
/// The last word is never read or written. Its role is unproven. Reserve the
/// complete block and release it before any pointer into it is used again.
typedef struct {
    SVECTOR worldCorners[4]; // [0] newer base, [1] newer tip, [2] older base, [3] older tip
    s32     otz;             // SZ3 / 4 of the three-vertex transform; ordering and blend depth
    s32     projectionFlags; // GTE FLAG word of that transform; bit 31 set skips the quad
    s32     field_28;        // Role unproven; the drawer never reads or writes this word
} BladeTrailScratch;
STATIC_ASSERT_SIZEOF(BladeTrailScratch, 0x2C);

static void _bladeTrailDraw(s16 newestSlot, s16 packedTint);

#endif /* SRC_SHARED_BLADE_TRAIL_H */
