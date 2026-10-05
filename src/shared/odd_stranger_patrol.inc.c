/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Walk the actor along its `patrolPoints` waypoint pair: `patrolTarget` picks the
/// waypoint the offset is taken from and flips once the actor closes inside
/// 0xA0 of it, or after 0x15 frames in `stateTimer`, and the wrapped yaw toward
/// that waypoint is clamped to +-0x20, added back to the facing yaw and the
/// root rotation rescaled by 0x1194. The `detectPlayerOutOfReach` probe
/// takes one 0xA step forward, the obstacle walk runs against `gridContacts`
/// (plus `hitContacts` through `oddStrangerPushContacts` when the spawn
/// sub-type is 0x10), and each arm counts `stateTimer` up while the yaw stays
/// inside 0x80.
/// The tail drops the actor to state 6 on the `gPlayerStatus` range checks and
/// the `gSceneCombatState` bits, and the live-actor arm restarts the 0x1AE clip.
void oddStrangerPatrol(Task* arg0)
{
    OddStrangerWork*       work;
    ActorTurnScratch*      turn;
    TmdObject*             obj;
    WorldCollisionContact* rec;
    GfxCoord*              coord;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 2;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        work->stateTimer = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    } else {
        SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
        turn           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
        turn->delta.vx = work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0];
        turn->delta.vy = 0;
        turn->delta.vz = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
#if ODD_STRANGER_VARIANT == 2
        turn->delta.vx = work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0];
        turn->delta.vy = 0;
        turn->delta.vz = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
#endif
        if (!oddStrangerOutOfRange(&turn->delta, 0xA0) || work->stateTimer >= 0x15) {
            if (work->patrolTarget == 0) {
                work->patrolTarget = 1;
            } else {
                work->patrolTarget = 0;
            }
            work->stateTimer = 0;
        }
        oddStrangerDrive(arg0);
        coord               = arg0->extra.tmd->coords;
        turn->angle         = actorNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->lookYawTarget = turn->angle;
        if (turn->angle > ODD_STRANGER_PATROL_TURN_CLAMP) {
            turn->angle = ODD_STRANGER_PATROL_TURN_CLAMP;
        }
        if (turn->angle < -ODD_STRANGER_PATROL_TURN_CLAMP) {
            turn->angle = -ODD_STRANGER_PATROL_TURN_CLAMP;
        }
        turn->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        if (work->blendActive == 0 && (detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ODD_STRANGER_WALK_STEP) << 16) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, ODD_STRANGER_WALK_STEP);
        }
#if ODD_STRANGER_VARIANT == 1
        if ((arg0->spawnArg1.value >> 16) != 0x10) {
            if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
                if (ABS(work->lookYawTarget) < 0x80) {
                    work->stateTimer = (u16)work->stateTimer + 1;
                }
            }
        } else {
            rec = work->hitContacts;
            if (ActorContact_PushContact(arg0->extra.tmd->coords, rec, ARRAY_SIZE(work->hitContacts)) != 1 && ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
                oddStrangerPushContacts(arg0, rec, ARRAY_SIZE(work->hitContacts));
            } else {
                if (ABS(work->lookYawTarget) < 0x80) {
                    work->stateTimer = (u16)work->stateTimer + 1;
                }
            }
        }
#else
        if ((arg0->spawnArg1.value >> 16) != 0x10) {
            if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1 &&
                ABS(work->lookYawTarget) < 0x80) {
                work->stateTimer++;
            } else {
                oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
        } else {
            if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1 ||
                 ActorContact_PushContact(arg0->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 1) &&
                ABS(work->lookYawTarget) < 0x80) {
                work->stateTimer++;
            } else {
                oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
        }
#endif
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (detectSightBlocked(arg0) != 1) {
            actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &turn->delta);
            if (!oddStrangerOutOfRange(&turn->delta, work->noticeRadius)) {
                work->state = ODD_STRANGER_STATE_ALERT;
            } else if (!oddStrangerOutOfRange(&turn->delta, 0xFA0)) {
                coord       = arg0->extra.tmd->coords;
                turn->angle = actorNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
                if (ABS(turn->angle) < 0x300) {
                    work->state = ODD_STRANGER_STATE_ALERT;
                }
            }
        }
        if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
            work->state = ODD_STRANGER_STATE_ALERT;
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    }
}
