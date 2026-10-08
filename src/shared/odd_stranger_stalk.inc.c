/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Limits the stalking turn, replaces root yaw and restores normal model scale.
///
/// Requires a live `Task*` and reserved `ActorChaseScratch*` as side-effect-free
/// pointer expressions, and a positive limit in 4096ths of a turn. Arguments
/// occur repeatedly; the scratch angle becomes the absolute root heading.
/// Translation stays intact. Expands to a block; invoke inside a braced block.
#define ODD_STRANGER_TURN_STALK_ROOT(task, chase, turnLimit)                                        \
    {                                                                                               \
        GfxCoord* headingRoot;                                                                      \
                                                                                                    \
        if ((chase)->turn > (turnLimit)) {                                                          \
            (chase)->turn = (turnLimit);                                                            \
        }                                                                                           \
        if ((chase)->turn < -(turnLimit)) {                                                         \
            (chase)->turn = -(turnLimit);                                                           \
        }                                                                                           \
        headingRoot    = (task)->extra.tmd->coords;                                                 \
        (chase)->turn += ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]);           \
        gfxRotMatrixY(&(task)->extra.tmd->coords->coord, (chase)->turn, GRAPHICS_ROTATION_REPLACE); \
        _actorRenderRescaleYaw((task)->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE);                 \
    }

/// Snapshots the player heading and a fresh reverse bearing for stalking.
///
/// Requires live enemy/player roots in the same parent frame and reserved
/// scratch. Reads the player heading before refreshing XYZ player-minus-enemy
/// translation, narrowed to signed halfwords; vector pad and turn stay intact.
/// The reverse X/Z bearing wraps to [-2048, 2048] in 4096ths of a turn. No
/// root is composed or rotated. Scratch remains reserved for the caller.
static __inline__ void _oddStrangerReadStalkPlayerBearings(const Task* task, ActorChaseScratch* chase)
{
#if ODD_STRANGER_VARIANT == 1
    s16 reverseBearing;
#endif

    chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                              (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
#if ODD_STRANGER_VARIANT == 1
    reverseBearing       = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer = reverseBearing;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(reverseBearing);
#else
    chase->yawFromPlayer = ratan2(chase->delta.vx, chase->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    chase->yawFromPlayer = _actorAngleNormalizeYaw(chase->yawFromPlayer);
#endif
}

/// Walks toward the player, veers while sight is blocked and grabs within reach.
///
/// Handles `ODD_STRANGER_STATE_STALK` on a live Odd Stranger task with bound
/// rigs and a live player in the same root-parent space. With clear sight, a
/// signed turn below 512 angle units and a range inside 1100 coordinate units
/// permit `GRAB`; 91 visible ticks select `WATCH` and override that request.
/// Blocked sight picks a random veer side and reverses it after the counter
/// reaches 241 (variant 2 increments that counter twice per blocked tick).
/// Root turning is limited to 32 angle units per tick, with 4096 per turn.
/// Movement uses the variant's step, or five units during blending; the grab
/// cooldown counts down once per call. Borrows one chase scratch block plus
/// nested contact, movement and range-test workspace.
static void _oddStrangerStalk(Task* task)
{
    enum {
        ODD_STRANGER_STALK_SPAWN           = 0x10,
        ODD_STRANGER_STALK_GRAB_TURN_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ODD_STRANGER_STALK_GRAB_RADIUS     = 1100,
        ODD_STRANGER_STALK_WATCH_TICKS     = 91,
        ODD_STRANGER_STALK_VEER_TICKS      = 241,
        ODD_STRANGER_STALK_TURN_LIMIT      = 32,
        ODD_STRANGER_STALK_BLEND_STEP      = 5
    };
    OddStrangerWork*   work;
    TmdObject*         model;
    GfxCoord*          headingRoot;
    ActorChaseScratch* savedCursor;
    ActorChaseScratch* chase;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->hitBody.radius    = ODD_STRANGER_BODY_RADIUS;
        work->animRequest       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate          = ODD_STRANGER_STALK_RATE;
        work->animId            = ODD_STRANGER_ANIM_WALK;
        work->blendActive       = 0;
        work->dashCount         = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        sceneEngageBattle(1);
        work->stateTimer  = 0;
        work->exitCounter = 0;
        if ((task->spawnArg1.value >> 16) == ODD_STRANGER_STALK_SPAWN) {
            work->hitBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    work->stateTimer++;
    work->exitCounter++;
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    SCRATCH_STACK_CURSOR(ActorChaseScratch) = savedCursor - 1;
    chase                                   = savedCursor - 1;
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) != 1) {
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts)) != 1) {
            _oddStrangerApplyBodyPushback(task, work->hitContacts, ARRAY_SIZE(work->hitContacts));
        }
    }
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _oddStrangerDriveAnimation(task);
    // Animation/contact updates precede the final player-offset snapshot.
    _oddStrangerReadStalkPlayerBearings(task, chase);
#if ODD_STRANGER_VARIANT == 1
    headingRoot         = task->extra.tmd->coords;
    chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
    work->lookYawTarget = chase->turn;
#endif
    if (_playerDetectionSightBlocked(task) != 1) {
        work->stateTimer    = 0;
        headingRoot         = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        // The grab gate is signed, so all negative turns pass the angle test.
        if (chase->turn < ODD_STRANGER_STALK_GRAB_TURN_LIMIT) {
            if (!_oddStrangerOutOfRange(&chase->delta, ODD_STRANGER_STALK_GRAB_RADIUS) && work->grabCooldown == 0) {
                work->state = ODD_STRANGER_STATE_GRAB;
            }
        }
        if (work->exitCounter >= ODD_STRANGER_STALK_WATCH_TICKS) {
            work->state = ODD_STRANGER_STATE_WATCH;
        }
    } else {
        work->exitCounter = 0;
#if ODD_STRANGER_VARIANT == 2
        work->stateTimer++;
#endif
        headingRoot         = task->extra.tmd->coords;
        chase->turn         = _actorAngleNormalizeYaw(ratan2(chase->delta.vx, chase->delta.vz) - ratan2(-headingRoot->coord.m[2][0], headingRoot->coord.m[2][2]));
        work->lookYawTarget = chase->turn;
        if (work->sidestepSide == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->sidestepSide = -1;
            } else {
                work->sidestepSide = 1;
            }
        }
        if (work->sidestepSide == 1) {
            chase->turn += ODD_STRANGER_STALK_TURN_BIAS;
        } else {
            chase->turn -= ODD_STRANGER_STALK_TURN_BIAS;
        }
        if (work->stateTimer >= ODD_STRANGER_STALK_VEER_TICKS) {
            work->stateTimer   = 0;
            work->sidestepSide = -work->sidestepSide;
        }
    }
    ODD_STRANGER_TURN_STALK_ROOT(task, chase, ODD_STRANGER_STALK_TURN_LIMIT);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animId == ODD_STRANGER_ANIM_WALK) {
        if (work->blendActive == 0) {
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_STALK_STEP) != 0) {
                _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_STALK_STEP);
            }
        } else {
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_STALK_BLEND_STEP) != 0) {
                _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_STALK_BLEND_STEP);
            }
        }
    } else if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->animId      = ODD_STRANGER_ANIM_WALK;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_BLEND;
    }
    if (work->grabCooldown != 0) {
        work->grabCooldown--;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#undef ODD_STRANGER_TURN_STALK_ROOT
