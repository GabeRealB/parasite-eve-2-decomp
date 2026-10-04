/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turn-entry body, as in the Horned Stranger's `func_actor_401300_801376E4`: carve the
/// chase scratch off the scratch stack, and while the live-actor flag is up
/// reset the display nodes and rebuild the actor's facing. The turn direction
/// comes off the wrapped yaw toward the player, the yaw itself out of the
/// root's own rotation, and the pair (`turnYaw` / `turnYawTarget`) is what the
/// per-frame arm then walks: the turn swings the facing 0x89 a frame until it
/// reaches the seeded yaw, `gfxRotMatrixY` rebuilds the rotation from it and
/// `actorRescaleYaw` re-scales the root by 0x1194. When the two have
/// met the actor re-arms (`state` 8 or 0xB) off `dashCount`, the obstacle
/// range and the `grabCooldown` cooldown, and the arm is then slid forward along
/// its obstacle table. `grabCooldown` counts down once per entry.
void oddStrangerTurnAround(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    ActorChaseScratch* head;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          facing;

    work = arg0->work;
    if (work->stateEntered != 0) {
        head                                                      = SCRATCH_STACK_CURSOR(ActorChaseScratch);
        obj                                                       = arg0->extra.tmd;
        SCRATCH_STACK_CURSOR(ActorChaseScratch)                   = head - 1;
        aim                                                       = head - 1;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = 0x10;
        work->animId            = 3;
        work->blendActive       = 0;
        work->lookYawTarget     = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        oddStrangerDrive(arg0);
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        coord               = arg0->extra.tmd->coords;
        aim->turn           = actorNormalizeYaw(ratan2(head[-1].delta.vx, aim->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing              = arg0->extra.tmd->coords;
        aim->angle          = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->turnYaw       = aim->angle;
        work->turnYawTarget = aim->angle + (u16)aim->turn * 2;
        SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    oddStrangerDrive(arg0);
    actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
    if (work->turnYaw == work->turnYawTarget) {
        if (work->dashCount < 2 || oddStrangerOutOfRange(&aim->delta, 0x384)
#if ODD_STRANGER_SIGHT_TEST
            || detectSightBlocked(arg0) == 1
#endif
            || work->grabCooldown != 0) {
            work->state = ODD_STRANGER_STATE_CIRCLE_DASH;
        } else {
            work->state = ODD_STRANGER_STATE_GRAB;
        }
    }
    if (work->turnYaw > work->turnYawTarget) {
        work->turnYaw -= 0x89;
        if (work->turnYaw < work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    if (work->turnYaw < work->turnYawTarget) {
        work->turnYaw += 0x89;
        if (work->turnYaw > work->turnYawTarget) {
            work->turnYaw = work->turnYawTarget;
        }
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, work->turnYaw, 1);
    actorRescaleYaw(arg0->extra.tmd->coords, 0x1194);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 0x28) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x28);
        }
    } else {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, 0x14) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x14);
        }
    }
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        oddStrangerPushContacts(arg0, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
