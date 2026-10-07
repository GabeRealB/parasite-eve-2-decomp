/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Runs the armed chaser's left-turn animation and forward shuffle.
///
/// Requires a live model, `DesertChaserWork` and spawn-argument `Enemy`, and
/// the player root in the model root's parent frame. Angles use 4096 units
/// per turn; translation uses that frame's coordinate units. Each tick spreads
/// the remaining turn over 30 minus the elapsed ticks (zero becomes one).
/// A player more than a quarter-turn to the right is faced away from instead.
/// The shuffle moves 26 units forward unconditionally, then requests an
/// 8-unit backward step subject to actor freeze and applies root-grid pushback.
/// Alignment within 0x20 angle units or a settled clip resumes pursuit.
/// Reserves and releases one 16-byte scratch block around the nested animation,
/// rotation and movement helpers; no pointer to it survives the call.
static void _desertChaserTurnLeftState(Task* task)
{
    TmdObject*                   model;
    Enemy*                       enemy;
    DesertChaserWork*            work;
    DesertChaserTurnStepScratch* scratch;
    s16                          nextHeading;
    s16                          ticksLeft;
    s32                          playerTurn;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserTurnStepScratch);
    work    = task->work;
    enemy   = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        model->flags                                          = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                         = 0;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = DESERT_CHASER_CLIP_TURN_PROBE;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->stateTimer                                      = 0;
    }
    work->stateTimer += 1;
    _desertChaserAnimTick(task);

    // Turn toward the player, or toward the opposite bearing after crossing behind.
    playerTurn          = _actorAngleTurnToPlayer(task, &scratch->offset, &gPlayerStatus);
    scratch->turn       = playerTurn;
    work->lookYawTarget = playerTurn;
    if (scratch->turn > 0) {
        if (abs(scratch->turn) >= ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2 + 1) {
            work->lookYawTarget = playerTurn - ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            scratch->turn      -= ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        }
    }
    ticksLeft          = DESERT_CHASER_TURN_TICKS - work->stateTimer;
    scratch->stepsLeft = ticksLeft;
    if (ticksLeft == 0) {
        scratch->stepsLeft = 1;
    }
    nextHeading      = scratch->turn / scratch->stepsLeft + ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    scratch->heading = nextHeading;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, nextHeading, GRAPHICS_ROTATION_REPLACE);

    // The clip's shuffle is unconditional; the additional movement respects freeze.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &scratch->offset);
    _actorMovementBuildDisplacement(&scratch->offset, 26);
    task->extra.tmd->coords->coord.t[0] += scratch->offset.vx;
    task->extra.tmd->coords->coord.t[2] += scratch->offset.vz;
    _actorMovementStepForward(task->extra.tmd->coords, -8);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
#if DESERT_CHASER_RUN_SEQUENCE
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#endif
    if (abs(scratch->turn) < DESERT_CHASER_TURN_ALIGNED_YAW) {
        work->state = DESERT_CHASER_STATE_PURSUE;
    }
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = DESERT_CHASER_STATE_PURSUE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserTurnStepScratch);
}
