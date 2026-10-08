/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Limits the waypoint turn, replaces root yaw and restores normal model scale.
///
/// Requires a live `Task*` and reserved `ActorTurnScratch*` as side-effect-free
/// pointer expressions, and a positive limit in 4096ths of a turn. Arguments
/// occur repeatedly; the scratch angle becomes the absolute root heading.
/// Translation stays intact. Expands to a block; invoke inside a braced block.
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
/// turns toward the old endpoint. The variant selects the turn limit and step.
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
#if ODD_STRANGER_VARIANT == 2
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
#if ODD_STRANGER_VARIANT == 1
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
