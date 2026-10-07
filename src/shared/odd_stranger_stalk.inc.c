/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State-2 aim body, as in the Horned Stranger's `func_actor_401300_8013CBAC` and
/// `Actor01900_Fn042BC`: on the live-actor flag it resets the effect node, forks
/// the first clip and seeds the animation slots, then walks both obstacle tables
/// and aims the actor at the player with `gfxRotMatrixY` / `_actorRenderRescaleYaw`.
/// `stateTimer` and `exitCounter` then count up under the `_playerDetectionSightBlocked`
/// clip test: the still-aiming arm re-wraps the turn, drops the actor to state
/// 0xB once the 0x44C range check fails inside 0x200 and re-arms at 0x1B past
/// 0x5B frames, while the settled arm draws a turn direction from `gRandomLcgState`
/// and flips it every 0xF1 frames. The tail takes one forward step off the
/// 0x12C probe, or re-arms the clip on the `rig.slots[1].status` bit. `grabCooldown` counts
/// down once per entry.
void oddStrangerStalk(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* head;
    ActorChaseScratch* chase;
#if ODD_STRANGER_VARIANT == 1
    s16 yaw;
#endif

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ODD_STRANGER_STALK_RATE;
        work->animId            = 2;
        work->blendActive       = 0;
        work->dashCount         = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        sceneEngageBattle(1);
        work->stateTimer  = 0;
        work->exitCounter = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    work->stateTimer++;
    work->exitCounter++;
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    chase                                   = head - 1;
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 1) {
            oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(arg0);
    chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                              (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
#if ODD_STRANGER_VARIANT == 1
    yaw                  = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
    chase->yawFromPlayer = yaw;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(yaw);
    coord                = arg0->extra.tmd->coords;
    chase->turn          = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->lookYawTarget  = chase->turn;
#else
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
#endif
    if (_playerDetectionSightBlocked(arg0) != 1) {
        work->stateTimer    = 0;
        coord               = arg0->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (chase->turn < 0x200) {
            if (!_oddStrangerOutOfRange(&chase->delta, 0x44C) && work->grabCooldown == 0) {
                work->state = ODD_STRANGER_STATE_GRAB;
            }
        }
        if (work->exitCounter >= 0x5B) {
            work->state = ODD_STRANGER_STATE_WATCH;
        }
    } else {
        work->exitCounter = 0;
#if ODD_STRANGER_VARIANT == 2
        work->stateTimer++;
#endif
        coord               = arg0->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = -1;
            } else {
                work->sidestepSide = 1;
            }
        }
        if (work->sidestepSide == 1) {
            chase->turn += ODD_STRANGER_STALK_TURN_BIAS;
        } else {
            chase->turn -= ODD_STRANGER_STALK_TURN_BIAS;
        }
        if (work->stateTimer >= 0xF1) {
            work->stateTimer   = 0;
            work->sidestepSide = -work->sidestepSide;
        }
    }
    if (chase->turn > 0x20) {
        chase->turn = 0x20;
    }
    if (chase->turn < -0x20) {
        chase->turn = -0x20;
    }
    facing       = arg0->extra.tmd->coords;
    chase->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->turn, 1);
    _actorRenderRescaleYaw(arg0->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == 2) {
        if (work->blendActive == 0) {
            if ((s16)_playerDetectionOutOfReach(arg0->extra.tmd->coords, 0x12C, ODD_STRANGER_STALK_STEP) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, ODD_STRANGER_STALK_STEP);
            }
        } else {
            if ((s16)_playerDetectionOutOfReach(arg0->extra.tmd->coords, 0x12C, 5) != 0) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 5);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->animId      = 2;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
    }
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
