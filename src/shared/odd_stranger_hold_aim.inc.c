/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Aim step toward the player: the same body as `oddStrangerFacePlayer`
/// with three differences. The wrapped turn is clamped to zero-or-negative
/// rather than +-0x10, so the actor only ever rotates one way; the animation
/// slot is `ODD_STRANGER_HOLD_AIM_CLIP` and `hitBody.field_1C` is written
/// before the other state words; and the spawn arm clears the `stateTimer` latch
/// on its way out instead of arming state F0.
void oddStrangerHoldAim(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         obj;
    GfxCoord*          coord;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = ODD_STRANGER_HOLD_AIM_CLIP;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_BODY2_GRID
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        oddStrangerDrive(arg0);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer += 1;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    aim->turn           = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
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
