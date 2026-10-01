/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xE of the `field_6A6` table: step 0 starts animation 0x17 when
/// `field_6B8` is 1 (step 1, waits for frame 0x10) and animation 0x1B
/// otherwise (step 2, waits for frame 0x16); when done the enemy's handler
/// chain advances to state 2.
void golemPawnRookDownedFinishState(Task* arg0)
{
    Actor105600Work* work;
    s32              sel;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            /* The 32-bit local is load-bearing: an s16 one makes combine fold
             * the sign-extension into a second `lh` of field_6B8. */
            sel = work->field_6B8;
            if (sel == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = sel;
                return;
            }
            work->field_694 = 0x1B;
            work->field_6A8 = 2;
            return;
        case 1:
            if (work->field_698 >= 0x10) {
                arg0->state     = 2;
                work->field_6A8 = 0;
            }
            return;
        case 2:
            if (work->field_698 >= 0x16) {
                arg0->state     = state;
                work->field_6A8 = 0;
            }
            return;
    }
}
