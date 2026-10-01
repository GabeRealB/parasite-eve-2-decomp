/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Aim step toward the player: the same body as `oddStrangerFacePlayer`
/// with three differences. The wrapped turn is clamped to zero-or-negative
/// rather than +-0x10, so the actor only ever rotates one way; the animation
/// slot is `ODD_STRANGER_HOLD_AIM_CLIP` and `field_8D0.field_1C` is written
/// before the other state words; and the spawn arm clears the `field_6` latch
/// on its way out instead of arming state F0.
void oddStrangerHoldAim(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = ODD_STRANGER_BODY_RADIUS;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = ODD_STRANGER_HOLD_AIM_CLIP;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_BODY2_GRID
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->field_A10.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        oddStrangerDrive(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6 += 1;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 7;
    }
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8AE = aim->turn;
    if (aim->turn > 0) {
        aim->turn = 0;
    }
    if (aim->turn < 0) {
        aim->turn = 0;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    oddStrangerDrive(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
