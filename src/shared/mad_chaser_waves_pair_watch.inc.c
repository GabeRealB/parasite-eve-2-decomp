/* Part of the Mad Chaser waves library; see mad_chaser_waves.h. */

/// Pair spawner: watches both Mad Chasers and, once both are gone, marks the slot
/// done and ends.
void madChaserWavePairWatch(Task* arg0)
{
    OverlayEncounterPairWork* work = arg0->work;

    madChaserWavePairCull(arg0);
    if (work->goneMask == OVERLAY_ENCOUNTER_PAIR_GONE_BOTH) {
        gMadChaserWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(arg0);
    }
}
