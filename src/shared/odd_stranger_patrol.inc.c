/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Walk the actor along its `field_C` waypoint pair: `field_14` picks the
/// waypoint the offset is taken from and flips once the actor closes inside
/// 0xA0 of it, or after 0x15 frames in `field_6`, and the wrapped yaw toward
/// that waypoint is clamped to +-0x20, added back to the facing yaw and the
/// root rotation rescaled by 0x1194. The `detectPlayerOutOfReach` probe
/// takes one 0xA step forward, the obstacle walk runs against `field_A30`
/// (plus `field_8F0` through `oddStrangerPushContacts` when the spawn
/// sub-type is 0x10), and each arm counts `field_6` up while the yaw stays
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
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = ODD_STRANGER_BODY_RADIUS;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        work->field_6 = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    } else {
        SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
        turn           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
        turn->delta.vx = work->field_C[work->field_14].x - arg0->extra.tmd->coords->coord.t[0];
        turn->delta.vy = 0;
        turn->delta.vz = work->field_C[work->field_14].z - arg0->extra.tmd->coords->coord.t[2];
#if ODD_STRANGER_VARIANT == 2
        turn->delta.vx = work->field_C[work->field_14].x - arg0->extra.tmd->coords->coord.t[0];
        turn->delta.vy = 0;
        turn->delta.vz = work->field_C[work->field_14].z - arg0->extra.tmd->coords->coord.t[2];
#endif
        if (!oddStrangerOutOfRange(&turn->delta, 0xA0) || work->field_6 >= 0x15) {
            if (work->field_14 == 0) {
                work->field_14 = 1;
            } else {
                work->field_14 = 0;
            }
            work->field_6 = 0;
        }
        oddStrangerDrive(arg0);
        coord           = arg0->extra.tmd->coords;
        turn->angle     = actorNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = turn->angle;
        if (turn->angle > ODD_STRANGER_PATROL_TURN_CLAMP) {
            turn->angle = ODD_STRANGER_PATROL_TURN_CLAMP;
        }
        if (turn->angle < -ODD_STRANGER_PATROL_TURN_CLAMP) {
            turn->angle = -ODD_STRANGER_PATROL_TURN_CLAMP;
        }
        turn->angle += ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
        actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
        if (work->field_89A == 0 && (detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ODD_STRANGER_WALK_STEP) << 16) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, ODD_STRANGER_WALK_STEP);
        }
#if ODD_STRANGER_VARIANT == 1
        if ((arg0->spawnArg1.value >> 16) != 0x10) {
            if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1) {
                if (ABS(work->field_8AE) < 0x80) {
                    work->field_6 = (u16)work->field_6 + 1;
                }
            }
        } else {
            rec = work->field_8F0;
            if (ActorContact_PushContact(arg0->extra.tmd->coords, rec, 0xC) != 1 && ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
                oddStrangerPushContacts(arg0, rec, 0xC);
            } else {
                if (ABS(work->field_8AE) < 0x80) {
                    work->field_6 = (u16)work->field_6 + 1;
                }
            }
        }
#else
        if ((arg0->spawnArg1.value >> 16) != 0x10) {
            if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1 &&
                ABS(work->field_8AE) < 0x80) {
                work->field_6++;
            } else {
                oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
            }
        } else {
            if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1 ||
                 ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC) == 1) &&
                ABS(work->field_8AE) < 0x80) {
                work->field_6++;
            } else {
                oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
            }
        }
#endif
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (detectSightBlocked(arg0) != 1) {
            actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &turn->delta);
            if (!oddStrangerOutOfRange(&turn->delta, work->field_C16)) {
                work->field_0 = 6;
            } else if (!oddStrangerOutOfRange(&turn->delta, 0xFA0)) {
                coord       = arg0->extra.tmd->coords;
                turn->angle = actorNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
                if (ABS(turn->angle) < 0x300) {
                    work->field_0 = 6;
                }
            }
        }
        if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
            work->field_0 = 6;
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    }
}
