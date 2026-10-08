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

    /// Plays a fall or impact cue at the body origin with its placement instance.
    ///
    /// Borrows actor/root and the carrier's gGolemPawnRookVoiceCues table; cueIndex
    /// must be in bounds. soundId/audioPan are s32 lvalues. All arguments must be
    /// side-effect-free; root and outputs occur repeatedly. The compound statement
    /// is confined to this handler and undefined below.
#define GOLEM_PAWN_ROOK_PLAY_KNOCKDOWN_CUE(actor, root, cueIndex, soundId, audioPan)                                                                                              \
    {                                                                                                                                                                             \
        (soundId)  = gGolemPawnRookVoiceCues[(cueIndex)] | ((((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT); \
        (audioPan) = (s8)worldCoordGetOriginAudioPan((root));                                                                                                                     \
        sndEvtRequestScriptStart((soundId), (s32)(audioPan), (s8)worldCoordGetOriginAudioDepth((root)));                                                                          \
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
                behindFallRandom  = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
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
                frontFallRandom   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
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
                shiftEndRandom  = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = shiftEndRandom;
                work->timer     = (u16)(((u32)shiftEndRandom >> 0x10) & GOLEM_PAWN_ROOK_KNOCKDOWN_REST_FRAME_MASK);
            }
            break;
    }

#undef GOLEM_PAWN_ROOK_PLAY_KNOCKDOWN_CUE
}
