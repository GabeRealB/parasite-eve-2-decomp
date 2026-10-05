/* Part of the walker library; see walker.h. */

/// Draws the walker's ground shadow, a 0x200 quad at the room's shadow shade,
/// under the model root, unless the model is hidden or has no buffer yet.
void walkerDrawShadowShaded(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_STACK_RESERVE_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        effectDrawGroundShadow(vec, 0x200, gRoomEffectState->groundShadowShade);
        SCRATCH_STACK_RELEASE_BYTES(0x18);
    }
}
