/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Plays a fatal reaction while downed, then hands the body task to its dead state.
///
/// `actor` is a live GOLEM body task. The saved downed pose selects the reaction
/// clip and its end frame; no movement or collision shape is changed here.
static void _golemPawnRookDownedDeathState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_DOWNED_DEATH_START  = 0,
        GOLEM_PAWN_ROOK_DOWNED_DEATH_BEHIND = 1,
        GOLEM_PAWN_ROOK_DOWNED_DEATH_FRONT  = 2,
    };

    GolemPawnRookWork* work;
    s32                downedPose;
    s16                step;

    work = actor->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_DOWNED_DEATH_START:
            // Keep the widened pose snapshot used again as the handler step.
            downedPose = work->downedPose;
            if (downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                work->anim = GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_BEHIND;
                work->step = downedPose;
                return;
            }
            work->anim = GOLEM_PAWN_ROOK_ANIM_DOWNED_HIT_FRONT;
            work->step = GOLEM_PAWN_ROOK_DOWNED_DEATH_FRONT;
            return;
        case GOLEM_PAWN_ROOK_DOWNED_DEATH_BEHIND:
            if (work->animFrame >= GOLEM_PAWN_ROOK_DOWNED_HIT_BEHIND_FRAMES) {
                actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
                work->step   = GOLEM_PAWN_ROOK_DOWNED_DEATH_START;
            }
            return;
        case GOLEM_PAWN_ROOK_DOWNED_DEATH_FRONT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_DOWNED_HIT_FRONT_FRAMES) {
                actor->state = step;
                work->step   = GOLEM_PAWN_ROOK_DOWNED_DEATH_START;
            }
            return;
    }
}
