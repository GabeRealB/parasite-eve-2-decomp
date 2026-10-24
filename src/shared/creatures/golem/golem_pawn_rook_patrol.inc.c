/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Walks a placement-sized patrol leg, listens at its end, and turns back.
///
/// `actor` is a live GOLEM body task. The placement variant sets each leg's
/// distance in multiples of 1000 game units; movement begins after the animation
/// blend. Player noise or another GOLEM's death interrupts the patrol and joins
/// battle. Angles use 4096 units per turn.
static void _golemPawnRookPatrolState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_TURN_AROUND     = 3,
        GOLEM_PAWN_ROOK_PATROL_WALK          = 0,
        GOLEM_PAWN_ROOK_PATROL_LISTEN        = 1,
        GOLEM_PAWN_ROOK_PATROL_TURN          = 2,
        GOLEM_PAWN_ROOK_PATROL_SPEED         = 20,
        GOLEM_PAWN_ROOK_PATROL_LISTEN_FRAMES = 96,
        GOLEM_PAWN_ROOK_PATROL_TURN_FRAMES   = 35,
        GOLEM_PAWN_ROOK_PATROL_TURN_RATE     = 59,
        GOLEM_PAWN_ROOK_PATROL_SCRATCH_BYTES = 16,
        GOLEM_PAWN_ROOK_PATROL_DISTANCE_UNIT = 1000,
    };

    Enemy*             enemy;
    GolemPawnRookWork* work;
    GfxCoord*          root;
    s16                step;
    s16                speed;
    s32                yaw;
    s32                patrolLengthMultiplier;

    // This reservation has no accessed fields; its original role is unproven.
    SCRATCH_STACK_RESERVE_BYTES(GOLEM_PAWN_ROOK_PATROL_SCRATCH_BYTES);

    root  = actor->extra.tmd->coords;
    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    step  = work->step;

    switch (step) {
        case GOLEM_PAWN_ROOK_PATROL_WALK:
            speed = 0;
            if (work->animFrame >= _gGolemPawnRookAnimBlendFrames[work->anim]) {
                speed = GOLEM_PAWN_ROOK_PATROL_SPEED;
            }
            work->forwardSpeed        = speed;
            work->turnRate            = 0;
            work->patrolDistanceLeft -= work->forwardSpeed;
            if (work->patrolDistanceLeft <= 0) {
                work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                work->timer        = 0;
                work->step         = GOLEM_PAWN_ROOK_PATROL_LISTEN;
                work->forwardSpeed = 0;
            }
            _golemPawnRookCheckPlayerNoise(actor);
            break;
        case GOLEM_PAWN_ROOK_PATROL_LISTEN:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->animFrame >= GOLEM_PAWN_ROOK_PATROL_LISTEN_FRAMES) {
                if (work->patrolDistanceLeft <= 0) {
                    patrolLengthMultiplier   = enemy->place->variant;
                    work->anim               = GOLEM_PAWN_ROOK_ANIM_TURN_AROUND;
                    work->step               = GOLEM_PAWN_ROOK_PATROL_TURN;
                    work->patrolDistanceLeft = patrolLengthMultiplier * GOLEM_PAWN_ROOK_PATROL_DISTANCE_UNIT;
                    yaw                      = ratan2(root->coord.m[0][2], root->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
                    work->yaw                = yaw;
                    work->targetYaw          = (yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                } else {
                    work->anim = GOLEM_PAWN_ROOK_ANIM_WALK;
                    work->step = GOLEM_PAWN_ROOK_PATROL_WALK;
                }
            }
            break;
        case GOLEM_PAWN_ROOK_PATROL_TURN:
            work->forwardSpeed = 0;
            work->turnRate     = GOLEM_PAWN_ROOK_PATROL_TURN_RATE;
            if (work->animFrame >= GOLEM_PAWN_ROOK_PATROL_TURN_FRAMES) {
                work->anim = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step = GOLEM_PAWN_ROOK_PATROL_WALK;
            }
            break;
    }

    if ((work->playerSpotted != 0) || (gSceneCombatState.golemPawnRookDeathAlert != 0)) {
        work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
        work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
        work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
        work->timer    = 0;
        sceneEngageBattle(1);
    }

    SCRATCH_STACK_RELEASE_BYTES(GOLEM_PAWN_ROOK_PATROL_SCRATCH_BYTES);
}

/// Reacts to the player's action noise while the GOLEM is idle or patrolling.
///
/// `actor` is a live GOLEM body task. Within 1500 horizontal game units any
/// noisy action spots the player. Farther away, loud actions always start a
/// listening animation; footsteps and active PE do so only within 3000 units.
/// This check does not test walls or change `playerSpotted` back to zero.
static void _golemPawnRookCheckPlayerNoise(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_NOISE_SPOT_RANGE         = 1500,
        GOLEM_PAWN_ROOK_NOISE_LISTEN_STEP        = 1,
        GOLEM_PAWN_ROOK_QUIET_NOISE_LISTEN_RANGE = 3000,
    };

    GolemPawnRookWork* work;
    GfxCoord*          root;
    s32                playerDx;
    s32                playerDistance;
    s32                playerDz;
    s32                heardDistantAction;
    VECTOR*            toPlayer;

    root                         = actor->extra.tmd->coords;
    work                         = actor->work;
    toPlayer                     = SCRATCH_STACK_CURSOR(VECTOR) - 1;
    toPlayer->vx                 = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
    toPlayer->vy                 = 0;
    playerDz                     = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
    toPlayer->vz                 = playerDz;
    playerDx                     = toPlayer->vx;
    heardDistantAction           = 0;
    SCRATCH_STACK_CURSOR(VECTOR) = toPlayer;
    playerDistance               = SquareRoot0((playerDx * playerDx) + (playerDz * playerDz));
    if (playerDistance < GOLEM_PAWN_ROOK_NOISE_SPOT_RANGE) {
        if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_PE_CAST_OTHER | SCENE_COMBAT_ACTION_FOOTSTEP)) {
            work->playerSpotted = 1;
        }
    } else {
        if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
            heardDistantAction = 1;
        }
        if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_FOOTSTEP)) && (playerDistance < GOLEM_PAWN_ROOK_QUIET_NOISE_LISTEN_RANGE)) {
            heardDistantAction = 1;
        }
        if (heardDistantAction != 0) {
            work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            work->timer        = 0;
            work->step         = GOLEM_PAWN_ROOK_NOISE_LISTEN_STEP;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Plays a nonfatal fall and keeps the GOLEM lying in its saved downed pose.
///
/// The last hit's side chooses the fall clip and enlarged torso collision sphere.
/// After the fall, live enemies alternate random rests and ten-frame pose shifts;
/// an enemy whose HP ran out during the fall enters task teardown instead.
/// The actor must have live GOLEM work, model coordinates and its spawn Enemy.
/// Counters measure frame-handler calls and randomized rests span 0..63 frames.
static void _golemPawnRookKnockdownState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_KNOCKDOWN_BEGIN                 = 0,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_BEHIND           = 1,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_FRONT            = 2,
        GOLEM_PAWN_ROOK_KNOCKDOWN_REST                  = 3,
        GOLEM_PAWN_ROOK_KNOCKDOWN_SHIFT                 = 4,
        GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_FALL_BEHIND      = 0x16,
        GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_FALL_FRONT       = 0x1A,
        GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_REST_BEHIND      = 0x19,
        GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_REST_FRONT       = 0x1D,
        GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_SHIFT_BEHIND     = 0x18,
        GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_SHIFT_FRONT      = 0x1C,
        GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_OFFSET         = 167,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FRONT_OFFSET          = 265,
        GOLEM_PAWN_ROOK_KNOCKDOWN_BODY_RADIUS           = 350,
        GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_FALL_CUE_FRAME = 20,
        GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_IMPACT_FRAME   = 44,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FRONT_IMPACT_FRAME    = 25,
        GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_FRAMES         = 66,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FRONT_FRAMES          = 49,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_CUE_BASE         = 12,
        GOLEM_PAWN_ROOK_KNOCKDOWN_IMPACT_CUE_BASE       = 8,
        GOLEM_PAWN_ROOK_KNOCKDOWN_START                 = 1,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FALLEN                = 2,
        GOLEM_PAWN_ROOK_KNOCKDOWN_SHIFT_FRAMES          = 10U,
        GOLEM_PAWN_ROOK_KNOCKDOWN_REST_FRAME_MASK       = 63,
    };
    s16                step;
    s16                shiftAnimation;
    s16                restAnimation;
    s32                soundId;
    s32                shiftEndRandom;
    s32                behindFallPan;
    s32                behindImpactPan;
    s32                frontImpactPan;
    u16                restTimer;
    u16                shiftTimer;
    u32                behindFallRandom;
    u32                frontFallRandom;
    GolemPawnRookWork* work;
    GfxCoord*          root;

    /// Requests a fall or impact sound script at the composed body origin.
    ///
    /// actor borrows a live `Enemy` at spawnArg2.pointer; root is composed into the
    /// active view. Captures the carrier's `_gGolemPawnRookMotionCues[17]`, whose
    /// script IDs have zero instance bytes. cueIndex is evaluated once and must
    /// be in 0..16; fall/impact calls use sound sets 0..4 at bases 12/8, including
    /// set 0. The placement index (0..15) is inserted at bits 8..11. soundId and
    /// audioPan are writable s32 lvalues; pan is sampled before depth and both
    /// narrow to signed bytes. The pan query changes GTE state and borrows scratch.
    /// Arguments must be side-effect-free; root and outputs occur repeatedly.
    /// Use the compound statement only as a standalone invocation; undefined below.
#define GOLEM_PAWN_ROOK_PLAY_KNOCKDOWN_CUE(actor, root, cueIndex, soundId, audioPan)                                                                                                \
    {                                                                                                                                                                               \
        (soundId)  = _gGolemPawnRookMotionCues[(cueIndex)] | ((((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT); \
        (audioPan) = (s8)worldCoordGetOriginAudioPan((root));                                                                                                                       \
        sndEvtRequestScriptStart((soundId), (audioPan), (s8)worldCoordGetOriginAudioDepth((root)));                                                                                 \
    }

    work = actor->work;
    root = actor->extra.tmd->coords;
    step = work->step;
    switch (step) {
        // Transfer room-grid collision to the fallen torso and record the hit side.
        case GOLEM_PAWN_ROOK_KNOCKDOWN_BEGIN:
            if (work->hitFromFront == 0) {
                work->anim            = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_FALL_BEHIND;
                work->step            = GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_BEHIND;
                work->downedPose      = GOLEM_PAWN_ROOK_DOWNED_BEHIND;
                work->hurtBody.pos.vz = -GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_OFFSET;
            } else {
                work->anim            = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_FALL_FRONT;
                work->step            = GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_FRONT;
                work->downedPose      = GOLEM_PAWN_ROOK_DOWNED_FRONT;
                work->hurtBody.pos.vz = GOLEM_PAWN_ROOK_KNOCKDOWN_FRONT_OFFSET;
            }
            work->hurtBody.radius                             = GOLEM_PAWN_ROOK_KNOCKDOWN_BODY_RADIUS;
            work->forwardSpeed                                = 0;
            work->turnRate                                    = 0;
            work->knockdownStage                              = GOLEM_PAWN_ROOK_KNOCKDOWN_START;
            work->hurtBody.flags                              = (u16)(work->hurtBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->groundBody.flags                            = (u16)(work->groundBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
            ((Enemy*)actor->spawnArg2.pointer)->reactionFlags = 0;
            work->fallingDown                                 = 1;
            break;
        case GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_BEHIND:
            if (work->animFrame == GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_FALL_CUE_FRAME) {
                GOLEM_PAWN_ROOK_PLAY_KNOCKDOWN_CUE(actor, root, work->soundSet + GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_CUE_BASE, soundId, behindFallPan);
            }
            if (work->animFrame == GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_IMPACT_FRAME) {
                GOLEM_PAWN_ROOK_PLAY_KNOCKDOWN_CUE(actor, root, work->soundSet + GOLEM_PAWN_ROOK_KNOCKDOWN_IMPACT_CUE_BASE, soundId, behindImpactPan);
            }
            if (work->animFrame >= GOLEM_PAWN_ROOK_KNOCKDOWN_BEHIND_FRAMES) {
                work->anim        = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_REST_BEHIND;
                work->fallingDown = 0;
                behindFallRandom  = RANDOM_LCG_NEXT(gRandomLcgState);
                work->timer       = (u16)((behindFallRandom >> 0x10) & GOLEM_PAWN_ROOK_KNOCKDOWN_REST_FRAME_MASK);
                gRandomLcgState   = behindFallRandom;
                if (((Enemy*)actor->spawnArg2.pointer)->hp > 0) {
                    work->step = GOLEM_PAWN_ROOK_KNOCKDOWN_REST;
                } else {
                    actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
                    work->step   = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                }
            }
            if (work->knockdownStage == GOLEM_PAWN_ROOK_KNOCKDOWN_START) {
                work->knockdownStage = GOLEM_PAWN_ROOK_KNOCKDOWN_FALLEN;
                break;
            }
            break;
        case GOLEM_PAWN_ROOK_KNOCKDOWN_FALL_FRONT:
            if (work->animFrame == GOLEM_PAWN_ROOK_KNOCKDOWN_FRONT_IMPACT_FRAME) {
                GOLEM_PAWN_ROOK_PLAY_KNOCKDOWN_CUE(actor, root, work->soundSet + GOLEM_PAWN_ROOK_KNOCKDOWN_IMPACT_CUE_BASE, soundId, frontImpactPan);
            }
            if (work->animFrame >= GOLEM_PAWN_ROOK_KNOCKDOWN_FRONT_FRAMES) {
                work->anim        = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_REST_FRONT;
                work->fallingDown = 0;
                frontFallRandom   = RANDOM_LCG_NEXT(gRandomLcgState);
                work->timer       = (u16)((frontFallRandom >> 0x10) & GOLEM_PAWN_ROOK_KNOCKDOWN_REST_FRAME_MASK);
                gRandomLcgState   = frontFallRandom;
                if (((Enemy*)actor->spawnArg2.pointer)->hp > 0) {
                    work->step = GOLEM_PAWN_ROOK_KNOCKDOWN_REST;
                } else {
                    actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
                    work->step   = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                }
            }
            if (work->knockdownStage == GOLEM_PAWN_ROOK_KNOCKDOWN_START) {
                work->knockdownStage = GOLEM_PAWN_ROOK_KNOCKDOWN_FALLEN;
            }
            break;
        // Alternate a random rest of 0..63 frames with a ten-frame pose shift.
        case GOLEM_PAWN_ROOK_KNOCKDOWN_REST:
            restTimer   = work->timer - 1;
            work->timer = restTimer;
            if ((s16)restTimer <= 0) {
                shiftAnimation = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_SHIFT_FRONT;
                if (work->downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                    shiftAnimation = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_SHIFT_BEHIND;
                }
                work->timer = GOLEM_PAWN_ROOK_KNOCKDOWN_SHIFT_FRAMES;
                work->anim  = shiftAnimation;
                work->step  = GOLEM_PAWN_ROOK_KNOCKDOWN_SHIFT;
                break;
            }
            break;
        case GOLEM_PAWN_ROOK_KNOCKDOWN_SHIFT:
            shiftTimer  = work->timer - 1;
            work->timer = shiftTimer;
            if ((s16)shiftTimer <= 0) {
                restAnimation = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_REST_FRONT;
                if (work->downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                    restAnimation = GOLEM_PAWN_ROOK_KNOCKDOWN_ANIM_REST_BEHIND;
                }
                work->anim      = restAnimation;
                work->step      = GOLEM_PAWN_ROOK_KNOCKDOWN_REST;
                shiftEndRandom  = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState = shiftEndRandom;
                work->timer     = (u16)(((u32)shiftEndRandom >> 0x10) & GOLEM_PAWN_ROOK_KNOCKDOWN_REST_FRAME_MASK);
            }
            break;
    }

#undef GOLEM_PAWN_ROOK_PLAY_KNOCKDOWN_CUE
}

#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Flinches from a hit while downed and resumes the matching lying pose.
///
/// `actor` is a live GOLEM body task. The downed pose selects the reaction clip;
/// its completion returns to the knockdown behaviour's rest step with a fresh
/// random dwell of 0..63 frames.
static void _golemPawnRookDownedHitState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_KNOCKDOWN_REST_STEP = 3,
        GOLEM_PAWN_ROOK_ANIM_LIE_FRONT      = 0x1D,
        GOLEM_PAWN_ROOK_ANIM_LIE_BEHIND     = 0x19,
        GOLEM_PAWN_ROOK_DOWNED_HIT_START    = 0,
        GOLEM_PAWN_ROOK_DOWNED_HIT_BEHIND   = 1,
        GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT    = 2,
        GOLEM_PAWN_ROOK_DOWNED_DWELL_MASK   = 63,
    };

    GolemPawnRookWork* work;
    s16                step;
    s32                downedPose;

    work = actor->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_DOWNED_HIT_START:
            downedPose = work->downedPose;
            if (downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                work->anim = GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_BEHIND;
                work->step = downedPose;
            } else {
                work->anim = GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_FRONT;
                work->step = GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT;
            }
            break;
        case GOLEM_PAWN_ROOK_DOWNED_HIT_BEHIND:
            if (work->animFrame >= GOLEM_PAWN_ROOK_DOWNED_HIT_BEHIND_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_LIE_BEHIND;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                work->step     = GOLEM_PAWN_ROOK_KNOCKDOWN_REST_STEP;
                RANDOM_LCG_ADVANCE();
                work->timer = RANDOM_LCG_DRAW() & GOLEM_PAWN_ROOK_DOWNED_DWELL_MASK;
            }
            break;
        case GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_LIE_FRONT;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                work->step     = GOLEM_PAWN_ROOK_KNOCKDOWN_REST_STEP;
                RANDOM_LCG_ADVANCE();
                work->timer = RANDOM_LCG_DRAW() & GOLEM_PAWN_ROOK_DOWNED_DWELL_MASK;
            }
            break;
    }
}

/// Plays the fatal fall from standing and hands the body task to its dead state.
///
/// `actor` is a live GOLEM body task whose HP has reached zero. The last hit's
/// side chooses the fall and saved downed pose. Grid collision moves from the
/// standing ground body to the enlarged hurt body until the fall timer expires.
static void _golemPawnRookCollapseState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_FALL_FRONT                  = 0x1A,
        GOLEM_PAWN_ROOK_ANIM_FALL_BEHIND                 = 0x16,
        GOLEM_PAWN_ROOK_COLLAPSE_START                   = 0,
        GOLEM_PAWN_ROOK_COLLAPSE_FALL                    = 1,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FRAMES           = 66,
        GOLEM_PAWN_ROOK_COLLAPSE_FRONT_FRAMES            = 49,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_OFFSET           = 167,
        GOLEM_PAWN_ROOK_COLLAPSE_FRONT_OFFSET            = 265,
        GOLEM_PAWN_ROOK_COLLAPSE_BODY_RADIUS             = 350,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FIRST_CUE_FRAME  = 20,
        GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_SECOND_CUE_FRAME = 44,
        GOLEM_PAWN_ROOK_COLLAPSE_FRONT_CUE_FRAME         = 25,
        GOLEM_PAWN_ROOK_COLLAPSE_FIRST_CUE_BASE          = 12,
        GOLEM_PAWN_ROOK_COLLAPSE_IMPACT_CUE_BASE         = 8,
        GOLEM_PAWN_ROOK_KNOCKDOWN_START                  = 1,
        GOLEM_PAWN_ROOK_KNOCKDOWN_FALLEN                 = 2,
    };

    GolemPawnRookWork* work;
    GfxCoord*          root;
    s32                soundId;
    s16                step;

    work = actor->work;
    root = actor->extra.tmd->coords;
    step = work->step;

    switch (step) {
        case GOLEM_PAWN_ROOK_COLLAPSE_START:
            if (work->hitFromFront == 0) {
                work->anim            = GOLEM_PAWN_ROOK_ANIM_FALL_BEHIND;
                work->step            = GOLEM_PAWN_ROOK_COLLAPSE_FALL;
                work->downedPose      = GOLEM_PAWN_ROOK_DOWNED_BEHIND;
                work->timer           = GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FRAMES;
                work->hurtBody.pos.vz = -GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_OFFSET;
            } else {
                work->anim            = GOLEM_PAWN_ROOK_ANIM_FALL_FRONT;
                work->step            = GOLEM_PAWN_ROOK_COLLAPSE_FALL;
                work->downedPose      = GOLEM_PAWN_ROOK_DOWNED_FRONT;
                work->timer           = GOLEM_PAWN_ROOK_COLLAPSE_FRONT_FRAMES;
                work->hurtBody.pos.vz = GOLEM_PAWN_ROOK_COLLAPSE_FRONT_OFFSET;
            }
            // The fallen hurt body now resolves the room grid in place of the foot sphere.
            work->hurtBody.radius                             = GOLEM_PAWN_ROOK_COLLAPSE_BODY_RADIUS;
            work->forwardSpeed                                = 0;
            work->turnRate                                    = 0;
            work->knockdownStage                              = GOLEM_PAWN_ROOK_KNOCKDOWN_START;
            work->hurtBody.flags                             |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags                           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            ((Enemy*)actor->spawnArg2.pointer)->reactionFlags = 0;
            work->fallingDown                                 = 1;
            break;
        case GOLEM_PAWN_ROOK_COLLAPSE_FALL:
            if (work->knockdownStage == GOLEM_PAWN_ROOK_KNOCKDOWN_START) {
                work->knockdownStage = GOLEM_PAWN_ROOK_KNOCKDOWN_FALLEN;
            }
            if (work->downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                if (work->animFrame == GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_FIRST_CUE_FRAME) {
                    s32 audioPan;

                    soundId = _gGolemPawnRookMotionCues[work->soundSet + GOLEM_PAWN_ROOK_COLLAPSE_FIRST_CUE_BASE] |
                              ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                    audioPan = (s8)worldCoordGetOriginAudioPan(root);

                    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                }
                if (work->animFrame == GOLEM_PAWN_ROOK_COLLAPSE_BEHIND_SECOND_CUE_FRAME) {
                    s32 audioPan;

                    soundId = _gGolemPawnRookMotionCues[work->soundSet + GOLEM_PAWN_ROOK_COLLAPSE_IMPACT_CUE_BASE] |
                              ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                    audioPan = (s8)worldCoordGetOriginAudioPan(root);

                    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                }
            } else if (work->animFrame == GOLEM_PAWN_ROOK_COLLAPSE_FRONT_CUE_FRAME) {
                s32 audioPan;

                soundId = _gGolemPawnRookMotionCues[work->soundSet + GOLEM_PAWN_ROOK_COLLAPSE_IMPACT_CUE_BASE] |
                          ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
                audioPan = (s8)worldCoordGetOriginAudioPan(root);

                sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            work->timer--;
            if (work->timer <= 0) {
                actor->state      = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
                work->step        = GOLEM_PAWN_ROOK_COLLAPSE_START;
                work->fallingDown = 0;
            }
            break;
    }
}

/// Rebuilds the body root's yaw while turning toward its requested heading.
///
/// Headings use 4096 units per turn; targetYaw is 0..4095 and turnRate is a
/// nonnegative angular step per call. The shorter arc crosses zero when needed;
/// the turn-about animation always steps in the negative direction unless already
/// close enough to snap. Replaces pitch, roll and scale but preserves translation.
/// The stored yaw can cross outside 0..4095 until the next call remeasures it.
/// The enclosing frame handler owns the root's composition-dirty mark.
static void _golemPawnRookTurnTowardTarget(Task* actor)
{
    enum { GOLEM_PAWN_ROOK_TURN_ANIM_ABOUT = 3 };
    GolemPawnRookWork* work;
    GfxCoord*          root;
    SVECTOR*           rotation;
    s32                currentYaw;
    u16                targetYaw;
    s16                yawDifference;
    s32                absoluteDifference;
    s32                turnRate;
    s32                unsignedTurnRate;
    s32                wrapTurnRate;
    s32                previousYaw;
    s32                nextYaw;
    s32                wrappedTurnRate;

    rotation           = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    root               = actor->extra.tmd->coords;
    work               = actor->work;
    currentYaw         = ratan2(root->coord.m[0][2], root->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw          = work->targetYaw;
    yawDifference      = targetYaw - currentYaw;
    absoluteDifference = yawDifference >= 0 ? yawDifference : -yawDifference;

    work->yaw = currentYaw;
    if (absoluteDifference < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate         = work->turnRate;
        unsignedTurnRate = (u16)work->turnRate;
        if (turnRate >= absoluteDifference) {
            work->yaw = targetYaw;
        } else {
            if (work->anim == GOLEM_PAWN_ROOK_TURN_ANIM_ABOUT) {
                nextYaw = currentYaw - unsignedTurnRate;
            } else {
                nextYaw = work->yaw;
                if (yawDifference <= 0) {
                    nextYaw -= turnRate;
                } else {
                    nextYaw += turnRate;
                }
            }
            work->yaw = nextYaw;
        }
    } else {
        wrapTurnRate = work->turnRate;
        if (yawDifference > 0 ? wrapTurnRate >= ACTOR_TRANSFORM_ANGLE_TURN - yawDifference : wrapTurnRate >= ACTOR_TRANSFORM_ANGLE_TURN + yawDifference) {
            work->yaw = work->targetYaw;
        } else if (work->anim == GOLEM_PAWN_ROOK_TURN_ANIM_ABOUT) {
            work->yaw = (u16)work->yaw - (u16)work->turnRate;
        } else {
            wrappedTurnRate = work->turnRate;
            previousYaw     = work->yaw;
            if (yawDifference > 0) {
                work->yaw = previousYaw - wrappedTurnRate;
            } else {
                work->yaw = previousYaw + wrappedTurnRate;
            }
        }
    }
    rotation->vx = 0;
    rotation->vy = work->yaw;
    rotation->vz = 0;
    RotMatrix(rotation, &root->coord);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Applies the current hit flinch to body part 3 and eases its pitch and yaw to zero.
///
/// Angles use 4096 units per turn and decay by 32 per call, snapping inside that
/// distance. Run after animation has rebuilt the part's pose; the enclosing frame
/// handler marks its composition dirty. Clears hitTiltActive once both axes settle.
/// The caller supplies live body work; the temporary matrix is released on return.
static void _golemPawnRookDecayHitTilt(Task* actor)
{
    enum { GOLEM_PAWN_ROOK_HIT_TILT_DECAY = 32 };
    GolemPawnRookWork* work;
    GfxCoord*          bodyCoords;
    MATRIX*            tiltMatrix;
    s32                pitch;
    s32                yaw;
    s32                absolutePitch;
    s32                nextPitch;
    s32                absoluteYaw;
    s32                nextYaw;
    s32                tiltRemaining;

    /// Steps one signed flinch angle toward zero and records any remaining tilt.
    ///
    /// angleField is a writable s16 angle, in 4096 units per turn. value,
    /// magnitude, next and remaining are distinct s32 lvalues. decay is a
    /// positive signed-word angular step whose addition of 1 must not overflow;
    /// callers use 32. Magnitudes <= decay snap to zero; otherwise the updated
    /// angle retains its sign and remaining is set to 1. remaining is never
    /// cleared; callers initialize it to 0 before processing both axes.
    /// magnitude/next are written only on their respective nonzero/decay branches.
    /// Arguments must be side-effect-free and can be evaluated repeatedly.
    /// Captures no locals; use the compound statement only as a standalone
    /// invocation. It is undefined below.
#define GOLEM_PAWN_ROOK_DECAY_TILT_AXIS(angleField, value, magnitude, next, remaining, decay) \
    {                                                                                         \
        (value) = (angleField);                                                               \
        if ((value) != 0) {                                                                   \
            (magnitude) = __builtin_abs((value));                                             \
            if ((magnitude) < (decay) + 1) {                                                  \
                (angleField) = 0;                                                             \
            } else {                                                                          \
                (next) = (value) - (decay);                                                   \
                if ((value) <= 0) {                                                           \
                    (next) = (value) + (decay);                                               \
                }                                                                             \
                (angleField) = (next);                                                        \
                (remaining)  = 1;                                                             \
            }                                                                                 \
        }                                                                                     \
    }

    tiltMatrix    = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    tiltRemaining = 0;
    work          = actor->work;
    bodyCoords    = actor->extra.tmd->coords;
    RotMatrix(&work->hitTilt, tiltMatrix);
    gte_MulMatrix0(&bodyCoords[3].coord, tiltMatrix, &bodyCoords[3].coord);
    GOLEM_PAWN_ROOK_DECAY_TILT_AXIS(work->hitTilt.vx, pitch, absolutePitch, nextPitch, tiltRemaining, GOLEM_PAWN_ROOK_HIT_TILT_DECAY);
    GOLEM_PAWN_ROOK_DECAY_TILT_AXIS(work->hitTilt.vy, yaw, absoluteYaw, nextYaw, tiltRemaining, GOLEM_PAWN_ROOK_HIT_TILT_DECAY);
    if (tiltRemaining == 0) {
        work->hitTiltActive = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);

#undef GOLEM_PAWN_ROOK_DECAY_TILT_AXIS
}

/// Plays the body animation's two sound cues on their falling edges.
///
/// soundSet 0 disables cues; values 1..4 choose the carrier's footstep cue pair.
/// Slot 1 supplies cue flags, remembered only when a current record exists.
/// Sound requests carry the enemy's placement instance and the root's spatial pan
/// and depth; the task, rig and spawn Enemy remain borrowed.
static void _golemPawnRookPlayAnimCues(Task* actor)
{
    s32                    soundId;
    s32                    cue2Pan;
    s32                    cue1Pan;
    GolemPawnRookWork*     work;
    GfxCoord*              root;
    const AnimationRecord* record;

    /// Requests an animation sound script at the composed body origin.
    ///
    /// actor borrows a live `Enemy` at spawnArg2.pointer; root is composed into the
    /// active view. Captures the carrier's `_gGolemPawnRookMotionCues[17]`, whose
    /// script IDs have zero instance bytes. cueIndex is evaluated once and must
    /// be in 0..16; animation callers use 1..8 from sound sets 1..4. The placement
    /// index (0..15) is inserted at bits 8..11. soundId/audioPan are writable s32
    /// lvalues; pan is sampled before depth and both narrow to signed bytes. The
    /// pan query changes GTE state and borrows projection scratch. Arguments must
    /// be side-effect-free; root and outputs occur repeatedly. Use the compound
    /// statement only as a standalone invocation; it is undefined below.
#define GOLEM_PAWN_ROOK_PLAY_ANIM_CUE(actor, root, cueIndex, soundId, audioPan)                                                                                                     \
    {                                                                                                                                                                               \
        (soundId)  = _gGolemPawnRookMotionCues[(cueIndex)] | ((((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT); \
        (audioPan) = (s8)worldCoordGetOriginAudioPan((root));                                                                                                                       \
        sndEvtRequestScriptStart((soundId), (audioPan), (s8)worldCoordGetOriginAudioDepth((root)));                                                                                 \
    }

    work = actor->work;
    root = actor->extra.tmd->coords;
    if (work->soundSet != 0) {
        record = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
        if (record != NULL) {
            if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->prevCueFlags & ANIMATION_RECORD_CUE_2)) {
                GOLEM_PAWN_ROOK_PLAY_ANIM_CUE(actor, root, work->soundSet * 2 - 1, soundId, cue2Pan);
            }
            if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->prevCueFlags & ANIMATION_RECORD_CUE_1)) {
                GOLEM_PAWN_ROOK_PLAY_ANIM_CUE(actor, root, work->soundSet * 2, soundId, cue1Pan);
            }
            work->prevCueFlags = (u16)(record->flags & ANIMATION_RECORD_CUE_MASK);
        }
    }

#undef GOLEM_PAWN_ROOK_PLAY_ANIM_CUE
}

#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Restarts a changed corpse clip with blending, or advances its existing pose.
///
/// `actor` owns an initialized nineteen-slot body rig and `coord` is its model
/// root. The requested clip indexes the carrier's 32-entry blend-frame table
/// and must be loaded in the animation bank. A changed request resets the
/// halfword frame counter and seeks slots 1..18 to frame 0 with that whole-frame
/// blend duration; an unchanged request increments the counter and ticks them.
/// Slot 0 is untouched. Marks the root dirty after either path, without composing
/// it. Work, clip and model storage remain borrowed for their task lifetime;
/// calls require initialized animation/scratch/GTE state and retain no new pointer.
static inline void _golemPawnRookAdvanceCorpsePose(Task* actor, GfxCoord* coord)
{
    GolemPawnRookWork* animWork;
    s16                blendFrames;
    s32                slotIndex;

    animWork = actor->work;
    if (animWork->anim != animWork->playingAnim) {
        animWork->playingAnim = animWork->anim;
        animWork->animFrame   = 0U;
        blendFrames           = _gGolemPawnRookAnimBlendFrames[animWork->anim];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(animWork->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&animWork->rig.anim, slotIndex, animWork->anim, 0, blendFrames);
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        animWork->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(animWork->rig.slots); slotIndex++) {
            animationTickSlot(&animWork->rig.anim, slotIndex);
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Saves a dead GOLEM's pose and keeps its corpse animated, lit and shadowed.
///
/// enemy and actor remain live throughout this persistent body state. Step 0
/// unlinks combat bodies and lock-on, awards the battle reference, and saves
/// the downed pose; step 1 emits a spark every fourth animation frame. Restored
/// corpses start at step 2 and skip those effects. Actor control pauses updates
/// or hides the corpse. Its eight scratch bytes are released on the running
/// path; paused and hidden returns leave them reserved until the next main-loop
/// frame resets the shared cursor.
static void _golemPawnRookDeadState(Enemy* enemy, Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_CORPSE_RELEASE_COMBAT   = 0,
        GOLEM_PAWN_ROOK_CORPSE_SPARKS           = 1,
        GOLEM_PAWN_ROOK_CORPSE_SPARK_FRAME_MASK = 3,
        GOLEM_PAWN_ROOK_CORPSE_SPARK_Y_MASK     = 511,
        GOLEM_PAWN_ROOK_CORPSE_BEHIND_ANIM      = 0x19,
        GOLEM_PAWN_ROOK_CORPSE_FRONT_ANIM       = 0x1D,
        GOLEM_PAWN_ROOK_FIRST_LAUNCHER_ID       = 0x38,
        GOLEM_PAWN_ROOK_CORPSE_SPARK_SIZE       = 1024,
        GOLEM_PAWN_ROOK_CORPSE_SHADOW_HALF_SIZE = 768,
        GOLEM_PAWN_ROOK_CORPSE_SHADOW_SHADE     = 128,
    };

    GolemPawnRookWork* work;
    GfxCoord*          coord;
    GfxCoord*          root;
    GfxCoord*          part;
    SVECTOR*           sparkOffset;
    VECTOR3            worldPos;
    s16                corpseAnim;
    u32                randomDraw;

    work        = actor->work;
    coord       = actor->extra.tmd->coords;
    sparkOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            actor->extra.tmd->flags       = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            coord->composeStamp                      = GRAPHICS_COORD_DIRTY;
            actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            root        = actor->extra.tmd->coords;
            worldPos.vx = root->workm.t[0];
            worldPos.vy = root->workm.t[1];
            worldPos.vz = root->workm.t[2];
            worldCoordUpdateActorColor(actor->spawnArg2.pointer, &worldPos, 0, 0);
            root        = actor->extra.tmd->coords;
            part        = &root[3];
            worldPos.vx = part->workm.t[0];
            worldPos.vy = root->workm.t[1];
            worldPos.vz = part->workm.t[2];
            effectDrawGroundShadow(&worldPos, GOLEM_PAWN_ROOK_CORPSE_SHADOW_HALF_SIZE, GOLEM_PAWN_ROOK_CORPSE_SHADOW_SHADE);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Release combat participation once, while retaining the corpse model and work.
    switch (work->step) {
        case GOLEM_PAWN_ROOK_CORPSE_RELEASE_COMBAT:
            enemy->recs = NULL;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->sightBody);
            worldCollisionUnlinkBody(&work->groundBody);
            worldCollisionUnlinkBody(&work->hurtBody);
            worldCollisionUnlinkBody(&work->strikeBody);
            if ((u32)((u16)work->actorId - GOLEM_PAWN_ROOK_FIRST_LAUNCHER_ID) < 2U) {
                worldCollisionUnlinkBody(&work->laserBody);
            }
            sceneReleaseBattleRefWithRewards(actor, work->actorId);
            corpseAnim = GOLEM_PAWN_ROOK_CORPSE_FRONT_ANIM;
            if (work->downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                corpseAnim = GOLEM_PAWN_ROOK_CORPSE_BEHIND_ANIM;
            }
            work->anim        = corpseAnim;
            work->step        = GOLEM_PAWN_ROOK_CORPSE_SPARKS;
            enemy->spawnState = (u8)work->downedPose;
            areaSaveEnemyPose(enemy);
            gSceneCombatState.golemPawnRookDeathAlert = 1;
            break;
        case GOLEM_PAWN_ROOK_CORPSE_SPARKS:
            if (!(work->animFrame & GOLEM_PAWN_ROOK_CORPSE_SPARK_FRAME_MASK)) {
                sparkOffset->vx = 0;
                sparkOffset->vz = 0;
                randomDraw      = RANDOM_LCG_NEXT(gRandomLcgState);
                sparkOffset->vy = -((randomDraw >> 0x10) & GOLEM_PAWN_ROOK_CORPSE_SPARK_Y_MASK);
                gRandomLcgState = randomDraw;
                effectSpawn(EFFECT_FLASH_BURST, &actor->extra.tmd->coords[3], GOLEM_PAWN_ROOK_CORPSE_SPARK_SIZE, sparkOffset);
            }
            break;
    }
    // Keep the saved downed pose animated and visible after combat has ended.
    _golemPawnRookAdvanceCorpsePose(actor, coord);
    actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    root        = actor->extra.tmd->coords;
    worldPos.vx = root->workm.t[0];
    worldPos.vy = root->workm.t[1];
    worldPos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &worldPos, 0, 0);
    root        = actor->extra.tmd->coords;
    part        = &root[3];
    worldPos.vx = part->workm.t[0];
    worldPos.vy = root->workm.t[1];
    worldPos.vz = part->workm.t[2];
    effectDrawGroundShadow(&worldPos, GOLEM_PAWN_ROOK_CORPSE_SHADOW_HALF_SIZE, GOLEM_PAWN_ROOK_CORPSE_SHADOW_SHADE);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
