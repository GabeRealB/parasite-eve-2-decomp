/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turn-entry body, as in the Horned Stranger's `func_actor_401300_801376E4`: carve the
/// chase scratch off the scratch stack, and while the live-actor flag is up
/// reset the display nodes and rebuild the actor's facing. The turn direction
/// comes off the wrapped yaw toward the player, the yaw itself out of the
/// root's own rotation, and the pair (`field_C00` / `field_C02`) is what the
/// per-frame arm then walks: the turn swings the facing 0x89 a frame until it
/// reaches the seeded yaw, `gfxRotMatrixY` rebuilds the rotation from it and
/// `actorRescaleYaw` re-scales the root by 0x1194. When the two have
/// met the actor re-arms (`field_0` 8 or 0xB) off `field_C24`, the obstacle
/// range and the `field_C1B` cooldown, and the arm is then slid forward along
/// its obstacle table. `field_C1B` counts down once per entry.
void oddStrangerTurnAround(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    ActorChaseScratch* head;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;

    work = arg0->work;
    if (work->field_4 != 0) {
        head                                                      = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        obj                                                       = arg0->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                   = head - 1;
        aim                                                       = head - 1;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.radius = ODD_STRANGER_BODY_RADIUS;
        work->field_898        = 1;
        work->field_8A2        = 0x10;
        work->field_89E        = 3;
        work->field_89A        = 0;
        work->field_8AE        = 0;
        work->field_B50.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        coord           = arg0->extra.tmd->coords;
        aim->turn       = actorNormalizeYaw(ratan2(head[-1].delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing          = arg0->extra.tmd->coords;
        aim->angle      = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C00 = aim->angle;
        work->field_C02 = aim->angle + (u16)aim->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    oddStrangerDrive(arg0);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    if (work->field_C00 == work->field_C02) {
        if (work->field_C24 < 2 || oddStrangerOutOfRange(&aim->delta, 0x384)
#if ODD_STRANGER_SIGHT_TEST
            || detectSightBlocked(arg0) == 1
#endif
            || work->ODD_STRANGER_SIGHT_COOLDOWN != 0) {
            work->field_0 = 8;
        } else {
            work->field_0 = 0xB;
        }
    }
    if (work->field_C00 > work->field_C02) {
        work->field_C00 -= 0x89;
        if (work->field_C00 < work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    if (work->field_C00 < work->field_C02) {
        work->field_C00 += 0x89;
        if (work->field_C00 > work->field_C02) {
            work->field_C00 = work->field_C02;
        }
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, work->field_C00, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->field_89A == 0) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 0x28) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x28);
        }
    } else {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 0x14) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x14);
        }
    }
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) != 1) {
        oddStrangerPushContacts(arg0, work->field_8F0, 0xC);
    }
    if (work->ODD_STRANGER_SIGHT_COOLDOWN != 0) {
        work->ODD_STRANGER_SIGHT_COOLDOWN--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
