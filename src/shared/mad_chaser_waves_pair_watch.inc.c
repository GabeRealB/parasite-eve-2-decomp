/* Part of the scripted encounter library; see mad_chaser_waves.h. */

/// Completes the encounter row once both Sucklercephs have died or been culled.
///
/// Requires live pair work, borrowed enemy tasks and a high spawn halfword
/// selecting row 0..16. The carrier's cull check releases enemy pointers and
/// records their gone bits on the next update. Enemy teardown is separate;
/// killing the spawner releases its owned work after marking the row done.
static void _overlayEncounterPairWatch(Task* waveTask)
{
    OverlayEncounterPairWork* work = waveTask->work;

    _actor342400WaveCullPair(waveTask);
    if (work->goneMask == OVERLAY_ENCOUNTER_PAIR_GONE_BOTH) {
        gMadChaserWaveSlots[(s16)(waveTask->spawnArg1.value >> 16)].status = OVERLAY_ENCOUNTER_SLOT_DONE;
        taskKill(waveTask);
    }
}
