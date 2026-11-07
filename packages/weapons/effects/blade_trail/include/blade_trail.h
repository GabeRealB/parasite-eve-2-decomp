/* The swoosh the melee weapons (Gunblade, M4A1 Bayonet, tonfa baton) leave behind a swing: two 8-slot rings record
 * the blade's two ends each frame, and the draw joins the seven spans back from the newest into Gouraud quads that
 * fade along the trail, colored by a packed tint.
 *
 * draw.inc.c holds the draw; before including it the carrier defines the rings, `static GfxCoord
 * _gBladeTrailFirstHistory[8]` and `_gBladeTrailSecondHistory[8]`.
 */

#ifndef WEAPONS_BLADE_TRAIL_H
#define WEAPONS_BLADE_TRAIL_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include <core/pe2.h>

#include <gameplay/hud_sprites.h>

#include <engine/gfx.h>

/// Packed trail tints: a 0..3 multiplier per channel, red at bit 8, green at bit 4 and blue at bit 0, which scales
/// each edge's fading brightness.
enum {
    BLADE_TRAIL_TINT_BLUE_WHITE   = 0x112, // Red 1, green 1, blue 2
    BLADE_TRAIL_TINT_YELLOW_WHITE = 0x331  // Red 3, green 3, blue 1
};

/// Workspace of `_bladeTrailDraw` for one quad of the trail, reused for each of the seven.
typedef struct {
    SVECTOR worldCorners[4]; // View-space translations: newer first, newer second, older first, older second end
    s32     otz;             // The older second end's depth (SZ3 / 4): ordering-table bucket and blend depth
    s32     projectionFlags; // GTE flags of the three-corner projection; negative skips the quad
    s32     field_28;        // Never accessed
} BladeTrailScratch;
STATIC_ASSERT_SIZEOF(BladeTrailScratch, 0x2C);

/// Saves a blade end's pose in a trail slot, parented to the view, so the slot keeps its place in the world as the
/// weapon moves on; the trail reads only its translation.
static inline void _bladeTrailStoreFrame(GfxCoord* historyFrame, const GfxCoord* composedEndpoint)
{
    historyFrame->parent = &gGfxViewCoord;
    historyFrame->workm  = composedEndpoint->workm;
    gte_SetRotMatrix(&composedEndpoint->workm);
    gte_SetTransMatrix(&composedEndpoint->workm);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &historyFrame->workm, &historyFrame->coord);
}

static void _bladeTrailDraw(s16 newestSlot, s16 packedTint);

#endif /* WEAPONS_BLADE_TRAIL_H */
