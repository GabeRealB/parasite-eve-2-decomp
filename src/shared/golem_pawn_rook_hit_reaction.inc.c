/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Hit-reaction state, entry 8 of `Actor05600_D16540`: step 0 starts
/// animation 0x11 and stops the actor; step 1 waits for frame 0x37, then parks
/// on animation 2 (entry 2) or, with `field_6E0` set, on animation 0x14
/// (entry 0xA).
void golemPawnRookHitReactionState(Task* task)
{
    Actor105600Work* work;
    s16              state;

    work  = (Actor105600Work*)task->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_694 = 0x11;
            work->field_6A8 = 1;
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x37) {
                if (work->field_6E0 == 0) {
                    work->field_694 = 2;
                    work->field_6A6 = 2;
                    work->field_6A8 = 0;
                } else {
                    work->field_694 = 0x14;
                    work->field_6A6 = 0xA;
                    work->field_6A8 = 0;
                }
            }
            break;
    }
}
