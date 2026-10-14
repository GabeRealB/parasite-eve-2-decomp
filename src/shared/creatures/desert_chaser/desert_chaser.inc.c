/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Patrols two waypoints and switches to alert roaming or pursuit on detection.
///
/// Requires the armed build's live work, enemy, model and contact arrays. The
/// waypoint index is 0 or 1; positions share the root's parent frame. Entry resets
/// draw, animation and collision state. Subsequent ticks turn at most 16/4096 of
/// a revolution and walk 20 coordinate units, alternating waypoints on arrival
/// or after 21 nearly straight grid pushes. The regular build gates detection by
/// line of sight and placement frame, and noise selects roam; Water Tower detects
/// every tick and selects pursuit. Temporary scratch storage is released each tick.
static void _desertChaserPatrol(Task* task)
{
    enum {
        DESERT_CHASER_PATROL_TURN_LIMIT         = 0x10,
        DESERT_CHASER_PATROL_STUCK_YAW_LIMIT    = 0x80,
        DESERT_CHASER_PATROL_NEAR_PLAYER_RADIUS = 2000,
        DESERT_CHASER_PATROL_FAR_PLAYER_RADIUS  = 4000,
        DESERT_CHASER_PATROL_DETECTION_YAW      = 0x300,
    };
    DesertChaserWork*      work;
    Enemy*                 enemy;
    ActorTurnScratch*      scratch;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              detectionCoord;
    GfxCoord*              turnCoord;
    WorldCollisionContact* frontContacts;
    s32                    signedTurn;
    s32                    lookTurnMagnitude;
    s16                    nextYaw;

    work = task->work;
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
    enemy = task->spawnArg2.pointer; /* also read by the look-around below */
#endif
    if (work->stateEntered != 0) {
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
        enemy = task->spawnArg2.pointer;
#endif
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        work->blendActive                                    = 0;
        work->animId                                         = DESERT_CHASER_CLIP_PATROL;
        work->waistYawTarget                                 = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        _desertChaserAnimTick(task);
        _desertChaserAnimTick(task);
        work->stateTimer                 = 0;
        work->wallProbe.shape.ends[1].vz = DESERT_CHASER_PATROL_PROBE_REACH;
        return;
    }
    // Steer toward the current waypoint, preserving look and waist turns.
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    scratch->delta.vx = work->patrolPoints[work->patrolTarget].x - task->extra.tmd->coords->coord.t[0];
    scratch->delta.vy = 0;
    scratch->delta.vz = work->patrolPoints[work->patrolTarget].z - task->extra.tmd->coords->coord.t[2];
    if (!actorOutsideRadius(&scratch->delta, DESERT_CHASER_WAYPOINT_RADIUS) || work->stateTimer >= DESERT_CHASER_STUCK_TICKS) {
        if (work->patrolTarget == 0)
            work->patrolTarget = 1;
        else
            work->patrolTarget = 0;
        work->stateTimer = 0;
    }
    _desertChaserAnimTick(task);
    rootCoord           = task->extra.tmd->coords;
    signedTurn          = _actorAngleTurnToOffset(rootCoord, scratch->delta.vx, scratch->delta.vz);
    scratch->angle      = signedTurn;
    work->lookYawTarget = signedTurn;
    if (scratch->angle >= DESERT_CHASER_PATROL_TURN_LIMIT + 1)
        scratch->angle = DESERT_CHASER_PATROL_TURN_LIMIT;
    if (scratch->angle < -DESERT_CHASER_PATROL_TURN_LIMIT)
        scratch->angle = -DESERT_CHASER_PATROL_TURN_LIMIT;
    work->waistYawTarget = scratch->angle;
    turnCoord            = task->extra.tmd->coords;
    nextYaw              = (u16)scratch->angle + ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]);
    scratch->angle       = nextYaw;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, nextYaw, 1);
    frontContacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    if (work->blendActive == 0) {
        if (_desertChaserCapsuleTouchesGrid(task)) {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_PATROL_STEP);
        } else {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_PATROL_STEP);
        }
        frontContacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    }
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, frontContacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->delta);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) == 1) {
        lookTurnMagnitude = abs(work->lookYawTarget);
        if (lookTurnMagnitude < DESERT_CHASER_PATROL_STUCK_YAW_LIMIT)
            work->stateTimer += 1;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
    if (_playerDetectionSightBlocked(task) != 1)
#endif
    {
        detectionCoord    = task->extra.tmd->coords;
        scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - detectionCoord->coord.t[0];
        scratch->delta.vy = gPlayerStatus.coordMtx->t[1] - detectionCoord->coord.t[1];
        scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - detectionCoord->coord.t[2];
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        /* each chaser looks on its own frame of fifteen */
        if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == gDisplayState.animFrame % DESERT_CHASER_SIGHT_FRAME_PERIOD)
#endif
        {
            if (!_actorRangeOutsideRadiusXZ(&scratch->delta, DESERT_CHASER_PATROL_NEAR_PLAYER_RADIUS)) {
                DESERT_CHASER_NOTICE(work);
            } else if (!_actorRangeOutsideRadiusXZ(&scratch->delta, DESERT_CHASER_PATROL_FAR_PLAYER_RADIUS)) {
                rootCoord      = task->extra.tmd->coords;
                signedTurn     = _actorAngleTurnToOffset(rootCoord, scratch->delta.vx, scratch->delta.vz);
                scratch->angle = signedTurn;
                signedTurn     = abs(signedTurn);
                if (signedTurn < DESERT_CHASER_PATROL_DETECTION_YAW) {
                    DESERT_CHASER_NOTICE(work);
                }
            }
        }
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE)
            work->state = DESERT_CHASER_STATE_ROAM;
#endif
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Prepares the held player's animation and move request before starting playback.
///
/// Requires a live held player and initialized chaser work with its front/rear
/// animation set already selected. `playerClip` is 1 (throw) or 3 (close catch).
/// Clears whole-coordinate displacement, enables grid/body collision requests,
/// retains the player's current control state and marks the catch active.
/// Water Tower also resets its catch timer. Animation dispatch borrows the
/// embedded request synchronously; the prepared movement is sent by later states.
static inline void _desertChaserBeginPlayerHold(Task* playerTask, DesertChaserWork* work, s32 playerClip)
{
    work->playerAnim.animationId       = playerClip;
    work->playerAnim.blend             = ANIMATION_BLEND_RESET;
    work->playerAnim.blendFrames       = 0;
    work->playerMove.displacement.vx   = 0;
    work->playerMove.displacement.vy   = 0;
    work->playerMove.displacement.vz   = 0;
    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
    work->playerMove.keepControl       = 1;
    work->playerHeld                   = 1;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
    work->lastCommand.fields.catchFrames = 0;
#endif
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
}

/// Winds up a lunge, catches the player and selects close-hold or throw recovery.
///
/// Requires the armed work/model/enemy and live player and scene task slots.
/// Positions share the roots' parent frame; bearings use 4096 units per turn.
/// Contact during clip 3 catches only a player in the forward cone who accepts
/// the button-hold request. Accumulated lunge travel below 1000 selects the close
/// catch; greater travel selects the throw. A sufficiently large backward grid
/// push selects wall recoil or the regular build's mesa fall/recovery. The two
/// armed builds share the catch protocol; Water Tower additionally rumbles the
/// pad and suppresses attack damage after death. Scratch storage is frame-local.
static void _desertChaserPursue(Task* task)
{
    enum {
        DESERT_CHASER_LUNGE_ENTRY_TURN_LIMIT    = 0x300,
        DESERT_CHASER_LUNGE_BACKWARD_YAW_LIMIT  = 0x600,
        DESERT_CHASER_LUNGE_BACKWARD_PUSH_TICKS = 11,
        DESERT_CHASER_LUNGE_MIN_PUSH_SQUARED    = 3601,
        DESERT_CHASER_CATCH_PRESS_COUNT         = 128,
        DESERT_CHASER_CATCH_YAW_LIMIT           = 0x180,
        DESERT_CHASER_CATCH_FRONT_YAW_LIMIT     = 0x400,
        DESERT_CHASER_CLOSE_CATCH_TRAVEL        = 1000,
        DESERT_CHASER_PLAYER_CLIP_THROW         = 1,
        DESERT_CHASER_PLAYER_CLIP_CLOSE_CATCH   = 3,
        DESERT_CHASER_LUNGE_WINDUP_TURN_LIMIT   = 0x40,
        DESERT_CHASER_LUNGE_ALIGNED_YAW         = 0x80,
        DESERT_CHASER_LUNGE_TRACK_PLAYER_TICKS  = 15,
        DESERT_CHASER_STATE_MESA_FALL           = 6,
        DESERT_CHASER_BROADCAST_STAGE_KEY       = 9,
        DESERT_CHASER_BROADCAST_AREA_KEY        = 1,
        DESERT_CHASER_BROADCAST_CHASING         = 1,
        DESERT_CHASER_BROADCAST_LUNGING         = 4,
    };
    enum {
        DESERT_CHASER_PLAYER_SCRIPTED_ATTACK_STATE = 10,
        DESERT_CHASER_LUNGE_DUST_PART_7            = (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x300,
        DESERT_CHASER_LUNGE_DUST_PART_9            = (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x500,
        DESERT_CHASER_LUNGE_DUST_PART_14           = (5 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0xA00,
        DESERT_CHASER_LUNGE_DUST_PART_17           = (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x800,
        DESERT_CHASER_ATTACK_THROW_FRONT           = 0,
        DESERT_CHASER_ATTACK_THROW_REAR            = 1,
        DESERT_CHASER_ATTACK_CLOSE_FRONT           = 2,
        DESERT_CHASER_ATTACK_CLOSE_REAR            = 3,
    };
    PlayerStatus* playerStatus = &gPlayerStatus;
    SVECTOR       entryPlayerOffset;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
    SVECTOR dustOffset;
#endif
    DesertChaserWork*          work;
    Enemy*                     enemy;
    GfxCoord*                  rootCoord;
    GfxCoord*                  facingCoord;
    TmdObject*                 model;
    DesertChaserPursueScratch* scratch;
    Task*                      playerTask;
    GameActor*                 playerActor;
    s16                        playerOffsetZ;
    s16                        nextYaw;
    s32                        entryPlayerTurn;
    s32                        relativeYaw;
    u16                        catchYaw;
    u32                        pushLengthSquared;
    u16                        lungeDistance;
    s32                        animationId;
    s32                        emitDust;
    s32                        dustArguments;
    s32                        effectPart;
    s32                        audioPan;

    work        = task->work;
    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerActor = playerTask->work;
    enemy       = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                 = task->extra.tmd;
        entryPlayerOffset.pad = _actorAngleTurnToPlayer(task, &entryPlayerOffset, playerStatus);
        entryPlayerTurn       = (s16)entryPlayerOffset.pad;
        if (entryPlayerTurn > DESERT_CHASER_LUNGE_ENTRY_TURN_LIMIT) {
            work->state = DESERT_CHASER_STATE_TURN_RIGHT;
        } else if (entryPlayerTurn < -DESERT_CHASER_LUNGE_ENTRY_TURN_LIMIT) {
            work->state = DESERT_CHASER_STATE_TURN_LEFT;
        }
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animId                                          = DESERT_CHASER_CLIP_WINDUP;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(task);
        work->wallProbe.shape.ends[1].vz  = DESERT_CHASER_LUNGE_PROBE_REACH;
        work->stateTimer                  = 0;
        work->stateCounter                = 0;
        work->lungeDistance               = 0;
        work->lookYawTarget               = 0;
        work->broadcast.context.loc.stage = DESERT_CHASER_BROADCAST_STAGE_KEY;
        work->broadcast.context.loc.area  = DESERT_CHASER_BROADCAST_AREA_KEY;
        work->broadcast.command           = DESERT_CHASER_BROADCAST_CHASING;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
        return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserPursueScratch);
    if (work->animId == DESERT_CHASER_CLIP_LUNGE) {
        work->stateTimer += 1;
    }
    // A large backward correction interrupts the lunge.
    if ((_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) && (work->stateTimer >= DESERT_CHASER_LUNGE_BACKWARD_PUSH_TICKS)) {
        pushLengthSquared          = ActorContact_ScratchPosition.vx * ActorContact_ScratchPosition.vx;
        scratch->pushLengthSquared = pushLengthSquared;
        scratch->pushLengthSquared = pushLengthSquared + ActorContact_ScratchPosition.vz * ActorContact_ScratchPosition.vz;
        relativeYaw                = _actorAngleTurnToOffset(task->extra.tmd->coords, ActorContact_ScratchPosition.vx, ActorContact_ScratchPosition.vz);
        if ((abs(relativeYaw) >= DESERT_CHASER_LUNGE_BACKWARD_YAW_LIMIT + 1) && (scratch->pushLengthSquared >= (u32)DESERT_CHASER_LUNGE_MIN_PUSH_SQUARED)) {
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
            if (((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, 0, 0)) && _actor00100IsInMesaDropRegion(task)) {
                if (_actor00100FacesMesaDrop(task->extra.tmd->coords)) {
                    work->state = DESERT_CHASER_STATE_MESA_FALL;
                } else {
                    work->state = DESERT_CHASER_STATE_END_LUNGE;
                }
            } else
#endif
            {
                work->state = DESERT_CHASER_STATE_WALL_KNOCKDOWN;
            }
        }
    }
    // Capture the player only after its receiver accepts the hold request.
    if ((_desertChaserAvoidWalk(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->offset) != 0) &&
        (work->animId == DESERT_CHASER_CLIP_LUNGE) && (playerActor->mode != GAME_ACTOR_MODE_SCRIPTED)) {
        work->playerButtonHold.pressCount = DESERT_CHASER_CATCH_PRESS_COUNT;
        rootCoord                         = task->extra.tmd->coords;
        scratch->offset.vx                = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
        scratch->offset.vy                = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
        playerOffsetZ                     = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
        scratch->offset.vz                = playerOffsetZ;
        scratch->catchYaw                 = ratan2(scratch->offset.vx, playerOffsetZ);
        facingCoord                       = task->extra.tmd->coords;
        catchYaw                          = scratch->catchYaw - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
        scratch->catchYaw                 = catchYaw;
        relativeYaw                       = _actorAngleNormalizeYaw(catchYaw);
        scratch->catchYaw                 = relativeYaw;
        if (abs(relativeYaw) < DESERT_CHASER_CATCH_YAW_LIMIT) {
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
            printf("EM01 PLAYER WORK %d, %d\n", playerActor->mode, playerActor->state);
#endif
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &scratch->offset);
                catchYaw            = ratan2(scratch->offset.vx, scratch->offset.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                scratch->catchYaw   = catchYaw;
                scratch->catchYaw   = _actorAngleNormalizeYaw(catchYaw);
                scratch->turn       = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
                scratch->offset.vy  = 0;
                scratch->offset.vx  = -scratch->offset.vx;
                scratch->offset.vz  = -scratch->offset.vz;
                scratch->playerTurn = _actorAngleTurnToOffset(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
                if (abs(scratch->playerTurn) < DESERT_CHASER_CATCH_FRONT_YAW_LIMIT) {
                    work->playerAnim.source.sets = _gDesertChaserFrontCatchPlayerAnimSets;
                } else {
                    work->playerAnim.source.sets = _gDesertChaserRearCatchPlayerAnimSets;
                    scratch->catchYaw            = scratch->catchYaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                }
                work->playerPlacement.rot.vx = 0;
                work->playerPlacement.rot.vy = scratch->catchYaw;
                work->playerPlacement.rot.vz = 0;
                work->playerPlacement.pos.vx = playerTask->extra.tmd->coords->coord.t[0];
                work->playerPlacement.pos.vy = playerTask->extra.tmd->coords->coord.t[1];
                work->playerPlacement.pos.vz = playerTask->extra.tmd->coords->coord.t[2];
                TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                if (work->lungeDistance < DESERT_CHASER_CLOSE_CATCH_TRAVEL) {
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                    if (enemy->hp > 0)
#endif
                    {
                        if (abs(scratch->playerTurn) < DESERT_CHASER_CATCH_FRONT_YAW_LIMIT) {
                            scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, DESERT_CHASER_ATTACK_CLOSE_FRONT);
                        } else {
                            scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, DESERT_CHASER_ATTACK_CLOSE_REAR);
                        }
                    }
                    if (scratch->playerKilled != 1) {
                        _desertChaserBeginPlayerHold(playerTask, work, DESERT_CHASER_PLAYER_CLIP_CLOSE_CATCH);
                    }
                    work->state = DESERT_CHASER_STATE_CLOSE_CATCH;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                    padScriptSpawnVariableMotorRamp(3, 0xFF, 8);
#endif
                } else {
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                    if (enemy->hp > 0)
#endif
                    {
                        if (abs(scratch->playerTurn) < DESERT_CHASER_CATCH_FRONT_YAW_LIMIT) {
                            scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, DESERT_CHASER_ATTACK_THROW_FRONT);
                        } else {
                            scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, DESERT_CHASER_ATTACK_THROW_REAR);
                        }
                    }
                    if (scratch->playerKilled == 1) {
                        // Fatal capture leaves the player in input-wait state; zero HP gates its attack logic.
                        ((GameActor*)playerTask->work)->state = DESERT_CHASER_PLAYER_SCRIPTED_ATTACK_STATE;
                    }
                    _desertChaserBeginPlayerHold(playerTask, work, DESERT_CHASER_PLAYER_CLIP_THROW);
                    work->state = DESERT_CHASER_STATE_THROW;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                    padScriptSpawnVariableMotorRamp(8, 0xFF, 8);
#endif
                }
            }
        }
        scratch->playerBearing = _actorAngleTurnToPlayer(task, &scratch->offset, &gPlayerStatus);
    } else {
        relativeYaw            = _actorAngleTurnToPlayer(task, &scratch->offset, &gPlayerStatus);
        scratch->playerBearing = relativeYaw;
        if ((abs(relativeYaw) >= DESERT_CHASER_LUNGE_BACKWARD_YAW_LIMIT + 1) && (work->animId == DESERT_CHASER_CLIP_LUNGE)) {
            work->state = DESERT_CHASER_STATE_END_LUNGE;
        }
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    scratch->turn                         = _actorAngleTurnToOffset(task->extra.tmd->coords, work->playerDelta.vx, work->playerDelta.vz);
    _desertChaserAnimTick(task);
    if (work->animId == DESERT_CHASER_CLIP_WINDUP) {
        if (scratch->turn >= DESERT_CHASER_LUNGE_WINDUP_TURN_LIMIT + 1) {
            scratch->turn = DESERT_CHASER_LUNGE_WINDUP_TURN_LIMIT;
        }
        if (scratch->turn < -DESERT_CHASER_LUNGE_WINDUP_TURN_LIMIT) {
            scratch->turn = -DESERT_CHASER_LUNGE_WINDUP_TURN_LIMIT;
        }
        facingCoord   = task->extra.tmd->coords;
        nextYaw       = (u16)scratch->turn + ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
        scratch->turn = nextYaw;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, nextYaw, 1);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        if (_desertChaserCapsuleTouchesGrid(task) != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_LUNGE_GRID_STEP);
            lungeDistance = (u16)work->lungeDistance + DESERT_CHASER_LUNGE_GRID_STEP;
        } else {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_LUNGE_STEP);
            lungeDistance = (u16)work->lungeDistance + DESERT_CHASER_LUNGE_STEP;
        }
        work->lungeDistance = lungeDistance;
    }
    if (work->animId == DESERT_CHASER_CLIP_WINDUP) {
        work->stateCounter = (u16)work->stateCounter + 1;
    }
    if (work->stateCounter > work->windupFrames) {
        animationId = work->animId;
        if (animationId == DESERT_CHASER_CLIP_WINDUP) {
            if ((abs(scratch->playerBearing) < DESERT_CHASER_LUNGE_ALIGNED_YAW) || (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                work->animId      = DESERT_CHASER_CLIP_LUNGE;
                work->animRequest = animationId;
                audioPan          = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(SOUND_DESERT_CHASER_LUNGE, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
#if !DESERT_CHASER_WATER_TOWER_BEHAVIOR
                work->broadcast.context.loc.stage = DESERT_CHASER_BROADCAST_STAGE_KEY;
                work->broadcast.context.loc.area  = DESERT_CHASER_BROADCAST_AREA_KEY;
                work->broadcast.command           = DESERT_CHASER_BROADCAST_LUNGING;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
#endif
            }
        }
    }
    if ((work->stateCounter < DESERT_CHASER_LUNGE_TRACK_PLAYER_TICKS) || (abs(scratch->playerBearing) >= DESERT_CHASER_LUNGE_ALIGNED_YAW + 1)) {
        if (work->animId == DESERT_CHASER_CLIP_WINDUP) {
            work->playerDelta.vx = playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0];
            work->playerDelta.vy = playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1];
            work->playerDelta.vz = playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2];
        }
    }
    if (work->animId == DESERT_CHASER_CLIP_LUNGE) {
        // Dust uses limb-local XYZ: stack storage in Water Tower, work storage in regular combat.
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case 5:
                emitDust      = 1;
                effectPart    = 7;
                dustArguments = DESERT_CHASER_LUNGE_DUST_PART_7;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 0x2BC;
#else
                work->effectOffset.vz = 0;
                work->effectOffset.vx = 0;
                work->effectOffset.vy = 0x2BC;
#endif
                break;
            case 8:
                emitDust      = 1;
                effectPart    = 9;
                dustArguments = DESERT_CHASER_LUNGE_DUST_PART_9;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 0x2BC;
#else
                work->effectOffset.vz = 0;
                work->effectOffset.vx = 0;
                work->effectOffset.vy = 0x2BC;
#endif
                break;
            case 10:
                emitDust      = 1;
                effectPart    = 14;
                dustArguments = DESERT_CHASER_LUNGE_DUST_PART_14;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 0x258;
#else
                work->effectOffset.vz = 0;
                work->effectOffset.vx = 0;
                work->effectOffset.vy = 0x258;
#endif
                break;
            case 13:
                emitDust      = 1;
                effectPart    = 17;
                dustArguments = DESERT_CHASER_LUNGE_DUST_PART_17;
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 0x258;
#else
                work->effectOffset.vz = 0;
                work->effectOffset.vx = 0;
                work->effectOffset.vy = 0x258;
#endif
                break;
            default:
                emitDust      = 0;
                effectPart    = 0;
                dustArguments = 1;
                break;
        }
        if ((gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) && (emitDust == 1)) {
#if DESERT_CHASER_WATER_TOWER_BEHAVIOR
            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[effectPart], dustArguments | DESERT_CHASER_CUE_DUST_RECURSIVE, &dustOffset);
#else
            effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[effectPart], dustArguments | DESERT_CHASER_CUE_DUST_RECURSIVE, &work->effectOffset);
#endif
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserPursueScratch);
}
