/* Part of the Mad Chaser library; see mad_chaser.h. */

/// While `hitTaken` is 1, consumes the pending request in `hitReaction`:
/// request 3 moves the state machine to state 8 and request 5 to state 9,
/// anything else is just cleared. Returns 1 when `hitTaken` is 1 and 0
/// otherwise.
s32 madChaserTakeKnockdownRequest(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (work->hitTaken == 1) {
        switch (work->hitReaction) {
            case MAD_CHASER_HIT_REACTION_STATUS:
                work->state    = 8;
                work->subState = 0;
                break;
            case MAD_CHASER_HIT_REACTION_KNOCKDOWN:
                work->state    = 9;
                work->subState = 0;
                break;
        }
        work->hitReaction = MAD_CHASER_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}
