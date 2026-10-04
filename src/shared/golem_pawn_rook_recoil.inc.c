/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 9 of the `behavior` table: step 0 starts animation 0x12 when
/// `hitFromFront` is 1 (step 1, waits for frame 0x50) and animation 0x13
/// otherwise (step 2, waits for frame 0x3B); either way the dwell counters are
/// cleared and the enemy parks on animation 2 (entry 2) when done.
void golemPawnRookRecoilState(Task* arg0)
{
    GolemPawnRookWork* work;
    s32                state;
    s32                next;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            next = work->hitFromFront;
            if (next == 1) {
                work->anim = 0x12;
                work->step = next;
            } else {
                work->anim = 0x13;
                work->step = 2;
            }
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            break;
        case 1:
            if (work->animFrame >= 0x50) {
                work->anim     = 2;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = 0;
            }
            break;
        case 2:
            if (work->animFrame >= 0x3B) {
                work->anim     = state;
                work->behavior = state;
                work->step     = 0;
            }
            break;
    }
}
