/* Part of the Mad Chaser library; see mad_chaser.h. */

/// While `hitTaken` is 1, consumes the pending request in `hitReaction`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `hitTaken` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 madChaserTakeHitRequest(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    if (work->hitTaken == 1) {
        switch ((s16)(work->hitReaction - 1)) {
            case MAD_CHASER_HIT_REACTION_LIGHT - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 6;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_HEAVY - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 7;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_STATUS - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 8;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_BLAST - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 7;
                w->subState      = 0;
                break;
            }
            case MAD_CHASER_HIT_REACTION_KNOCKDOWN - 1: {
                MadChaserWork* w = (MadChaserWork*)arg0->work;
                w->state         = 9;
                w->subState      = 0;
                break;
            }
        }
        work->hitReaction = MAD_CHASER_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}
