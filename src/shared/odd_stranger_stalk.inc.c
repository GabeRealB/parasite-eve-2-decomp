/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State-2 aim body, as in the Horned Stranger's `func_actor_401300_8013CBAC` and
/// `Actor01900_Fn042BC`: on the live-actor flag it resets the effect node, forks
/// the first clip and seeds the animation slots, then walks both obstacle tables
/// and aims the actor at the player with `gfxRotMatrixY` / `actorRescaleYaw`.
/// `field_6` and `field_8` then count up under the `detectSightBlocked`
/// clip test: the still-aiming arm re-wraps the turn, drops the actor to state
/// 0xB once the 0x44C range check fails inside 0x200 and re-arms at 0x1B past
/// 0x5B frames, while the settled arm draws a turn direction from `gRandomLcgState`
/// and flips it every 0xF1 frames. The tail takes one forward step off the
/// 0x12C probe, or re-arms the clip on the `rig.slots[1].status` bit. `field_C1B` counts
/// down once per entry.
void oddStrangerStalk(Task* arg0)
{
    OddStrangerWork*   work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;
    ActorChaseScratch* head;
    ActorChaseScratch* s;
#if ODD_STRANGER_VARIANT == 1
    s16 yaw;
#endif

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = ODD_STRANGER_BODY_RADIUS;
        work->field_898        = 1;
        work->field_8A2        = ODD_STRANGER_STALK_RATE;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_C24        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
        if ((arg0->spawnArg1.value >> 16) == 0x10) {
            work->field_8D0.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    work->field_6++;
    work->field_8++;
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    s                                       = head - 1;
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_8F0, 0xC) != 1) {
            oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
        }
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
    s->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                          (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &s->delta);
#if ODD_STRANGER_VARIANT == 1
    yaw             = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw          = yaw;
    s->yaw          = actorNormalizeYaw(yaw);
    coord           = arg0->extra.tmd->coords;
    s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8AE = s->turn;
#else
    s->yaw = ratan2(s->delta.vx, s->delta.vz) + 0x800;
    s->yaw = actorNormalizeYaw(s->yaw);
#endif
    if (detectSightBlocked(arg0) != 1) {
        work->field_6   = 0;
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (s->turn < 0x200) {
            if (!oddStrangerOutOfRange(&s->delta, 0x44C) && work->ODD_STRANGER_SIGHT_COOLDOWN == 0) {
                work->field_0 = 0xB;
            }
        }
        if (work->field_8 >= 0x5B) {
            work->field_0 = 0x1B;
        }
    } else {
        work->field_8 = 0;
#if ODD_STRANGER_VARIANT == 2
        work->field_6++;
#endif
        coord           = arg0->extra.tmd->coords;
        s->turn         = actorNormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8AE = s->turn;
        if (work->field_C08 == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_C08 = -1;
            } else {
                work->field_C08 = 1;
            }
        }
        if (work->field_C08 == 1) {
            s->turn += ODD_STRANGER_STALK_TURN_BIAS;
        } else {
            s->turn -= ODD_STRANGER_STALK_TURN_BIAS;
        }
        if (work->field_6 >= 0xF1) {
            work->field_6   = 0;
            work->field_C08 = -work->field_C08;
        }
    }
    if (s->turn > 0x20) {
        s->turn = 0x20;
    }
    if (s->turn < -0x20) {
        s->turn = -0x20;
    }
    facing   = arg0->extra.tmd->coords;
    s->turn += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->turn, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 2) {
        if (work->field_89A == 0) {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ODD_STRANGER_STALK_STEP) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, ODD_STRANGER_STALK_STEP);
            }
        } else {
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 5) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, 5);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_89E = 2;
        work->field_898 = 1;
    }
    if (work->ODD_STRANGER_SIGHT_COOLDOWN != 0) {
        work->ODD_STRANGER_SIGHT_COOLDOWN--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
