/* Private fixed-shade walker shadow; see walker.h for instance bindings. */

#ifndef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW
#error "Bind ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW before including this fragment"
#endif

/// Draws a walker's subtractive ground shadow at a fixed shade of 192.
///
/// `task` must own a live TMD model with a composed root view transform.
/// Hidden models and models without a primitive buffer draw nothing. The quad
/// has a 512-unit half-side before view-frame rotation and is centred on the
/// root's cached view-space translation, without querying ground height.
/// Hidden or cancelled room effects suppress drawing in `effectDrawGroundShadow`.
/// The scratch stack needs 80 free, word-aligned bytes for this block and the
/// renderer's nested reservation; the centre is borrowed until drawing returns.
static void ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW(Task* task)
{
    enum {
        ACTOR_RENDER_FIXED_WALKER_SHADOW_HALF_SIZE = 512,
        ACTOR_RENDER_FIXED_WALKER_SHADOW_SHADE     = 192
    };
    const TmdObject*                      model;
    const GfxCoord*                       rootCoord;
    ActorRenderGroundShadowCentreScratch* shadowScratch;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && model->buffer != NULL) {
        // Preserve the whole reservation; only the centre's three words are used.
        shadowScratch            = SCRATCH_STACK_RESERVE_BLOCK(ActorRenderGroundShadowCentreScratch);
        shadowScratch->centre.vx = rootCoord->workm.t[0];
        shadowScratch->centre.vy = rootCoord->workm.t[1];
        shadowScratch->centre.vz = rootCoord->workm.t[2];
        effectDrawGroundShadow(&shadowScratch->centre, ACTOR_RENDER_FIXED_WALKER_SHADOW_HALF_SIZE, ACTOR_RENDER_FIXED_WALKER_SHADOW_SHADE);
        SCRATCH_STACK_RELEASE_BLOCK(ActorRenderGroundShadowCentreScratch);
    }
}
