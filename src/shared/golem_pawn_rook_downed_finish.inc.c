/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xE of the `behavior` table: step 0 starts animation 0x17 when
/// `downedPose` is 1 (step 1, waits for frame 0x10) and animation 0x1B
/// otherwise (step 2, waits for frame 0x16); when done the enemy's handler
/// chain advances to state 2.
void golemPawnRookDownedFinishState(Task* arg0)
{
    GolemPawnRookWork* work;
    s32                sel;
    s16                state;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            /* The 32-bit local is load-bearing: an s16 one makes combine fold
             * the sign-extension into a second `lh` of downedPose. */
            sel = work->downedPose;
            if (sel == 1) {
                work->anim = 0x17;
                work->step = sel;
                return;
            }
            work->anim = 0x1B;
            work->step = 2;
            return;
        case 1:
            if (work->animFrame >= 0x10) {
                arg0->state = 2;
                work->step  = 0;
            }
            return;
        case 2:
            if (work->animFrame >= 0x16) {
                arg0->state = state;
                work->step  = 0;
            }
            return;
    }
}
