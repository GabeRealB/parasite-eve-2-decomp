/* Part of the Mad Chaser library; see mad_chaser.h. */

#ifndef SRC_SHARED_MAD_CHASER_FRAME_SHADOWS
#define SRC_SHARED_MAD_CHASER_FRAME_SHADOWS

/// Draws the Mad Chaser's main span and two branch segments as ground shadows.
///
/// Borrows `task->extra.tmd`, which must have at least nine live coordinates
/// and valid parent chains through an orthonormal view coordinate. Draws
/// 2-to-6 first with half-width 200, then 1-to-7 and 7-to-8 with half-width
/// 128, in world units. All three lie at world Y zero, use subtractive blending
/// and grey texture modulation 255, and narrow positions to signed halfwords.
/// The caller decides visibility; task state, work and model draw flags are
/// neither checked nor changed here. Each segment can be rejected by its GTE
/// projection status, so the call queues zero to three quads.
///
/// Requires initialized GTE projection, 192 free aligned scratch-stack bytes,
/// a current 1024-depth-tag ordering table and word-aligned frame-arena room
/// for three `POLY_FT4` packets. Each draw reloads the task's live model and
/// refreshes coordinate caches, including the view; GTE state is changed.
/// Scratch reservations are released between draws. Queued packets remain
/// live through frame DMA.
static __inline__ void _madChaserDrawFrameShadows(Task* task)
{
    enum {
        MAD_CHASER_FRAME_SHADOW_MAIN_START_PART   = 2,
        MAD_CHASER_FRAME_SHADOW_MAIN_END_PART     = 6,
        MAD_CHASER_FRAME_SHADOW_BRANCH_BASE_PART  = 1,
        MAD_CHASER_FRAME_SHADOW_BRANCH_JOINT_PART = 7,
        MAD_CHASER_FRAME_SHADOW_BRANCH_TIP_PART   = 8,
        MAD_CHASER_FRAME_SHADOW_WIDE_HALF_WIDTH   = 200,
        MAD_CHASER_FRAME_SHADOW_NARROW_HALF_WIDTH = 128,
        MAD_CHASER_FRAME_SHADOW_WORLD_Y           = 0,
        MAD_CHASER_FRAME_SHADOW_SHADE             = 255
    };

    _madChaserDrawLimbShadow(task, MAD_CHASER_FRAME_SHADOW_MAIN_START_PART, MAD_CHASER_FRAME_SHADOW_MAIN_END_PART,
                             MAD_CHASER_FRAME_SHADOW_WIDE_HALF_WIDTH, MAD_CHASER_FRAME_SHADOW_WORLD_Y, MAD_CHASER_FRAME_SHADOW_SHADE);
    _madChaserDrawLimbShadow(task, MAD_CHASER_FRAME_SHADOW_BRANCH_BASE_PART, MAD_CHASER_FRAME_SHADOW_BRANCH_JOINT_PART,
                             MAD_CHASER_FRAME_SHADOW_NARROW_HALF_WIDTH, MAD_CHASER_FRAME_SHADOW_WORLD_Y, MAD_CHASER_FRAME_SHADOW_SHADE);
    _madChaserDrawLimbShadow(task, MAD_CHASER_FRAME_SHADOW_BRANCH_JOINT_PART, MAD_CHASER_FRAME_SHADOW_BRANCH_TIP_PART,
                             MAD_CHASER_FRAME_SHADOW_NARROW_HALF_WIDTH, MAD_CHASER_FRAME_SHADOW_WORLD_Y, MAD_CHASER_FRAME_SHADOW_SHADE);
}

#endif
