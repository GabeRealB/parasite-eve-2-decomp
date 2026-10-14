/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Plays the far-catch throw and prepares the held player's forward displacement.
///
/// Requires the armed chaser's live enemy/model and player task. Entry plays
/// clip 5, limits the recorded lunge distance to 4000 world units and stores a
/// 133-unit step along the facing for the frame driver to send to the player.
/// Tick 15 emits the throw cue if player collision is still requested. Once
/// the clip settles, the chaser leaps back, or flees on Water Tower command 1.
/// Sixteen scratch bytes are reserved per call; only the first vector is used.
static void _desertChaserThrowPlayer(Task* task)
{
    // Throw ticks, world-unit distances and the dust effect's packed options.
    enum {
        DESERT_CHASER_CLIP_THROW               = 5,
        DESERT_CHASER_THROW_MAX_LUNGE_DISTANCE = 4000,
        DESERT_CHASER_THROW_PLAYER_STEP        = 133,
        DESERT_CHASER_THROW_CUE_TICK           = 15,
        DESERT_CHASER_SOUND_THROW              = 0x4001000A,
        DESERT_CHASER_THROW_DUST_ARGUMENT      = DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0xA00
    };

    Enemy*            enemy;
    DesertChaserWork* work;
    SVECTOR*          throwStep;
    TmdObject*        model;
    s32               x;
    s32               z;
    s32               throwSound;
    s32               entryPan;
    s32               eventPan;
    s32               nextState;
    u16               elapsedFrames;
    Task*             player;

    work      = task->work;
    player    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    throwStep = SCRATCH_STACK_RESERVE_BYTES(2 * sizeof(*throwStep));
    enemy     = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        work->hitFlag = 0;
#endif
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animId                                          = DESERT_CHASER_CLIP_THROW;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(task);
        // The frame driver sends this facing-aligned push to the held player.
        gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, throwStep);
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        work->playerAnimFrames = 0;
#endif
        work->stateTimer = 0;
        VectorNormalSS(throwStep, throwStep);
        if (work->lungeDistance > DESERT_CHASER_THROW_MAX_LUNGE_DISTANCE) {
            work->lungeDistance = DESERT_CHASER_THROW_MAX_LUNGE_DISTANCE;
        }
        gte_lddp(DESERT_CHASER_THROW_PLAYER_STEP);
        gte_ldsv(throwStep);
        gte_gpf12();
        gte_stsv(throwStep);
        x                                  = throwStep->vx;
        work->playerMove.displacement.vy   = 0;
        work->playerMove.displacement.vx   = x;
        z                                  = throwStep->vz;
        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        work->playerMove.keepControl       = 1;
        work->playerMove.displacement.vz   = z;
        entryPan                           = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(SOUND_COMMON(7), (s32)entryPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        padScriptSpawnVariableMotorRamp(8, 0xFF, 8);
#endif
    }
    elapsedFrames    = work->stateTimer + 1;
    work->stateTimer = elapsedFrames;
    if (((s16)elapsedFrames == DESERT_CHASER_THROW_CUE_TICK) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
        throwSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | DESERT_CHASER_SOUND_THROW;
        eventPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(throwSound, (s32)eventPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
            effectSpawn(EFFECT_DUST_PUFF, player->extra.tmd->coords + 1, DESERT_CHASER_THROW_DUST_ARGUMENT, NULL);
        }
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        work->state = DESERT_CHASER_STATE_LEAP_BACK;
#else
        nextState = work->lastCommand.word & DESERT_CHASER_COMMAND_MASK;
        if (nextState == DESERT_CHASER_COMMAND_WATER_TOWER_START_RUN) {
            nextState = DESERT_CHASER_STATE_FLEE;
        } else {
            nextState = DESERT_CHASER_STATE_LEAP_BACK;
        }
        work->state = nextState;
#endif
    }
    SCRATCH_STACK_RELEASE_BYTES(2 * sizeof(*throwStep));
}

/// Temporary player offset and turn storage for one close-catch tick.
///
/// The 16-byte scratch-stack reservation is uninitialized and released before
/// return. XYZ starts as player minus actor translation in their common parent
/// frame, narrowed to signed halfwords. Entry normalizes it at Q12, then scales
/// it to a 32-unit move request; later range and turn queries refill the offset.
/// The turn store is not consumed, and the remaining reserved bytes have no
/// established meaning or scalar type.
typedef struct {
    SVECTOR toPlayer;     // Player offset or scaled move direction; pad is untouched
    u8      unknown_8[4]; // Reserved bytes with no recovered accesses; role unproven
    s16     playerTurn;   // Signed turn toward the player, -2048..2048 (4096 per turn); stored only
    u8      unknown_E[2]; // Reserved bytes with no recovered accesses; role unproven
} _DesertChaserCloseCatchScratch;
STATIC_ASSERT_SIZEOF(_DesertChaserCloseCatchScratch, 16);

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
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
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
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
        if ((work->lastCommand.word & DESERT_CHASER_COMMAND_MASK) == DESERT_CHASER_COMMAND_WATER_TOWER_START_RUN) {
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
