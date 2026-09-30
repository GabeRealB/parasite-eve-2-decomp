/* Part of the hopper waves library; see hopper_waves.h. */

/// Forgets each pair member whose HP is gone and sets its bit in goneMask once
/// it is empty.
void hopperWavePairDropDead(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;

    if (work->enemy0 != NULL) {
        if (work->enemy0->hp <= 0) {
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= 1;
    }
    if (work->enemy1 != NULL) {
        if (work->enemy1->hp <= 0) {
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= 2;
    }
}
