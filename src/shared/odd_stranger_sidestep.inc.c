/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Turn-entry body, as in the Horned Stranger's `func_actor_401300_80137D78`: carve the
/// aim scratch off the scratch stack, and while the live-actor flag is up reset
/// the display nodes and rebuild the actor's facing. The turn direction
/// (`sidestepSide`) is drawn from `gRandomLcgState` on the first entry, and each entry
/// swings the facing toward the player by `sidestepAngle` plus a 0x171 bias until
/// `sidestepCount` has been counted once. The forward direction `sidestepDir` comes
/// out of the turn angle through `gfxRotMatrixY`, and the `sidestepStep` draw
/// scales it onto the scratch vector; the actor is then slid along its obstacle
/// table, halving that draw while it overlaps. Counts the entry in `stateTimer`
/// and keys state 7 once 0x1E of them have run.
void oddStrangerSidestep(Task* arg0)
{
    OddStrangerWork*   work;
    ActorChaseScratch* aim;
    ActorChaseScratch* head;
    TmdObject*         obj;
    GfxCoord*          coord;
    SVECTOR*           dir;
    MATRIX             mat;
    u16                angle;
    s32                kind;

    kind = (arg0->spawnArg1.value >> 16);
    work = arg0->work;
    if ((kind & 0xF0) == 0x10) {
        work->state = ODD_STRANGER_STATE_STALK;
        return;
    }
    head                                    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1;
    aim                                     = head - 1;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->hitBody.radius    = ODD_STRANGER_SWING_RADIUS;
        work->stateTimer        = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actorPositionDeltaToPlayer(&gPlayerStatus, arg0->extra.tmd->coords, &aim->delta);
        aim->turn = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = 1;
            } else {
                work->sidestepSide = -1;
            }
        }
        if (work->sidestepSide == 1) {
            work->animId = 0x15;
            if (work->sidestepCount == 0) {
                angle     = aim->turn + 0x171;
                aim->turn = work->sidestepAngle + angle;
            } else {
                aim->turn += work->sidestepAngle;
            }
            work->sidestepSide = -1;
        } else {
            work->animId = 0x14;
            if (work->sidestepCount == 0) {
                angle     = aim->turn - 0x171;
                aim->turn = angle - work->sidestepAngle;
            } else {
                aim->turn -= work->sidestepAngle;
            }
            work->sidestepSide = 1;
        }
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate    = 0xC;
        work->blendActive = 0;
        _oddStrangerDriveAnimation(arg0);
        gfxRotMatrixY(&mat, aim->turn, 1);
        dir = &work->sidestepDir;
        gfxReadMatrixZAxis(&mat, dir);
        VectorNormalSS(dir, dir);
        work->sidestepStep = 0xDE;
        work->sidestepCount++;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        gte_lddp(work->sidestepStep);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&aim->delta);
    } else {
        gte_lddp(work->sidestepStep >> 1);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&aim->delta);
    }
    if ((u32)((u16)work->stateTimer - 0xC) < 0xAU) {
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->extra.tmd->coords;
        coord->coord.t[2] += aim->delta.vz;
        if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
            work->sidestepStep = (u16)(work->sidestepStep >> 1);
        }
    }
    if (++work->stateTimer >= 0x1E) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
