/* Part of the hopper waves library; see hopper_waves.h. */

/// Controller state 1: spawns the first three encounter slots and advances.
void hopperWaveOpen(Task* arg0)
{
    s32                       i;
    OverlayEncounterCtrlWork* work = (OverlayEncounterCtrlWork*)arg0->work;
    OverlayEncounterSlot*     slot;

    for (i = 0; i < 3; i++) {
        slot = &gHopperWaveSlots[work->nextSlot];
        hopperWaveSpawnSlot(work->nextSlot, slot->kind, slot->command);
        work->nextSlot++;
    }
    arg0->state++;
}
