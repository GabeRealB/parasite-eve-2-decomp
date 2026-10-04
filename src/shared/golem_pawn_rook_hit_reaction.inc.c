/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Hit-reaction state, entry 8 of `Actor05600_D16540`: step 0 starts
/// animation 0x11 and stops the actor; step 1 waits for frame 0x37, then parks
/// on animation 2 (entry 2) or, with `buildupActive` set, on animation 0x14
/// (entry 0xA).
void golemPawnRookHitReactionState(Task* task)
{
    GolemPawnRookWork* work;
    s16                state;

    work  = task->work;
    state = work->step;
    switch (state) {
        case 0:
            work->anim         = 0x11;
            work->step         = 1;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            break;
        case 1:
            if (work->animFrame >= 0x37) {
                if (work->buildupActive == 0) {
                    work->anim     = 2;
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                    work->step     = 0;
                } else {
                    work->anim     = 0x14;
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_BUILDUP;
                    work->step     = 0;
                }
            }
            break;
    }
}
