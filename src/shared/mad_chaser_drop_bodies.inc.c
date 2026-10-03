/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Detaches the enemy's records and unlinks its three hit bodies, clears the
/// frame counter and advances the state.
void madChaserDropBodies(Task* arg0)
{
    MadChaserWork* work2;
    MadChaserWork* work;

    work                                    = (MadChaserWork*)arg0->work;
    ((Enemy*)arg0->spawnArg2.pointer)->recs = 0;
    work2                                   = (MadChaserWork*)arg0->work;
    Gp_UnlinkObj(&work2->pairBody);
    Gp_UnlinkObj(&work2->gridBody);
    Gp_UnlinkObj(&work2->attackBody);
    work->stateFrames = 0;
    work->state       = work->state + 1;
}
