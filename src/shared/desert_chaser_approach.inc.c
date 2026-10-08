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
#if !DESERT_CHASER_RUN_SEQUENCE
    enemy = task->spawnArg2.pointer; /* also read by the look-around below */
#endif
    if (work->stateEntered != 0) {
#if DESERT_CHASER_RUN_SEQUENCE
        enemy = task->spawnArg2.pointer;
#endif
        model                         = task->extra.tmd;
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
        work->waistYawTarget                                 = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if !DESERT_CHASER_RUN_SEQUENCE
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
#if !DESERT_CHASER_RUN_SEQUENCE
    if (_playerDetectionSightBlocked(task) != 1)
#endif
    {
        detectionCoord    = task->extra.tmd->coords;
        scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - detectionCoord->coord.t[0];
        scratch->delta.vy = gPlayerStatus.coordMtx->t[1] - detectionCoord->coord.t[1];
        scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - detectionCoord->coord.t[2];
#if !DESERT_CHASER_RUN_SEQUENCE
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
#if !DESERT_CHASER_RUN_SEQUENCE
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE)
            work->state = DESERT_CHASER_STATE_ROAM;
#endif
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}
