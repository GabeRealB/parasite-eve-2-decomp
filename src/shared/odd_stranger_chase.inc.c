/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turn-aim state body, as in `Actor01900_Fn04D14`: take a 0x10
/// chase scratch off the scratch stack and, on the live-actor flag, key the
/// two animation nodes, the frame counter and the `field_C06` clip phase.
/// Once `field_8` has counted 7 frames the arm aims at the player - the yaw
/// toward `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` goes in `playerYaw`, the wrapped yaw toward
/// `gPlayerStatus.coordMtx` in `yaw` - and the root is turned by the facing
/// yaw plus a +-0x60 clamp of the turn's 1000 bias. The forward draw
/// `field_C04` is the doubled frame parameter (halved while `field_89A` is
/// up, forced to 2 while the frame counter runs), and the actor slides along
/// it when the `0x12C` probe reports the step is clear. `field_C06` walks 8 ->
/// -1 -> 0 as `field_8A2` passes 0x18 and 0x12, and the 0 arm runs the
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
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = ODD_STRANGER_SWING_RADIUS;
        work->field_898        = 1;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        work->field_C06           = 8;
        work->field_6             = 0;
        work->field_8             = 0;
        gOddStrangerChaseDistance = 0;
        work->field_C24++;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    chase                                   = head - 1;
    arg0->extra.tmd->coords->composeStamp   = GRAPHICS_COORD_DIRTY;
    oddStrangerDrive(arg0);
#if ODD_STRANGER_VARIANT == 1
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1) {
#else
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 0) {
#endif
        work->field_8++;
    } else {
        oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
    }
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
    if (work->field_8 >= 7) {
        chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                  (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
        chase->yaw       = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
        chase->yaw       = actorNormalizeYaw(chase->yaw);
        work->field_0    = 0x1A;
    }
    coord       = arg0->extra.tmd->coords;
    chase->turn = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    turn        = chase->turn;
    if (turn >= 0) {
        diffPos = turn - 1000;
        if (ABS(diffPos) < 0x60) {
            chase->angle = chase->turn - 1000;
        } else if (diffPos > 0) {
            chase->angle = 0x60;
        } else {
            chase->angle = -0x60;
        }
    } else {
        diffNeg = turn + 1000;
        if (ABS(diffNeg) < 0x60) {
            chase->angle = chase->turn + 1000;
        } else if (diffNeg > 0) {
            chase->angle = 0x60;
        } else {
            chase->angle = -0x60;
        }
    }
    facing        = arg0->extra.tmd->coords;
    chase->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, chase->angle, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    coord                                 = arg0->extra.tmd->coords;
    work->field_8AE                       = actorNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_C04                       = work->field_8A2 * 8;
    if (work->field_89A != 0) {
        work->field_C04 = work->field_C04 >> 1;
    }
    if (work->field_8 != 0) {
        work->field_C04 = 2;
    }
    if ((detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->field_C04) << 0x10) != 0) {
        actorMoveForwardNonzero(arg0->extra.tmd->coords, (u16)work->field_C04);
    }
    gOddStrangerChaseDistance += (u16)work->field_C04;
    if (work->field_C06 == 8 && work->field_8A2 >= 0x18) {
        work->field_C06 = -1;
    }
    if (work->field_C06 == -1 && work->field_8A2 == 0x12) {
        work->field_C06 = 0;
        work->field_6   = 0;
    }
    if (work->field_C06 == 0) {
        if (++work->field_6 == 5) {
            chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                                      (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &chase->delta);
            chase->yaw = ratan2(chase->delta.vx, chase->delta.vz) + 0x800;
            chase->yaw = actorNormalizeYaw(chase->yaw);
            yaw        = chase->yaw - chase->playerYaw;
            if (yaw < 0) {
                yaw = -yaw;
            }
            if (yaw > 0x400
#if ODD_STRANGER_SIGHT_TEST
                && detectSightBlocked(arg0) != 1
#endif
                && work->ODD_STRANGER_SIGHT_COOLDOWN == 0) {
                work->field_0 = 0xB;
            } else {
                work->field_0 = 0x1A;
                work->field_2 = -1;
            }
        }
    }
    work->field_8A2 += (u16)work->field_C06;
    if (work->ODD_STRANGER_SIGHT_COOLDOWN != 0) {
        work->ODD_STRANGER_SIGHT_COOLDOWN--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
