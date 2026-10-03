/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Walk the actor at the player: on the live-actor flag it restarts the
/// 0x12 clip and clears the spawn pose, then takes a 0xC-byte scratch stack
/// turn block, aims it at `gPlayerStatus.coordMtx` through
/// `actorPositionYaw`, clamps the turn to +-0x40 and adds the facing
/// yaw back in before rebuilding the root coordinate. The obstacle walk
/// `ActorContact_PushContact` runs against `field_A30` and hands
/// `field_8F0` to `oddStrangerPushContacts` when it reports a hit, the
/// `detectPlayerOutOfReach` probe takes one forward step out of
/// `field_C04`, and that same countdown then runs down by 0xA a frame. The
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
    if (work->field_4 != 0) {
        enemy           = arg0->spawnArg2.pointer;
        obj             = arg0->extra.tmd;
        work->field_89E = 0x12;
        work->field_898 = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius        = ODD_STRANGER_BODY_RADIUS;
        work->field_B50.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->field_8B0               = 0;
        work->field_8A2               = 0x1E;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn            = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn->angle     = actorPositionYaw(arg0, &turn->delta, &gPlayerStatus);
    work->field_8AE = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->extra.tmd->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
        oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
    }
    if ((detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->field_C04) << 0x10) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, (u16)work->field_C04);
    }
    if (work->field_C04 > 0) {
        work->field_C04 = (u16)work->field_C04 - 0xA;
        if (work->field_C04 < 0) {
            work->field_C04 = 0;
        }
    }
    oddStrangerDrive(arg0);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) || work->field_C04 == 0) {
        work->field_0 = 9;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}
