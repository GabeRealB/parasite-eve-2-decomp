/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Aim step toward the player, as in the Horned Stranger's `func_actor_401300_8013A5C0`. Unlike
/// `oddStrangerFacePlayer` the wrapped turn is halved and clamped to
/// +-0x80 instead of +-0x10, so the actor turns at half speed and only the
/// 0x16/2 -> 0x16/0x11 spawn pair keys the follow-up; the spawn arm writes
/// `hitBody.field_1C` first and leaves `stateTimer` alone, and the exit turn
/// reads the sign of `lookYawTarget` with the `0x4B0` arm first.
void oddStrangerBackOff(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = 0x16;
        work->animId            = 2;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(arg0);
        return;
    }
    _oddStrangerDriveAnimation(arg0);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn           = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->lookYawTarget = aim->turn;
    if (ABS(aim->turn) < 0x81 && work->animId == 2) {
        work->animRate    = 0x16;
        work->animId      = 0x11;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        _oddStrangerDriveAnimation(arg0);
    }
    if (aim->turn >= 0x81) {
        aim->turn = 0x80;
    }
    if (aim->turn < -0x80) {
        aim->turn = -0x80;
    } else {
        aim->turn = aim->turn >> 1;
    }
    aim->turn += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, aim->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 0x11) {
        work->stateTimer++;
        if ((detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, -0x10) << 16) != 0) {
            _actorMovementStepForward(arg0->extra.tmd->coords, -0x10);
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
            oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if ((s16)work->stateTimer >= 0x13) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->state = ODD_STRANGER_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
