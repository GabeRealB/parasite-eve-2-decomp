/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Walk the actor at the player: on the live-actor flag it restarts the
/// 0x12 clip and clears the spawn pose, then takes a 0xC-byte scratch stack
/// turn block, aims it at `gPlayerStatus.coordMtx` through
/// `actorPositionYaw`, clamps the turn to +-0x40 and adds the facing
/// yaw back in before rebuilding the root coordinate. The obstacle walk
/// `ActorContact_PushContact` runs against `gridContacts` and hands
/// `hitContacts` to `oddStrangerPushContacts` when it reports a hit, the
/// `detectPlayerOutOfReach` probe takes one forward step out of
/// `slideStep`, and that same countdown then runs down by 0xA a frame. The
/// tail drops the actor to state 9 on the `rig.slots[1].status` bit or once the
/// countdown is spent.
void oddStrangerAdvance(Task* arg0)
{
    OddStrangerWork*  work;
    Enemy*            enemy;
    TmdObject*        obj;
    GfxCoord*         coord;
    ActorTurnScratch* turn;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy             = arg0->spawnArg2.pointer;
        obj               = arg0->extra.tmd;
        work->animId      = 0x12;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        obj->flags        = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x1E;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn                = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->angle         = actorPositionYaw(arg0, &turn->delta, &gPlayerStatus);
    work->lookYawTarget = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    if ((detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->slideStep) << 0x10) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, (u16)work->slideStep);
    }
    if (work->slideStep > 0) {
        work->slideStep = (u16)work->slideStep - 0xA;
        if (work->slideStep < 0) {
            work->slideStep = 0;
        }
    }
    oddStrangerDrive(arg0);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) || work->slideStep == 0) {
        work->state = ODD_STRANGER_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}
