/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turn the actor toward the player, clamped to +-0x10 past its current facing,
/// then rebuild the root coordinate scaled by 0x1194. Same body as
/// the Horned Stranger's `func_actor_401300_80136238`, minus that one's `field_8B6` state pair and
/// its message-id gate, which the Odd Stranger keeps in `downFramesBase`.
void oddStrangerFacePlayer(Task* arg0)
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
        tmdAllocPrimitiveBuffer(obj);
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 9;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(arg0);
        work->hitBody.radius = ODD_STRANGER_BODY_RADIUS;
        sceneEngageBattle(1);
        return;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                                   = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    aim->turn           = _actorAngleTurnToPlayer(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (aim->turn > 0x10) {
        aim->turn = 0x10;
    }
    if (aim->turn < -0x10) {
        aim->turn = -0x10;
    }
    coord      = arg0->extra.tmd->coords;
    aim->turn += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    _oddStrangerDriveAnimation(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
