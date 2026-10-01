/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Aim step toward the player, as in the Horned Stranger's `func_actor_401300_8013A5C0`. Unlike
/// `oddStrangerFacePlayer` the wrapped turn is halved and clamped to
/// +-0x80 instead of +-0x10, so the actor turns at half speed and only the
/// 0x16/2 -> 0x16/0x11 spawn pair keys the follow-up; the spawn arm writes
/// `field_8D0.field_1C` first and leaves `field_6` alone, and the exit turn
/// reads the sign of `field_8AE` with the `0x4B0` arm first.
void oddStrangerBackOff(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    TmdObject*         obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = ODD_STRANGER_BODY_RADIUS;
        work->field_898        = 1;
        work->field_8A2        = 0x16;
        work->field_89E        = 2;
        work->field_89A        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        return;
    }
    oddStrangerDrive(arg0);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    aim             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    aim->turn       = actorPositionYaw(arg0, &aim->delta, &gPlayerStatus);
    work->field_8AE = aim->turn;
    if (ABS(aim->turn) < 0x81 && work->field_89E == 2) {
        work->field_8A2 = 0x16;
        work->field_89E = 0x11;
        work->field_898 = 1;
        work->field_6   = 0;
        oddStrangerDrive(arg0);
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
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89E == 0x11) {
        work->field_6++;
        if ((detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, -0x10) << 16) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, -0x10);
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
            oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if ((s16)work->field_6 >= 0x13) {
            if (work->field_8AE <= 0) {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x4B0, 0);
            } else {
                gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
