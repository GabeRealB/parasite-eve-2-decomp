/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Ticks the buildup reaction to completion, then plays its recovery.
///
/// `task` is a live GOLEM body task with an active enemy buildup countdown.
/// Completion clears the GOLEM's buildup marker; the recovery animation then
/// returns to engage. The damage timer's grade and expiry rules belong to
/// `damageTickEnemyBuildup`.
static void _golemPawnRookBuildupState(Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_ANIM_BUILDUP_RECOVER    = 0x13,
        GOLEM_PAWN_ROOK_BUILDUP_WAIT            = 0,
        GOLEM_PAWN_ROOK_BUILDUP_RECOVER         = 1,
        GOLEM_PAWN_ROOK_BUILDUP_RECOVERY_FRAMES = 59,
    };

    GolemPawnRookWork* work;
    s16                step;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_BUILDUP_WAIT:
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->anim          = GOLEM_PAWN_ROOK_ANIM_BUILDUP_RECOVER;
                work->step          = GOLEM_PAWN_ROOK_BUILDUP_RECOVER;
                work->buildupActive = 0;
            }
            break;
        case GOLEM_PAWN_ROOK_BUILDUP_RECOVER:
            if (work->animFrame >= GOLEM_PAWN_ROOK_BUILDUP_RECOVERY_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            }
            break;
    }
}
