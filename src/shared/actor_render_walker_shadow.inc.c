/* Private walker-shadow instance; bind ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW
 * to a static void(Task*) function declared in the carrier's prologue. */

#ifndef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW
#error "Bind ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW before including this fragment"
#endif

/// Draws the second walker's ground shadow using the room's shadow shade.
///
/// `task` must own a live TMD model whose root view transform is composed.
/// Hidden models and models without a primitive buffer draw nothing. The quad
/// has a 512-unit half-side before view-frame rotation and uses the root's
/// cached view-space translation, without a ground-height query. A negative
/// room shade or hidden room effects suppress drawing in `effectDrawGroundShadow`.
/// The scratch stack needs 80 free, word-aligned bytes for this reservation and
/// the renderer's nested block; the centre is borrowed until the draw call returns.
static void ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW(Task* task)
{
    enum {
        ACTOR_RENDER_WALKER_SHADOW_HALF_SIZE     = 512,
        ACTOR_RENDER_WALKER_SHADOW_SCRATCH_BYTES = 24
    };
    const TmdObject* model;
    const GfxCoord*  rootCoord;
    VECTOR3*         shadowCentre;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && model->buffer != NULL) {
        // Keep the full reservation; only its first three words hold the centre.
        shadowCentre     = SCRATCH_STACK_RESERVE_BYTES(ACTOR_RENDER_WALKER_SHADOW_SCRATCH_BYTES);
        shadowCentre->vx = rootCoord->workm.t[0];
        shadowCentre->vy = rootCoord->workm.t[1];
        shadowCentre->vz = rootCoord->workm.t[2];
        effectDrawGroundShadow(shadowCentre, ACTOR_RENDER_WALKER_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        SCRATCH_STACK_RELEASE_BYTES(ACTOR_RENDER_WALKER_SHADOW_SCRATCH_BYTES);
    }
}
