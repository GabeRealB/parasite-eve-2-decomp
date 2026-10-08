/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Drives the close-catch clip and its lunge/recovery transitions.
///
/// Requires armed work, live player/model and enemy. Entry prepares a 32-unit
/// horizontal player move request and selects clip 5; it does not dispatch that
/// move itself. A settled clip picks roam beyond 2000 units, otherwise leap-back.
/// A clip-3 tick advances the lunge and selects wall recoil on grid push, then
/// returns to clip 5 after 21 ticks. Water Tower command 1 selects flee on entry.
/// The 16-byte scratch reservation is released each tick; only its player-turn
/// store uses the second vector-sized half, and that value is not read here.
static void _desertChaserCloseCatchState(Task* task)
{
    enum {
        DESERT_CHASER_CLOSE_CATCH_MOVE_STEP      = 32,
        DESERT_CHASER_CLOSE_CATCH_BACKOFF_RADIUS = 2000,
        DESERT_CHASER_CLOSE_CATCH_LUNGE_TICKS    = 21,
    };
    DesertChaserWork*               work;
    Enemy*                          enemy;
    TmdObject*                      model;
    _DesertChaserCloseCatchScratch* scratch;
    s32                             moveX, moveZ;
    s16                             playerTurn;
    s32                             playerOutsideRange;
    s32                             animationId;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_DesertChaserCloseCatchScratch);
    work    = task->work;
    enemy   = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->animId                                          = DESERT_CHASER_CLIP_CLOSE_CATCH;
        work->waistYawTarget                                  = 0;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->playerAnimFrames = 0;
#endif
        work->stateTimer                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                       = work->baseRate;
        _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &scratch->toPlayer);
        VectorNormalSS(&scratch->toPlayer, &scratch->toPlayer);
        gte_lddp(DESERT_CHASER_CLOSE_CATCH_MOVE_STEP);
        gte_ldsv(&scratch->toPlayer);
        gte_gpf12();
        gte_stsv(&scratch->toPlayer);
        moveX                              = scratch->toPlayer.vx;
        work->playerMove.displacement.vy   = 0;
        work->playerMove.displacement.vx   = moveX;
        moveZ                              = scratch->toPlayer.vz;
        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        work->playerMove.keepControl       = 1;
        work->wallProbe.shape.ends[1].vz   = DESERT_CHASER_LUNGE_PROBE_REACH;
        work->playerMove.displacement.vz   = moveZ;
#if DESERT_CHASER_RUN_SEQUENCE
        if ((work->lastCommand.word & DESERT_CHASER_COMMAND_MASK) == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            work->state = DESERT_CHASER_STATE_FLEE;
        }
#else
        padScriptSpawnVariableMotorRamp(3, 0xFF, 8);
#endif
    }
    work->stateTimer += 1;
    _desertChaserAnimTick(task);
    animationId = work->animId;
    switch (animationId) {
        case DESERT_CHASER_CLIP_CLOSE_CATCH:
            if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
                _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &scratch->toPlayer);
                playerOutsideRange = actorOutsideRadius(&scratch->toPlayer, DESERT_CHASER_CLOSE_CATCH_BACKOFF_RADIUS);
                if (playerOutsideRange) {
                    work->state = DESERT_CHASER_STATE_ROAM;
                } else {
                    work->state = DESERT_CHASER_STATE_LEAP_BACK;
                }
            }
            break;
        case DESERT_CHASER_CLIP_LUNGE:
            playerTurn          = _actorAngleTurnToPlayer(task, &scratch->toPlayer, &gPlayerStatus);
            scratch->playerTurn = playerTurn;
            if (_desertChaserCapsuleTouchesGrid(task)) {
                _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_LUNGE_GRID_STEP);
            } else {
                _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_LUNGE_STEP);
            }
            if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts))) {
                work->state = DESERT_CHASER_STATE_WALL_KNOCKDOWN;
            }
            if (work->stateTimer >= DESERT_CHASER_CLOSE_CATCH_LUNGE_TICKS) {
                work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
                work->blendActive = 0;
                work->animId      = DESERT_CHASER_CLIP_CLOSE_CATCH;
                work->animRate    = work->baseRate;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_DesertChaserCloseCatchScratch);
}
