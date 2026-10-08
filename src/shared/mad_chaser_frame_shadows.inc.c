/* Part of the Mad Chaser library; see mad_chaser.h. */

#ifndef SRC_SHARED_MAD_CHASER_FRAME_SHADOWS
#define SRC_SHARED_MAD_CHASER_FRAME_SHADOWS

/// Draws the three ground shadows used by the Mad Chaser frame callbacks.
///
/// Requires a live model with coordinates 1..8, initialized projection/scratch
/// storage and frame-arena room for three shadow quads. The 2-to-6 segment has
/// half-width 200; 1-to-7 and 7-to-8 have half-width 128, in world-coordinate
/// units. All lie at world Y zero with grey texture modulation 255. Each draw
/// refreshes its coordinate caches; queued packets live through frame DMA.
static __inline__ void _madChaserDrawFrameShadows(Task* task)
{
    enum {
        MAD_CHASER_FRAME_SHADOW_WIDE_HALF_WIDTH   = 200,
        MAD_CHASER_FRAME_SHADOW_NARROW_HALF_WIDTH = 128,
        MAD_CHASER_FRAME_SHADOW_WORLD_Y           = 0,
        MAD_CHASER_FRAME_SHADOW_SHADE             = 255
    };

    _madChaserDrawLimbShadow(task, 2, 6, MAD_CHASER_FRAME_SHADOW_WIDE_HALF_WIDTH, MAD_CHASER_FRAME_SHADOW_WORLD_Y, MAD_CHASER_FRAME_SHADOW_SHADE);
    _madChaserDrawLimbShadow(task, 1, 7, MAD_CHASER_FRAME_SHADOW_NARROW_HALF_WIDTH, MAD_CHASER_FRAME_SHADOW_WORLD_Y, MAD_CHASER_FRAME_SHADOW_SHADE);
    _madChaserDrawLimbShadow(task, 7, 8, MAD_CHASER_FRAME_SHADOW_NARROW_HALF_WIDTH, MAD_CHASER_FRAME_SHADOW_WORLD_Y, MAD_CHASER_FRAME_SHADOW_SHADE);
}

#endif
