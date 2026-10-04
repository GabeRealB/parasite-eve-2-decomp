/* Part of the Mad Chaser waves library; see mad_chaser_waves.h. */

/// Forgets each pair member whose HP is gone and sets its bit in goneMask once
/// it is empty.
void madChaserWavePairDropDead(Task* arg0)
{
    OverlayEncounterPairWork* work = arg0->work;

    if (work->enemy0 != NULL) {
        if (work->enemy0->hp <= 0) {
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY0;
    }
    if (work->enemy1 != NULL) {
        if (work->enemy1->hp <= 0) {
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY1;
    }
}
