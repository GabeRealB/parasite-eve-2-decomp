/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turn-aim state body, as in `Actor01900_Fn04D14`: take a 0x10
/// chase scratch off the scratch stack and, on the live-actor flag, key the
/// two animation nodes, the frame counter and the `dashRateStep` clip phase.
/// Once `exitCounter` has counted 7 frames the arm aims at the player - the
/// player's own facing yaw goes in `playerYaw`, the wrapped yaw from the player
/// back to the actor in `yawFromPlayer` - and the root is turned by the facing
/// yaw plus a +-0x60 clamp of the turn's 1000 bias. The forward draw
/// `slideStep` is the doubled frame parameter (halved while `blendActive` is
/// up, forced to 2 while the frame counter runs), and the actor slides along
/// it when the `0x12C` probe reports the step is clear. `dashRateStep` walks 8 ->
/// -1 -> 0 as `animRate` passes 0x18 and 0x12, and the 0 arm runs the
/// five-frame exit window that re-aims once more and picks state 0xB when the
/// actor faces away from the player, else state 0x1A.
void oddStrangerChase(Task* arg0)
{
    ActorChaseScratch* chase;
    ActorChaseScratch* head;
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    s32                turn;
    s32                diffPos;
    s32                diffNeg;
    s32                yaw;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = ODD_STRANGER_SWING_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animId            = 3;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(arg0);
        work->dashRateStep        = 8;
        work->stateTimer          = 0;
        work->exitCounter         = 0;
        gOddStrangerChaseDistance = 0;
        work->dashCount++;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    chase                                   = head - 1;
    arg0->extra.tmd->coords->composeStamp   = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(arg0);
#if ODD_STRANGER_VARIANT == 1
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
#else
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 0) {
#endif
        work->exitCounter++;
    } else {
        oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    if (work->exitCounter >= 7) {
        chase->playerYaw     = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
        chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
        chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
        work->state          = ODD_STRANGER_STATE_SLIDE;
    }
    coord       = arg0->extra.tmd->coords;
    chase->turn = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn        = chase->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (ABS(diffPos) < 0x60) {
            chase->heading = chase->turn - 1000;
        } else if (diffPos > 0) {
            chase->heading = 0x60;
        } else {
            chase->heading = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (ABS(diffNeg) < 0x60) {
            chase->heading = chase->turn + 1000;
        } else if (diffNeg > 0) {
            chase->heading = 0x60;
        } else {
            chase->heading = -0x60;
        }
    }
    facing          = arg0->extra.tmd->coords;
    chase->heading += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->heading, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    coord                                 = arg0->extra.tmd->coords;
    work->lookYawTarget                   = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->slideStep                       = work->animRate * 8;
    if (work->blendActive != 0) {
        work->slideStep = work->slideStep >> 1;
    }
    if (work->exitCounter != 0) {
        work->slideStep = 2;
    }
    if ((detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->slideStep) << 0x10) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, (u16)work->slideStep);
    }
    gOddStrangerChaseDistance += (u16)work->slideStep;
    if (work->dashRateStep == 8 && work->animRate >= 0x18) {
        work->dashRateStep = -1;
    }
    if (work->dashRateStep == -1 && work->animRate == 0x12) {
        work->dashRateStep = 0;
        work->stateTimer   = 0;
    }
    if (work->dashRateStep == 0) {
        if (++work->stateTimer == 5) {
            chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
            chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
            chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
            yaw                  = chase->yawFromPlayer - chase->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw > 0x400
#if ODD_STRANGER_SIGHT_TEST
                && detectSightBlocked(arg0) != 1
#endif
                && work->grabCooldown == 0) {
                work->state = ODD_STRANGER_STATE_GRAB;
            } else {
                work->state     = ODD_STRANGER_STATE_SLIDE;
                work->prevState = -1;
            }
        }
    }
    work->animRate += (u16)work->dashRateStep;
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
