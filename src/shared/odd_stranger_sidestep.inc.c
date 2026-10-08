/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Hops to alternating sides of the player bearing, then resumes the chase.
///
/// Handles `ODD_STRANGER_STATE_SIDESTEP` on a live Odd Stranger task with
/// bound rigs and a live player in the same root-parent space. Stalking spawns
/// select `STALK` immediately. Other spawns choose the first side randomly,
/// then alternate; the first hop adds 369 angle units to the configured
/// sidestep angle (4096 per turn). A Q12 unit direction scales to a 222-unit
/// step, halved during blending and on grid pushback. Movement runs on ticks
/// 12..21; the 30th tick selects `CHASE`.
/// Borrows one chase scratch block plus nested contact workspace.
static void _oddStrangerSidestep(Task* task)
{
    enum {
        ODD_STRANGER_SIDESTEP_SPAWN_MODE_MASK  = 0xF0,
        ODD_STRANGER_SIDESTEP_STALK_SPAWN      = 0x10,
        ODD_STRANGER_SIDESTEP_FIRST_ANGLE_BIAS = 369,
        ODD_STRANGER_SIDESTEP_RATE             = 12, // Sixteenths of a frame per tick
        ODD_STRANGER_SIDESTEP_INITIAL_STEP     = 222,
        ODD_STRANGER_SIDESTEP_FIRST_MOVE_TICK  = 12,
        ODD_STRANGER_SIDESTEP_MOVE_TICKS       = 10,
        ODD_STRANGER_SIDESTEP_END_TICK         = 30
    };
    OddStrangerWork*   work;
    ActorChaseScratch* sidestep;
    ActorChaseScratch* savedCursor;
    TmdObject*         model;
    GfxCoord*          root;
    SVECTOR*           direction;
    MATRIX             directionRotation;
    u16                biasedBearing;
    s32                spawnMode;

    spawnMode = (task->spawnArg1.value >> 16);
    work      = task->work;
    if ((spawnMode & ODD_STRANGER_SIDESTEP_SPAWN_MODE_MASK) == ODD_STRANGER_SIDESTEP_STALK_SPAWN) {
        work->state = ODD_STRANGER_STATE_STALK;
        return;
    }
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1;
    sidestep                                = savedCursor - 1;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_SWING_RADIUS;
        work->stateTimer        = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &sidestep->delta);
        sidestep->turn = ratan2(sidestep->delta.vx, sidestep->delta.vz);
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = 1;
            } else {
                work->sidestepSide = -1;
            }
        }
        if (work->sidestepSide == 1) {
            work->animId = ODD_STRANGER_ANIM_SIDESTEP_POSITIVE;
            if (work->sidestepCount == 0) {
                biasedBearing  = sidestep->turn + ODD_STRANGER_SIDESTEP_FIRST_ANGLE_BIAS;
                sidestep->turn = work->sidestepAngle + biasedBearing;
            } else {
                sidestep->turn += work->sidestepAngle;
            }
            work->sidestepSide = -1;
        } else {
            work->animId = ODD_STRANGER_ANIM_SIDESTEP_NEGATIVE;
            if (work->sidestepCount == 0) {
                biasedBearing  = sidestep->turn - ODD_STRANGER_SIDESTEP_FIRST_ANGLE_BIAS;
                sidestep->turn = biasedBearing - work->sidestepAngle;
            } else {
                sidestep->turn -= work->sidestepAngle;
            }
            work->sidestepSide = 1;
        }
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate    = ODD_STRANGER_SIDESTEP_RATE;
        work->blendActive = 0;
        _oddStrangerDriveAnimation(task);
        gfxRotMatrixY(&directionRotation, sidestep->turn, GRAPHICS_ROTATION_REPLACE);
        direction = &work->sidestepDir;
        gfxReadMatrixZAxis(&directionRotation, direction);
        VectorNormalSS(direction, direction);
        work->sidestepStep = ODD_STRANGER_SIDESTEP_INITIAL_STEP;
        work->sidestepCount++;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->blendActive == 0) {
        gte_lddp(work->sidestepStep);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestep->delta);
    } else {
        gte_lddp(work->sidestepStep >> 1);
        gte_ldsv(&work->sidestepDir);
        gte_gpf12();
        gte_stsv(&sidestep->delta);
    }
    // Translate only during the hop window; grid pushback damps later steps.
    if ((u32)((u16)work->stateTimer - ODD_STRANGER_SIDESTEP_FIRST_MOVE_TICK) < (u32)ODD_STRANGER_SIDESTEP_MOVE_TICKS) {
        root              = task->extra.tmd->coords;
        root->coord.t[0] += sidestep->delta.vx;
        root              = task->extra.tmd->coords;
        root->coord.t[2] += sidestep->delta.vz;
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
            work->sidestepStep = (u16)(work->sidestepStep >> 1);
        }
    }
    if (++work->stateTimer >= ODD_STRANGER_SIDESTEP_END_TICK) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
