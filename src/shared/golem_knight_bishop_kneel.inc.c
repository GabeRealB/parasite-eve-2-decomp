#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Releases the fall lock and begins the side's lying pose with a random dwell.
static inline void _golemKnightBishopFinishKnockdownFall(GolemKnightBishopWork* work, s16 lyingAnimation)
{
    enum { GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP = 3 };
    u32 randomDraw;
    work->anim         = lyingAnimation;
    work->step         = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP;
    work->reactionLock = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
    randomDraw         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState    = randomDraw;
    work->timer        = (randomDraw >> 16) & 0x3F;
}

/// Falls at low HP, then alternates the retained lying and writhing poses.
///
/// Requires live GOLEM work and model coordinates. The hit side chooses the
/// fall and hurt-sphere offset; grid participation moves off the root sphere.
/// The fall lock prevents reactions until the lying pose starts. Rest draws
/// 0..63 frames, with decrement before testing: stored 0 and 1 both leave rest
/// on its next update. Writhing lasts ten updates before another rest draw.
static void _golemKnightBishopKnockdownSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_START       = 0,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_BEHIND = 1,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_FRONT  = 2,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST        = 3,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_WRITHE      = 4,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    sound;
    s32                    downedAnimation;
    u32                    randomDraw;
    s16                    framesLeft;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_START:
            if (work->hitFromFront == 0) {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND;
                work->step            = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_BEHIND;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND_OFFSET_Z;
            } else {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT;
                work->step            = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_FRONT;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT_OFFSET_Z;
            }
            work->hurtBody.radius   = GOLEM_KNIGHT_BISHOP_HURT_RADIUS;
            work->knockdownStage    = GOLEM_KNIGHT_BISHOP_FALL_STARTED;
            work->fadeState         = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
            work->reactionLock      = GOLEM_KNIGHT_BISHOP_REACTION_FALLING;
            work->forwardSpeed      = 0;
            work->hurtBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_BEHIND:
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_FALL_BEHIND_IMPACT_FRAME) {
                sound = gGolemKnightBishopAnimCues[work->soundSet + 8] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_FALL_BEHIND_FRAMES) {
                _golemKnightBishopFinishKnockdownFall(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND);
            }
            if (work->knockdownStage == GOLEM_KNIGHT_BISHOP_FALL_STARTED) {
                work->knockdownStage = GOLEM_KNIGHT_BISHOP_FALL_SETTLED;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_FRONT:
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_FALL_FRONT_IMPACT_FRAME) {
                sound = gGolemKnightBishopAnimCues[work->soundSet + 8] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_FALL_FRONT_FRAMES) {
                _golemKnightBishopFinishKnockdownFall(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT);
            }
            if (work->knockdownStage == GOLEM_KNIGHT_BISHOP_FALL_STARTED) {
                work->knockdownStage = GOLEM_KNIGHT_BISHOP_FALL_SETTLED;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_FRONT;
                if (work->downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                    downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_BEHIND;
                }
                work->timer = 0xA;
                work->anim  = downedAnimation;
                work->step  = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_WRITHE;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_WRITHE:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT;
                if (work->downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                    downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND;
                }
                work->anim      = downedAnimation;
                work->step      = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST;
                randomDraw      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomDraw;
                work->timer     = (randomDraw >> 16) & 0x3F;
            }
            break;
    }
}
