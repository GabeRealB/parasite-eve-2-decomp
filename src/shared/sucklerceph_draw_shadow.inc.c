#include "gameplay/actor_render_shadow_types.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Draws the Sucklerceph's textured ground shadow at its composed root.
///
/// Requires a live model with an up-to-date root `workm`. The square has a
/// half-side of 448 game-coordinate units and uses the texture without colour
/// modulation. Reserves the full 24-byte centre block for the draw call;
/// only its first three words are accessed, and the trailing role is unproven.
static void _sucklercephDrawShadow(Task* task)
{
    enum { SUCKLERCEPH_SHADOW_HALF_SIDE   = 448,
           SUCKLERCEPH_SHADOW_RAW_TEXTURE = 0 };

    GfxCoord*                             rootCoord;
    ActorRenderGroundShadowCentreScratch* shadowScratch;

    rootCoord                = task->extra.tmd->coords;
    shadowScratch            = SCRATCH_STACK_RESERVE_BLOCK(ActorRenderGroundShadowCentreScratch);
    shadowScratch->centre.vx = rootCoord->workm.t[0];
    shadowScratch->centre.vy = rootCoord->workm.t[1];
    shadowScratch->centre.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&shadowScratch->centre, SUCKLERCEPH_SHADOW_HALF_SIDE, SUCKLERCEPH_SHADOW_RAW_TEXTURE);
    SCRATCH_STACK_RELEASE_BLOCK(ActorRenderGroundShadowCentreScratch);
}
