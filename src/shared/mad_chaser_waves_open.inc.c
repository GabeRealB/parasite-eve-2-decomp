/* Part of the Mad Chaser waves library; see mad_chaser_waves.h. */

/// Controller state 1: spawns the first three encounter slots and advances.
void madChaserWaveOpen(Task* arg0)
{
    s32                             i;
    OverlayEncounterControllerWork* work = arg0->work;
    OverlayEncounterSlot*           slot;

    for (i = 0; i < 3; i++) {
        slot = &gMadChaserWaveSlots[work->nextSlot];
        OVERLAY_ENCOUNTER_SPAWN_SLOT(work->nextSlot, slot->kind, slot->command);
        work->nextSlot++;
    }
    arg0->state++;
}

#undef OVERLAY_ENCOUNTER_SPAWN_SLOT
