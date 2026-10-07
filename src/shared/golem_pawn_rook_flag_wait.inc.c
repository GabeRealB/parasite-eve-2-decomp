/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xA of `Actor05600_D16540`: step 0 waits for `damageTickEnemyBuildup` on
/// the spawn context to fire, then starts animation 0x13 and clears
/// `buildupActive`; step 1 waits for frame 0x3B and parks on animation 2
/// (entry 2).
void golemPawnRookFlagWaitState(Task* task)
{
    GolemPawnRookWork* work;
    s16                state;

    work  = task->work;
    state = work->step;
    switch (state) {
        case 0:
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->anim          = 0x13;
                work->step          = 1;
                work->buildupActive = 0;
            }
            break;
        case 1:
            if (work->animFrame >= 0x3B) {
                work->anim     = 2;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = 0;
            }
            break;
    }
}
