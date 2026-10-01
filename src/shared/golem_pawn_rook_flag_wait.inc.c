/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xA of `Actor05600_D16540`: step 0 waits for `Gp_TickObjFlag2` on
/// the spawn context to fire, then starts animation 0x13 and clears
/// `field_6E0`; step 1 waits for frame 0x3B and parks on animation 2
/// (entry 2).
void golemPawnRookFlagWaitState(Task* task)
{
    GolemPawnRookWork* work;
    s16                state;

    work  = (GolemPawnRookWork*)task->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2(task->spawnArg2.pointer) != 0) {
                work->field_694 = 0x13;
                work->field_6A8 = 1;
                work->field_6E0 = 0;
            }
            break;
        case 1:
            if (work->field_698 >= 0x3B) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
    }
}
