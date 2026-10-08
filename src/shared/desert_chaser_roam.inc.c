/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Circles the player between generated waypoints, resuming pursuit when eligible.
///
/// Requires the armed work/model/enemy and live player model. Positions use the
/// root's parent units and bearings use 4096 units per turn. Waypoints alternate
/// around the player's bearing; the player-offset scratch also becomes the next
/// waypoint direction. After the look delay and chase holdoff, close range starts
/// pursuit. The delayed far-range test measures the avoidance displacement's
/// bearing, including zero when no push occurred, rather than the player's bearing.
/// The regular build gates looking by sight/placement frame and also reacts to
/// lost target locks or active Parasite Energy. Water Tower decrements holdoff here
/// and applies arena exclusion. Both scratch reservations are released before return.
static void _desertChaserRoam(Task* task)
{
    enum {
        DESERT_CHASER_ROAM_NEAR_RADIUS           = 1500,
        DESERT_CHASER_ROAM_FAR_RADIUS            = 8000,
        DESERT_CHASER_ROAM_FAR_LOOK_TICKS        = 451,
        DESERT_CHASER_ROAM_ENTRY_WAYPOINT_LENGTH = 1000,
        DESERT_CHASER_ROAM_WAYPOINT_LENGTH       = 2000,
        DESERT_CHASER_ROAM_ENTRY_WAYPOINT_YAW    = 1000,
        DESERT_CHASER_ROAM_WAYPOINT_YAW          = 750,
        DESERT_CHASER_ROAM_TURN_LIMIT            = 0x20,
        DESERT_CHASER_ROAM_LONG_TURN_THRESHOLD   = 0x600,
        DESERT_CHASER_ROAM_LOOK_TURN_THRESHOLD   = 0x100,
        DESERT_CHASER_ROAM_AVOIDANCE_YAW_LIMIT   = 0x300,
        DESERT_CHASER_ROAM_PLAYER_FACING_LIMIT   = 0x600,
        DESERT_CHASER_ROAM_WAIST_TURN_SCALE      = 16,
    };
    s32                      chaseRadius = DESERT_CHASER_ROAM_NEAR_RADIUS;
    Enemy*                   enemy;
    DesertChaserWork*        work;
    WorldCollisionContact*   frontContacts;
    GfxCoord*                entryCoord;
    GfxCoord*                patrolCoord;
    GfxCoord*                detectionCoord;
    GfxCoord*                turnCoord;
    MATRIX*                  waypointRotation;
    DesertChaserRoamScratch* scratch;
    SVECTOR*                 playerOffset;
    SVECTOR*                 detectionOffset;
    SVECTOR*                 waypointDirection;
    DesertChaserRoamScratch* frameCursor;
    TmdObject*               model;
    s16                      lookTurn;
    s16                      patrolTurn;
    s16                      nextYaw;
    s32                      playerHeadingX;
    s16                      bearingFromPlayer;
    s16                      playerOffsetZ;
    s32                      patrolTurnMagnitude;
    s32                      lookTurnMagnitude;
    s16                      longWayTurn;
    s32                      fullTurnMagnitude;
    s16                      wrappedPlayerBearing;
    s32                      turnDelta;
    s32                      avoidanceTurn;
    s32                      playerFacingDifference;
    u16                      unsignedPatrolTurn;
    work = task->work;
#if !DESERT_CHASER_RUN_SEQUENCE
    enemy = task->spawnArg2.pointer; /* also read by the look-around below */
#endif
    if (work->stateEntered != 0) {
        model   = task->extra.tmd;
        scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserRoamScratch);
#if DESERT_CHASER_RUN_SEQUENCE
        enemy = task->spawnArg2.pointer;
#endif
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
#if DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        work->blendActive                                    = 0;
        work->animId                                         = DESERT_CHASER_CLIP_PATROL;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        _desertChaserAnimTick(task);
        _desertChaserAnimTick(task);
        work->stateTimer          = 0;
        work->stateCounter        = 0;
        entryCoord                = task->extra.tmd->coords;
        scratch->toPatrolPoint.vx = (s16)(gPlayerStatus.coordMtx->t[0] - entryCoord->coord.t[0]);
        scratch->toPatrolPoint.vy = gPlayerStatus.coordMtx->t[1] - entryCoord->coord.t[1];
        playerOffsetZ             = gPlayerStatus.coordMtx->t[2] - entryCoord->coord.t[2];
        scratch->toPatrolPoint.vz = playerOffsetZ;
        work->lookYawTarget       = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPatrolPoint.vx, playerOffsetZ);
        waypointRotation          = &scratch->rotation;
        gfxRotMatrixY(waypointRotation, (s16)ratan2((s32)scratch->toPatrolPoint.vx, (s32)scratch->toPatrolPoint.vz) + DESERT_CHASER_ROAM_ENTRY_WAYPOINT_YAW, 1);
        gfxReadMatrixZAxis(waypointRotation, &scratch->toPatrolPoint);
        VectorNormalSS(&scratch->toPatrolPoint, &scratch->toPatrolPoint);
        gte_lddp(DESERT_CHASER_ROAM_ENTRY_WAYPOINT_LENGTH);
        gte_ldsv(&scratch->toPatrolPoint);
        gte_gpf12();
        gte_stsv(&scratch->toPatrolPoint);
        work->patrolTarget      = 0;
        work->patrolPoints[0].x = (s16)((u16)scratch->toPatrolPoint.vx + task->extra.tmd->coords->coord.t[0]);
        work->patrolPoints[0].z = (s16)((u16)scratch->toPatrolPoint.vz + task->extra.tmd->coords->coord.t[2]);
        SCRATCH_STACK_RELEASE_BLOCK(DesertChaserRoamScratch);
        work->wallProbe.shape.ends[1].vz = DESERT_CHASER_PATROL_PROBE_REACH;
        return;
    }
    work->stateCounter              += 1;
    frameCursor                      = SCRATCH_STACK_CURSOR(DesertChaserRoamScratch);
    scratch                          = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserRoamScratch);
    frameCursor[-1].toPatrolPoint.vx = (s16)(work->patrolPoints[work->patrolTarget].x - task->extra.tmd->coords->coord.t[0]);
    scratch->toPatrolPoint.vy        = 0;
    scratch->toPatrolPoint.vz        = work->patrolPoints[work->patrolTarget].z - task->extra.tmd->coords->coord.t[2];
    patrolCoord                      = task->extra.tmd->coords;
    frameCursor[-1].toPlayer.vx      = (s16)(gPlayerStatus.coordMtx->t[0] - patrolCoord->coord.t[0]);
    playerOffset                     = &frameCursor[-1].toPlayer;
    playerOffset->vy                 = gPlayerStatus.coordMtx->t[1] - patrolCoord->coord.t[1];
    playerOffset->vz                 = gPlayerStatus.coordMtx->t[2] - patrolCoord->coord.t[2];
    if (!actorOutsideRadius(&scratch->toPatrolPoint, DESERT_CHASER_WAYPOINT_RADIUS) || work->stateTimer >= DESERT_CHASER_STUCK_TICKS) {
        work->lookYawTarget = _actorAngleTurnToOffset(task->extra.tmd->coords, frameCursor[-1].toPlayer.vx, playerOffset->vz);
        if (work->patrolTarget == 0) {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) - DESERT_CHASER_ROAM_WAYPOINT_YAW, 1);
            work->patrolTarget = 1;
        } else {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + DESERT_CHASER_ROAM_WAYPOINT_YAW, 1);
            work->patrolTarget = 0;
        }
        waypointDirection = &scratch->toPlayer;
        gfxReadMatrixZAxis(&scratch->rotation, waypointDirection);
        VectorNormalSS(waypointDirection, waypointDirection);
        gte_lddp(DESERT_CHASER_ROAM_WAYPOINT_LENGTH);
        gte_ldsv(waypointDirection);
        gte_gpf12();
        gte_stsv(waypointDirection);
        work->patrolPoints[work->patrolTarget].x = (s16)((u16)scratch->toPlayer.vx + task->extra.tmd->coords->coord.t[0]);
        work->patrolPoints[work->patrolTarget].z = (s16)((u16)scratch->toPlayer.vz + task->extra.tmd->coords->coord.t[2]);
        work->stateTimer                         = 0;
    }
    _desertChaserAnimTick(task);
    work->lookYawTarget = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPlayer.vx, scratch->toPlayer.vz);
    turnDelta           = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPatrolPoint.vx, scratch->toPatrolPoint.vz);
    scratch->fullTurn   = (scratch->turn = (s16)turnDelta);
    patrolTurn          = scratch->turn;
    unsignedPatrolTurn  = (u16)scratch->turn;
    patrolTurnMagnitude = abs(scratch->turn);
    if (patrolTurnMagnitude >= DESERT_CHASER_ROAM_LONG_TURN_THRESHOLD + 1) {
        lookTurn          = work->lookYawTarget;
        lookTurnMagnitude = abs(lookTurn);
        if ((lookTurnMagnitude >= DESERT_CHASER_ROAM_LOOK_TURN_THRESHOLD + 1) && ((lookTurn * patrolTurn) < 0)) {
            longWayTurn = unsignedPatrolTurn - ACTOR_TRANSFORM_ANGLE_TURN;
            if (patrolTurn < 0) {
                longWayTurn = unsignedPatrolTurn + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            scratch->turn = longWayTurn;
        }
    }
    if (scratch->turn >= DESERT_CHASER_ROAM_TURN_LIMIT + 1) {
        scratch->turn = DESERT_CHASER_ROAM_TURN_LIMIT;
    }
    if (scratch->turn < -DESERT_CHASER_ROAM_TURN_LIMIT) {
        scratch->turn = -DESERT_CHASER_ROAM_TURN_LIMIT;
    }
    work->waistYawTarget = scratch->turn * DESERT_CHASER_ROAM_WAIST_TURN_SCALE;
    turnCoord            = task->extra.tmd->coords;
    nextYaw              = (u16)scratch->turn + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->turn        = nextYaw;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, (s32)nextYaw, 1);
    frontContacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    if (work->blendActive == 0) {
        if (_desertChaserCapsuleTouchesGrid(task)) {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_PATROL_STEP);
        } else {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_PATROL_STEP);
        }
        frontContacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    }
    // Avoidance overwrites toPatrolPoint with the applied displacement.
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, frontContacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->toPatrolPoint);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) == 1) {
        fullTurnMagnitude = abs(scratch->fullTurn);
        if (fullTurnMagnitude < DESERT_CHASER_ROAM_TURN_LIMIT) {
            work->stateTimer += 1;
        }
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#if !DESERT_CHASER_RUN_SEQUENCE
    if (_playerDetectionSightBlocked(task) != 1)
#endif
    {
        detectionCoord       = task->extra.tmd->coords;
        scratch->toPlayer.vx = (s16)(gPlayerStatus.coordMtx->t[0] - detectionCoord->coord.t[0]);
        detectionOffset      = &scratch->toPlayer;
        detectionOffset->vy  = gPlayerStatus.coordMtx->t[1] - detectionCoord->coord.t[1];
        detectionOffset->vz  = gPlayerStatus.coordMtx->t[2] - detectionCoord->coord.t[2];
        if (work->stateCounter > work->roamLookDelay) {
            if (work->chaseHoldoff <= 0) {
#if !DESERT_CHASER_RUN_SEQUENCE
                /* each chaser looks on its own frame of fifteen */
                if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == (gDisplayState.animFrame % DESERT_CHASER_SIGHT_FRAME_PERIOD))
#endif
                {
                    if (actorOutsideRadius(&scratch->toPlayer, chaseRadius)) {
                        if (!actorOutsideRadius(&scratch->toPlayer, DESERT_CHASER_ROAM_FAR_RADIUS) && work->stateCounter >= DESERT_CHASER_ROAM_FAR_LOOK_TICKS) {
                            avoidanceTurn = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPatrolPoint.vx, scratch->toPatrolPoint.vz);
                            scratch->turn = (s16)avoidanceTurn;
                            avoidanceTurn = abs(avoidanceTurn);
                            if (avoidanceTurn < DESERT_CHASER_ROAM_AVOIDANCE_YAW_LIMIT) {
                                work->state = DESERT_CHASER_STATE_PURSUE;
                            }
                        }
                    } else {
                        work->state = DESERT_CHASER_STATE_PURSUE;
                    }
                    playerHeadingX         = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
                    scratch->playerYaw     = ratan2((s32)playerHeadingX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
                    bearingFromPlayer      = ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                    scratch->yawFromPlayer = bearingFromPlayer;
                    wrappedPlayerBearing   = _actorAngleNormalizeYaw(bearingFromPlayer);
                    scratch->yawFromPlayer = wrappedPlayerBearing;
                    playerFacingDifference = wrappedPlayerBearing - scratch->playerYaw;
                    if (playerFacingDifference < 0) {
                        playerFacingDifference = -playerFacingDifference;
                    }
                    if ((playerFacingDifference >= DESERT_CHASER_ROAM_PLAYER_FACING_LIMIT + 1)
#if !DESERT_CHASER_RUN_SEQUENCE
                        || (worldTargetGetActorLockMask(&enemy->node) == 0)
#endif
                    ) {
                        work->state = DESERT_CHASER_STATE_PURSUE;
                    }
                }
            }
#if DESERT_CHASER_RUN_SEQUENCE
            else {
                work->chaseHoldoff -= 1;
            }
#endif
        }
    }
#if !DESERT_CHASER_RUN_SEQUENCE
    if ((work->chaseHoldoff <= 0) && (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_PE_ACTIVE)) {
        work->state = DESERT_CHASER_STATE_PURSUE;
    }
#else
    _actor421600PushOutsideArenaCenter(task->extra.tmd->coords);
#endif
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserRoamScratch);
#if DESERT_CHASER_RUN_SEQUENCE
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#endif
}
