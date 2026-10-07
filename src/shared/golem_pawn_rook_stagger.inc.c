/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Plays the standing stagger, then resumes engage or an interrupted buildup.
///
/// `task` is a live GOLEM body task. Movement stops at entry. The buildup marker
/// preserves a pending reaction across the stagger rather than restarting it.
static void _golemPawnRookStaggerState(Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_BUILDUP   = 0x14,
        GOLEM_PAWN_ROOK_ANIM_STAGGER   = 0x11,
        GOLEM_PAWN_ROOK_STAGGER_START  = 0,
        GOLEM_PAWN_ROOK_STAGGER_WAIT   = 1,
        GOLEM_PAWN_ROOK_STAGGER_FRAMES = 55,
    };

    GolemPawnRookWork* work;
    s16                step;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_STAGGER_START:
            work->anim         = GOLEM_PAWN_ROOK_ANIM_STAGGER;
            work->step         = GOLEM_PAWN_ROOK_STAGGER_WAIT;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            break;
        case GOLEM_PAWN_ROOK_STAGGER_WAIT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_STAGGER_FRAMES) {
                if (work->buildupActive == 0) {
                    work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                    work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                } else {
                    work->anim     = GOLEM_PAWN_ROOK_ANIM_BUILDUP;
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_BUILDUP;
                    work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                }
            }
            break;
    }
}
