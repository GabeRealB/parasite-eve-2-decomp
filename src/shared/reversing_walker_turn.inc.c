/* Part of the reversing walker library; see reversing_walker.h. */

/// State handler at index 3 of `D_actor_350700_80161E30`, the turn-to-face body
/// that follows `reverseWalkFaceTarget`. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the target `work->walk.targetRot.vy` is at least
/// 0x61 it steps `vec.vy` toward it by 0x60 -- the step is taken on an `s32`
/// widening of the extracted yaw -- and otherwise snaps the yaw to the target
/// and plays anim 0x7D3, clearing the two body counters. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, which
/// `actorRenderComposeCoordChain` picks up once `composeStamp` is cleared.
void reverseWalkTurnToYaw(Task* arg0)
{
    ReverseWalkWork*     work;
    GfxRotationWords*    words;
    GfxCoord*            coord;
    SVECTOR              vec;
    AnimationPlayRequest preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    gfxExtractSmallestEuler(&vec, &coord->coord);
    diff = (u16)work->walk.targetRot.vy - (u16)vec.vy;
    if (ABS(diff) >= 0x61) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x60;
        } else {
            vec.vy = vy + 0x60;
        }
    } else {
        vec.vy                      = work->walk.targetRot.vy;
        preset.source.index         = 0;
        preset.animationId          = 1;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 4;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = 0;
    }

    words         = (GfxRotationWords*)&coord->coord;
    words->m00M01 = ONE;
    words->m02M10 = 0;
    words->m11M12 = ONE;
    words->m20M21 = 0;
    words->m22    = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
