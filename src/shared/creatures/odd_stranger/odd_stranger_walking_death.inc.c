/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Writable placement-vector storage for the walking burst-death effects.
///
/// `ODD_STRANGER_HAS_EFFECT_OFFSET` selects the live work member in actor_401000
/// or the caller's local SVECTOR lvalue in actor_401800. Arguments must be stable
/// and side-effect-free; only the selected argument is evaluated, once per
/// expansion. Captures no locals. XYZ are part-local game-coordinate offsets;
/// the fourth halfword remains untouched. Spawning copies XYZ and retains the
/// address, but these particle/debris callbacks never dereference that address.
/// The binding is undefined after the handler.
#if ODD_STRANGER_HAS_EFFECT_OFFSET
#define ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(actorWork, localOffset) ((actorWork)->effectOffset)
#else
#define ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(actorWork, localOffset) (localOffset)
#endif

/// Collapses parts 2..10 after animation has placed the burst's remaining body.
///
/// actorTask is a stable live TMD Task pointer with coordinates through index 10,
/// evaluated once per part in ascending order. Each call samples its part's yaw
/// before replacing pitch, roll and scale with a yaw matrix at Q12 factor 1
/// (1/4096); coefficient quantization can alter the resulting heading.
/// Translation, parent and stored Euler angles stay intact; composition becomes
/// dirty. Each call releases its matrix scratch and clobbers GTE state. Captures
/// no locals. Invoke as a standalone braced statement; undefined below.
#define ODD_STRANGER_COLLAPSE_BURST_PARTS(actorTask)                          \
    {                                                                         \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 2);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 3);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 4);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 5);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 6);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 7);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 8);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 9);  \
        _actorRenderCollapseYawRotation((actorTask)->extra.tmd->coords + 10); \
    }

/// Walks through three debris spawns, then burns, flattens, fades and hides the corpse.
///
/// Requires live task/work/Enemy and at least model coordinates and rig slots 0..10.
/// Entry makes the target unlockable and starts the walk clip. Debris at frames
/// 3/5/6 inherits placement texture offsets. After frame 16 a slot control jump
/// selects clip 26; its timer stays zero until slot 1 holds a settled pose.
/// The settled sequence releases rewards at 25, starts burning at 30, darkens at
/// 42, fades at 48 and hides at 64. From frame 26 root Y scale decays by 11 Q12
/// units per frame; parts 2..10 remain collapsed. Hidden work remains task-owned.
/// Requires loaded effect resources and initialized scratch storage; GTE state changes.
static void _oddStrangerWalkingDeath(Task* task)
{
    enum {
        ODD_STRANGER_WALK_DEATH_FINAL_CLIP          = 26,
        ODD_STRANGER_WALK_DEATH_TRANSITION_FRAME    = 16,
        ODD_STRANGER_WALK_DEATH_DEBRIS_TASK         = 5,
        ODD_STRANGER_WALK_DEATH_EFFECT_OFFSET       = 100,
        ODD_STRANGER_WALK_DEATH_PARTICLE_ARG        = 0x10300,
        ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE         = 0x200,
        ODD_STRANGER_WALK_DEATH_EVENT_BASE_FRAME    = 25,
        ODD_STRANGER_WALK_DEATH_FIRST_DEBRIS_FRAME  = 3,
        ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_FRAME = 5,
        ODD_STRANGER_WALK_DEATH_THIRD_DEBRIS_FRAME  = 6,
        ODD_STRANGER_WALK_DEATH_EVENT_RELEASE       = 0,
        ODD_STRANGER_WALK_DEATH_EVENT_BURN          = 5,
        ODD_STRANGER_WALK_DEATH_EVENT_DARKEN        = 17,
        ODD_STRANGER_WALK_DEATH_EVENT_FADE          = 23,
        ODD_STRANGER_WALK_DEATH_EVENT_HIDE          = 39,
        ODD_STRANGER_WALK_DEATH_SCALE_START_FRAME   = 26,
        ODD_STRANGER_WALK_DEATH_SCALE_BASE_FRAME    = 20,
        ODD_STRANGER_WALK_DEATH_SCALE_DECAY_Q12     = 11,
        ODD_STRANGER_WALK_DEATH_REWARD_ARG          = 10,
        ODD_STRANGER_WALK_DEATH_BURN_ARG            = 2
    };
#if !ODD_STRANGER_HAS_EFFECT_OFFSET
    SVECTOR effectOffset;
#endif
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              nextFrame;
    s16              currentFrame;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->hitBody.radius                                          = ODD_STRANGER_BODY_RADIUS;
        work->gridBody.flags                                         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                                 = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYawTarget                                           = 0;
        work->stateTimer                                              = 0;
        ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vx = ODD_STRANGER_WALK_DEATH_EFFECT_OFFSET;
        ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vz = 0;
        ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vy = 0;
        work->animId                                                  = ODD_STRANGER_ANIM_WALK;
        work->animRequest                                             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                                                = ANIMATION_RATE_ONE;
        effectSpawn(EFFECT_030, task->extra.tmd->coords + 1, ODD_STRANGER_WALK_DEATH_PARTICLE_ARG, &ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset));
        work->stateTimer = 0;
    }
    nextFrame        = work->stateTimer + 1;
    work->stateTimer = nextFrame;
    switch (work->animId) {
        case ODD_STRANGER_ANIM_WALK:
            if ((s16)nextFrame >= ODD_STRANGER_WALK_DEATH_TRANSITION_FRAME && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
                work->animId      = ODD_STRANGER_WALK_DEATH_FINAL_CLIP;
                work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
                work->animRate    = ANIMATION_RATE_ONE;
                work->blendActive = 0;
            }
            if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, ODD_STRANGER_WALK_STEP) != 0) {
                _actorMovementStepForward(task->extra.tmd->coords, ODD_STRANGER_WALK_STEP);
            }
            _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
            if ((s16)work->stateTimer == ODD_STRANGER_WALK_DEATH_FIRST_DEBRIS_FRAME) {
                gEffectBurstTaskDescriptors[ODD_STRANGER_WALK_DEATH_DEBRIS_TASK].data.model = &ODD_STRANGER_BURST_PART_9_MODEL;
                ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vz               = ODD_STRANGER_WALK_DEATH_EFFECT_OFFSET;
                ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vy               = 0;
                ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset).vx               = 0;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 9, ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE, &ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE(work, effectOffset)), enemy);
            }
            if ((s16)work->stateTimer == ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_FRAME) {
                gEffectBurstTaskDescriptors[ODD_STRANGER_WALK_DEATH_DEBRIS_TASK].data.model = ODD_STRANGER_WALK_DEATH_SECOND_DEBRIS_MODEL;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 1, ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE, NULL), enemy);
            }
            if ((s16)work->stateTimer == ODD_STRANGER_WALK_DEATH_THIRD_DEBRIS_FRAME) {
                gEffectBurstTaskDescriptors[ODD_STRANGER_WALK_DEATH_DEBRIS_TASK].data.model = &ODD_STRANGER_BURST_PART_3_MODEL;
                _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 3, ODD_STRANGER_WALK_DEATH_DEBRIS_SIZE, NULL), enemy);
            }
            break;
        case ODD_STRANGER_WALK_DEATH_FINAL_CLIP:
            if (!(work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                work->stateTimer = 0;
            }
            // These events begin only after the final clip has settled.
            switch ((s16)(work->stateTimer - ODD_STRANGER_WALK_DEATH_EVENT_BASE_FRAME)) {
                case ODD_STRANGER_WALK_DEATH_EVENT_RELEASE:
                    sceneReleaseBattleRefWithRewards(task, ODD_STRANGER_WALK_DEATH_REWARD_ARG);
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_BURN:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    effectSpawn(EFFECT_CORPSE_BURN, task->extra.tmd->coords + 2, ODD_STRANGER_WALK_DEATH_BURN_ARG, NULL);
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_FADE:
                    task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_DARKEN:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case ODD_STRANGER_WALK_DEATH_EVENT_HIDE:
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->state            = ODD_STRANGER_STATE_HIDDEN;
                    break;
            }
            currentFrame = work->stateTimer;
            if (currentFrame >= ODD_STRANGER_WALK_DEATH_SCALE_START_FRAME) {
                _actorRenderRescaleYawY(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE, ODD_STRANGER_ROOT_SCALE - (currentFrame - ODD_STRANGER_WALK_DEATH_SCALE_BASE_FRAME) * ODD_STRANGER_WALK_DEATH_SCALE_DECAY_Q12);
            }
            break;
    }
    _oddStrangerDriveAnimation(task);
    ODD_STRANGER_COLLAPSE_BURST_PARTS(task);
}

#undef ODD_STRANGER_WALK_DEATH_OFFSET_STORAGE

#undef ODD_STRANGER_COLLAPSE_BURST_PARTS

/// Limits the stalking turn, replaces root yaw and restores normal model scale.
///
/// Requires a live TMD `Task*`, reserved `ActorChaseScratch*` and a nonnegative
/// signed-int limit that fits s16, in 4096ths of a turn. Arguments are evaluated
/// repeatedly and must be stable, side-effect-free and independent of the
/// block-local `headingRoot`. The ordered clamps overwrite `turn`; adding the
/// matrix heading then narrows it to s16 without wrapping to a half turn.
/// Replaces pitch, roll and prior scale with yaw at `ODD_STRANGER_ROOT_SCALE`;
/// translation stays intact and composition becomes dirty. The callees release
/// their matrix scratch and clobber GTE state; chase scratch stays reserved.
/// Invoke as a standalone braced statement.
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
static inline void _oddStrangerReadStalkPlayerBearings(const Task* task, ActorChaseScratch* chase)
{
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
    s16 reverseBearing;
#endif

    chase->playerYaw = ratan2(-(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0],
                              (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    _actorPositionDeltaToPlayer(&gPlayerStatus, task->extra.tmd->coords, &chase->delta);
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
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
/// reaches 241 (actor_401800 increments that counter twice per blocked tick).
/// Root turning is limited to 32 angle units per tick, with 4096 per turn.
/// Movement uses the build's step, or five units during blending; the grab
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
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
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
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401800
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
