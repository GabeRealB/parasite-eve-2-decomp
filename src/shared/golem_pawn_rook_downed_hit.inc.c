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
                work->anim      = GOLEM_PAWN_ROOK_ANIM_LIE_BEHIND;
                work->behavior  = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                work->step      = GOLEM_PAWN_ROOK_KNOCKDOWN_REST_STEP;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & GOLEM_PAWN_ROOK_DOWNED_DWELL_MASK;
            }
            break;
        case GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT_FRAMES) {
                work->anim      = GOLEM_PAWN_ROOK_ANIM_LIE_FRONT;
                work->behavior  = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                work->step      = GOLEM_PAWN_ROOK_KNOCKDOWN_REST_STEP;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & GOLEM_PAWN_ROOK_DOWNED_DWELL_MASK;
            }
            break;
    }
}
