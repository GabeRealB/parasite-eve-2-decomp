/* Part of the hopper waves library; see hopper_waves.h. */

/// Pair spawner: watches both hoppers and, once both are gone, marks the slot
/// done and ends.
void hopperWavePairWatch(Task* arg0)
{
    OverlayEncounterPairWork* work = (OverlayEncounterPairWork*)arg0->work;

    hopperWavePairCull(arg0);
    if (work->goneMask == 3) {
        gHopperWaveSlots[(s16)(arg0->spawnArg1.value >> 16)].status = 2;
        taskKill(arg0);
    }
}
