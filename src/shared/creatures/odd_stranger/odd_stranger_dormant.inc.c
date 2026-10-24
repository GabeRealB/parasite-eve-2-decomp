/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Waits in `ODD_STRANGER_STATE_DORMANT_SCRIPTED` for the player or combat noise.
///
/// Installs the scripted dormant animation in the borrowed bank on entry,
/// then queues the Patio soundId once and emits the hit effect at cue 4.
/// Player proximity or noise selects `ODD_STRANGER_STATE_ALERT`; only the
/// proximity path stops the soundId. Requires live work, enemy and model.
static void _oddStrangerDormantScripted(Task* task)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        root;
    SVECTOR          toPlayer;
    SVECTOR*         toPlayerPtr;
    s32              soundId;
    s32              pan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        _gOddStrangerAnimSets[ODD_STRANGER_ANIM_DORMANT_SCRIPTED] = &_gStrangerOddScriptedDormantClip;
        work->animId                                              = ODD_STRANGER_ANIM_DORMANT_SCRIPTED;
        work->animRequest                                         = ODD_STRANGER_ANIM_REQUEST_RESET;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYawTarget           = 0;
        work->stateTimer              = 0;
    } else if (work->stateTimer == 0) {
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AKROPOLIS_PATIO_STRANGER_DORMANT;
        pan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->stateTimer = 1;
    }
    _oddStrangerDriveAnimation(task);
    if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 4 && work->lastCueFrame != (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK)) {
        work->effectArg.coord      = task->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = ODD_STRANGER_PUFF_UNUSED_LOW_ARG;
        work->effectArg.spawnArgHi = 2;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), task->extra.tmd->coords + 5, NULL, &work->effectArg);
    }
    work->lastCueFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    root               = task->extra.tmd->coords;
    toPlayerPtr        = &toPlayer;
    toPlayer.vx        = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
    toPlayerPtr->vy    = gPlayerStatus.coordMtx->t[1] - root->coord.t[1];
    toPlayerPtr->vz    = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
    if (!_oddStrangerOutOfRange(toPlayerPtr, work->noticeRadius)) {
        sndEvtRequestScriptStop(SOUND_AKROPOLIS_PATIO_STRANGER_DORMANT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sceneEngageBattle(1);
        work->state = ODD_STRANGER_STATE_ALERT;
    }
    if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST) {
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
        sceneEngageBattle(1);
#endif
        work->state = ODD_STRANGER_STATE_ALERT;
    }
}

/// Limits the waypoint turn, replaces root yaw and restores normal model scale.
///
/// Requires a live TMD `Task*`, reserved `ActorTurnScratch*` and a nonnegative
/// signed-int limit that fits s16, in 4096ths of a turn. Arguments are evaluated
/// repeatedly and must be stable and side-effect-free. The ordered clamps
/// overwrite `angle`; adding the matrix heading then narrows it to s16 without
/// wrapping to a half turn. Replaces pitch, roll and prior scale with yaw at
/// `ODD_STRANGER_ROOT_SCALE`; translation stays intact and composition becomes
/// dirty. The callees release their matrix scratch and clobber GTE state;
/// turn scratch stays reserved. Invoke as a standalone braced statement.
#define ODD_STRANGER_TURN_PATROL_ROOT(task, turn, turnLimit)                                                          \
    {                                                                                                                 \
        if ((turn)->angle > (turnLimit)) {                                                                            \
            (turn)->angle = (turnLimit);                                                                              \
        }                                                                                                             \
        if ((turn)->angle < -(turnLimit)) {                                                                           \
            (turn)->angle = -(turnLimit);                                                                             \
        }                                                                                                             \
        (turn)->angle += ratan2(-(task)->extra.tmd->coords->coord.m[2][0], (task)->extra.tmd->coords->coord.m[2][2]); \
        gfxRotMatrixY(&(task)->extra.tmd->coords->coord, (turn)->angle, GRAPHICS_ROTATION_REPLACE);                   \
        _actorRenderRescaleYaw((task)->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);                                   \
    }

/// Walks between the two patrol endpoints until sight or combat activity alerts it.
///
/// Handles `ODD_STRANGER_STATE_PATROL` on a live Odd Stranger task with bound
/// rigs, two initialized patrol points and `patrolTarget` in 0..1. Roots share
/// the player's parent-coordinate space. Arrival inside 160 units or 21
/// blocked, nearly forward-facing ticks switches endpoints; this tick still
/// turns toward the old endpoint. The build selects the turn limit and step.
/// A visible player inside the notice radius, or within 4000 units and 768
/// angle units of facing, selects `ALERT`; attack signals do so regardless of
/// sight. Angles use 4096 units per turn. Borrows one turn scratch block plus
/// nested contact, movement and range-test workspace.
static void _oddStrangerPatrol(Task* task)
{
    enum {
        ODD_STRANGER_PATROL_ARRIVAL_RADIUS     = 160,
        ODD_STRANGER_PATROL_BLOCKED_TICKS      = 21,
        ODD_STRANGER_PATROL_FORWARD_TURN_LIMIT = 128,
        ODD_STRANGER_PATROL_SIGHT_RADIUS       = 4000,
        ODD_STRANGER_PATROL_SIGHT_TURN_LIMIT   = 768,
        ODD_STRANGER_PATROL_STALK_SPAWN        = 0x10
    };
    OddStrangerWork*       work;
    ActorTurnScratch*      turn;
    TmdObject*             model;
    WorldCollisionContact* hitContacts;
    GfxCoord*              headingRoot;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        work->stateTimer = 0;
        if ((task->spawnArg1.value >> 16) == ODD_STRANGER_PATROL_STALK_SPAWN) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    } else {
        SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
        turn           = SCRATCH_STACK_CURSOR(ActorTurnScratch);
        turn->delta.vx = work->patrolPoints[work->patrolTarget].x - task->extra.tmd->coords->coord.t[0];
        turn->delta.vy = 0;
        turn->delta.vz = work->patrolPoints[work->patrolTarget].z - task->extra.tmd->coords->coord.t[2];
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401800
        turn->delta.vx = work->patrolPoints[work->patrolTarget].x - task->extra.tmd->coords->coord.t[0];
        turn->delta.vy = 0;
        turn->delta.vz = work->patrolPoints[work->patrolTarget].z - task->extra.tmd->coords->coord.t[2];
#endif
        // Switch endpoints without refreshing this tick's waypoint offset.
        if (!_oddStrangerOutOfRange(&turn->delta, ODD_STRANGER_PATROL_ARRIVAL_RADIUS) || work->stateTimer >= ODD_STRANGER_PATROL_BLOCKED_TICKS) {
            if (work->patrolTarget == 0) {
                work->patrolTarget = 1;
            } else {
                work->patrolTarget = 0;
            }
            work->stateTimer = 0;
        }
        _oddStrangerDriveAnimation(task);
        headingRoot         = task->extra.tmd->coords;
        turn->angle         = _actorAngleNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
        work->lookYawTarget = turn->angle;
        ODD_STRANGER_TURN_PATROL_ROOT(task, turn, ODD_STRANGER_PATROL_TURN_CLAMP);
        if (work->blendActive == 0 && (_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_WALK_STEP) << 16) != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_WALK_STEP);
        }
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
        if ((task->spawnArg1.value >> 16) != ODD_STRANGER_PATROL_STALK_SPAWN) {
            if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
                if (ABS(work->lookYawTarget) < ODD_STRANGER_PATROL_FORWARD_TURN_LIMIT) {
                    work->stateTimer = (u16)work->stateTimer + 1;
                }
            }
        } else {
            hitContacts = work->hitContacts;
            if (_actorContactApplyGridPushback(task->extra.tmd->coords, hitContacts, ARRAY_SIZE(work->hitContacts)) != 1 && _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
                _oddStrangerApplyBodyPushback(task, hitContacts, ARRAY_SIZE(work->hitContacts));
            } else {
                if (ABS(work->lookYawTarget) < ODD_STRANGER_PATROL_FORWARD_TURN_LIMIT) {
                    work->stateTimer = (u16)work->stateTimer + 1;
                }
            }
        }
#else
        if ((task->spawnArg1.value >> 16) != ODD_STRANGER_PATROL_STALK_SPAWN) {
            if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1 &&
                ABS(work->lookYawTarget) < ODD_STRANGER_PATROL_FORWARD_TURN_LIMIT) {
                work->stateTimer++;
            } else {
                _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
        } else {
            if ((_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1 ||
                 _actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) == 1) &&
                ABS(work->lookYawTarget) < ODD_STRANGER_PATROL_FORWARD_TURN_LIMIT) {
                work->stateTimer++;
            } else {
                _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
            }
        }
#endif
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (_playerDetectionSightBlocked(task) != 1) {
            _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &turn->delta);
            if (!_oddStrangerOutOfRange(&turn->delta, work->noticeRadius)) {
                work->state = ODD_STRANGER_STATE_ALERT;
            } else if (!_oddStrangerOutOfRange(&turn->delta, ODD_STRANGER_PATROL_SIGHT_RADIUS)) {
                headingRoot = task->extra.tmd->coords;
                turn->angle = _actorAngleNormalizeYaw(ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
                if (ABS(turn->angle) < ODD_STRANGER_PATROL_SIGHT_TURN_LIMIT) {
                    work->state = ODD_STRANGER_STATE_ALERT;
                }
            }
        }
        if (gSceneCombatState.signals.packed & SCENE_COMBAT_SIGNAL_ATTACK_MASK) {
            work->state = ODD_STRANGER_STATE_ALERT;
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    }
}

#undef ODD_STRANGER_TURN_PATROL_ROOT

/// Clamps the slide turn and replaces the root rotation with a unit-scale yaw.
///
/// Requires a live TMD `Task*`, reserved `ActorTurnScratch*` and a nonnegative
/// signed-int limit that fits s16, in 4096ths of a turn. Arguments are evaluated
/// repeatedly and must be stable, side-effect-free and independent of the
/// block-local `headingRoot`. The ordered clamps overwrite `angle`; adding the
/// matrix heading then narrows it to s16 without wrapping to a half turn.
/// Replaces pitch, roll and prior scale; translation stays intact. Rotation
/// scratch is released by the callee; the caller retains its turn scratch.
/// Does not invalidate composition here. Invoke as a standalone braced statement.
#define ODD_STRANGER_TURN_SLIDE_ROOT(task, turn, turnLimit)                                         \
    {                                                                                               \
        GfxCoord* headingRoot;                                                                      \
                                                                                                    \
        if ((turn)->angle > (turnLimit)) {                                                          \
            (turn)->angle = (turnLimit);                                                            \
        }                                                                                           \
        if ((turn)->angle < -(turnLimit)) {                                                         \
            (turn)->angle = -(turnLimit);                                                           \
        }                                                                                           \
        headingRoot    = (task)->extra.tmd->coords;                                                 \
        (turn)->angle += ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]);           \
        gfxRotMatrixY(&(task)->extra.tmd->coords->coord, (turn)->angle, GRAPHICS_ROTATION_REPLACE); \
    }

/// Coasts toward the player with the circle dash's remaining forward step.
///
/// Handles `ODD_STRANGER_STATE_SLIDE` on a live Odd Stranger task with bound
/// animation rigs and a live player in the same root-parent coordinate space.
/// Turns at most 64 angle units per tick (4096 per turn), preserves the full
/// turn as the look target, and reduces the inherited step by 10 coordinate
/// units per tick. The clip boundary or a spent step selects `TURN_AROUND`.
/// Borrows one turn scratch block plus the nested movement/contact workspace.
static void _oddStrangerSlide(Task* task)
{
    enum {
        ODD_STRANGER_SLIDE_ANIM         = 18,
        ODD_STRANGER_SLIDE_RATE         = 30, // Sixteenths of a frame per tick
        ODD_STRANGER_SLIDE_TURN_LIMIT   = 64, // 4096 units per turn
        ODD_STRANGER_SLIDE_DECELERATION = 10
    };
    OddStrangerWork*  work;
    Enemy*            enemy;
    TmdObject*        model;
    ActorTurnScratch* turn;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy             = task->spawnArg2.pointer;
        model             = task->extra.tmd;
        work->animId      = ODD_STRANGER_SLIDE_ANIM;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        model->flags      = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->lookYaw                 = 0;
        work->animRate                = ODD_STRANGER_SLIDE_RATE;
    }
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    // The look keeps the full player bearing; only the root turn is limited.
    turn->angle         = _actorAngleTurnToPlayer(task, &turn->delta, &gPlayerStatus);
    work->lookYawTarget = turn->angle;
    ODD_STRANGER_TURN_SLIDE_ROOT(task, turn, ODD_STRANGER_SLIDE_TURN_LIMIT);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    }
    if ((_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, work->slideStep) << 0x10) != 0) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, (u16)work->slideStep);
    }
    if (work->slideStep > 0) {
        work->slideStep = (u16)work->slideStep - ODD_STRANGER_SLIDE_DECELERATION;
        if (work->slideStep < 0) {
            work->slideStep = 0;
        }
    }
    _oddStrangerDriveAnimation(task);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) || work->slideStep == 0) {
        work->state = ODD_STRANGER_STATE_TURN_AROUND;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

#undef ODD_STRANGER_TURN_SLIDE_ROOT

/// Applies the retreat's asymmetric turn and restores the normal model scale.
///
/// Requires a live model root and reserved chase scratch holding the signed
/// player turn. `turnLimit` is positive in 4096ths of a turn (128 at the call):
/// values above it are capped then halved; values below its negative retain
/// the full negative cap; all other values are arithmetically halved.
/// Replaces scratch turn with the resulting absolute heading. Discards pitch
/// and roll, retains translation and marks composition dirty. Nested matrix
/// scratch is released here; the caller retains the chase block.
static inline void _oddStrangerTurnBackOffRoot(Task* task, ActorChaseScratch* retreat, s16 turnLimit)
{
    if (retreat->turn >= (turnLimit + 1)) {
        retreat->turn = turnLimit;
    }
    if (retreat->turn < -turnLimit) {
        retreat->turn = -turnLimit;
    } else {
        retreat->turn = retreat->turn >> 1;
    }
    retreat->turn += ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, retreat->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
}

/// Faces the player, retreats for 19 ticks, then turns aside and chases.
///
/// Handles the unselected `ODD_STRANGER_STATE_BACK_OFF` on a live Odd Stranger
/// task with bound rigs and a live player in the same root-parent space.
/// The walk changes to the retreat clip once the absolute turn is at most 128
/// angle units. Positive turns are capped and halved; turns below -128 retain
/// the full negative cap. The retreat is 16 coordinate units per tick, followed
/// by a 1200-unit relative yaw turn (4096 per turn).
/// Borrows one chase scratch block plus nested contact/movement workspace.
static void _oddStrangerBackOff(Task* task)
{
    enum {
        ODD_STRANGER_BACK_OFF_RATE       = 22, // Sixteenths of a frame per tick
        ODD_STRANGER_BACK_OFF_ANIM       = 17,
        ODD_STRANGER_BACK_OFF_TURN_LIMIT = 128,
        ODD_STRANGER_BACK_OFF_STEP       = -16,
        ODD_STRANGER_BACK_OFF_TICKS      = 19,
        ODD_STRANGER_BACK_OFF_EXIT_TURN  = 1200
    };
    OddStrangerWork*   work;
    ActorChaseScratch* retreat;
    TmdObject*         model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ODD_STRANGER_BACK_OFF_RATE;
        work->animId            = ODD_STRANGER_ANIM_WALK;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _oddStrangerDriveAnimation(task);
        return;
    }
    _oddStrangerDriveAnimation(task);
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    retreat             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    retreat->turn       = _actorAngleTurnToPlayer(task, &retreat->delta, &gPlayerStatus);
    work->lookYawTarget = retreat->turn;
    if (ABS(retreat->turn) < (ODD_STRANGER_BACK_OFF_TURN_LIMIT + 1) && work->animId == ODD_STRANGER_ANIM_WALK) {
        work->animRate    = ODD_STRANGER_BACK_OFF_RATE;
        work->animId      = ODD_STRANGER_BACK_OFF_ANIM;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->stateTimer  = 0;
        _oddStrangerDriveAnimation(task);
    }
    _oddStrangerTurnBackOffRoot(task, retreat, ODD_STRANGER_BACK_OFF_TURN_LIMIT);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Only the retreat clip translates backward; alignment used the walk clip.
    if (work->animId == ODD_STRANGER_BACK_OFF_ANIM) {
        work->stateTimer++;
        if ((_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_BACK_OFF_STEP) << 16) != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_BACK_OFF_STEP);
        }
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
            _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if (work->stateTimer >= ODD_STRANGER_BACK_OFF_TICKS) {
            if (work->lookYawTarget <= 0) {
                gfxRotMatrixY(&task->extra.tmd->coords->coord, ODD_STRANGER_BACK_OFF_EXIT_TURN, GRAPHICS_ROTATION_COMPOSE);
            } else {
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -ODD_STRANGER_BACK_OFF_EXIT_TURN, GRAPHICS_ROTATION_COMPOSE);
            }
            work->state = ODD_STRANGER_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

/// Rebuilds the watch pose's yaw-only root without applying the player turn.
///
/// Requires a live model root and caller-reserved chase scratch. Replaces
/// `watch->turn` with the root heading in 4096 units per turn, discards pitch
/// and roll, and replaces the scale with the normal Q12 model scale.
/// Translation is retained and composition marked dirty. Nested matrix scratch
/// is released here; the caller retains and releases the chase block.
static inline void _oddStrangerRebuildWatchRoot(Task* task, ActorChaseScratch* watch)
{
    GfxCoord* headingRoot;

    // Both clamps cancel the player turn; their ordered stores preserve matching.
    if (watch->turn > 0) {
        watch->turn = 0;
    }
    if (watch->turn < 0) {
        watch->turn = 0;
    }
    headingRoot  = task->extra.tmd->coords;
    watch->turn += ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, watch->turn, GRAPHICS_ROTATION_REPLACE);
    _actorRenderRescaleYaw(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);
}

/// Watches the player without turning the root, then resumes the chase.
///
/// Handles `ODD_STRANGER_STATE_WATCH` on a live Odd Stranger task with bound
/// rigs and a live player in the same root-parent space. The full relative
/// bearing updates the look target, but both turn clamps reduce root rotation
/// to zero, preserving its heading while restoring the normal model scale.
/// The primary clip boundary selects `CHASE` before playback advances.
/// Borrows one chase scratch block plus the yaw-rescale workspace.
static void _oddStrangerWatch(Task* task)
{
    OddStrangerWork*   work;
    ActorChaseScratch* watch;
    TmdObject*         model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ANIMATION_RATE_ONE;
        work->animId            = ODD_STRANGER_ANIM_WATCH;
        work->blendActive       = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_IN_PLACE_GRID_ENABLED
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        _oddStrangerDriveAnimation(task);
        work->stateTimer = 0;
        return;
    }
    work->stateTimer += 1;
    SCRATCH_STACK_RESERVE_BLOCK(ActorChaseScratch);
    watch                                 = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ODD_STRANGER_STATE_CHASE;
    }
    watch->turn         = _actorAngleTurnToPlayer(task, &watch->delta, &gPlayerStatus);
    work->lookYawTarget = watch->turn;
    _oddStrangerRebuildWatchRoot(task, watch);
    _oddStrangerDriveAnimation(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}
