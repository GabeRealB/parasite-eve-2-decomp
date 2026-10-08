#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Returns to the downed lying pose with a fresh 0..63-frame dwell.
static inline void _golemKnightBishopResumeDownedRest(GolemKnightBishopWork* work, s16 lyingAnimation)
{
    enum {
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP = 3,
        GOLEM_KNIGHT_BISHOP_DOWNED_DWELL_MASK   = 63,
    };
    work->anim      = lyingAnimation;
    work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL;
    work->step      = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->timer     = (gRandomLcgState >> 16) & GOLEM_KNIGHT_BISHOP_DOWNED_DWELL_MASK;
}

/// Flinches from a hit while downed, then resumes the corresponding lying pose.
///
/// `task` owns a live GOLEM work block. The saved pose selects a 16- or 22-frame
/// reaction; completion returns to the knockdown rest step with a new random
/// dwell of 0..63 frames.
static void _golemKnightBishopDownedHitSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START  = 0,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND = 1,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT  = 2,
        GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND        = 0x10,
        GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT         = 0x14,
    };
    GolemKnightBishopWork* work;
    s16                    step;
    s32                    downedPose;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START:
            downedPose = work->downedPose;
            if (downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND;
                work->step = downedPose;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT;
                work->step = GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_BEHIND_FRAMES) {
                _golemKnightBishopResumeDownedRest(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_FRONT_FRAMES) {
                _golemKnightBishopResumeDownedRest(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT);
            }
            break;
    }
}
