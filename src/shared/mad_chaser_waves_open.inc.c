/* Part of the scripted encounter library; see mad_chaser_waves.h. */

/// Starts the encounter's first three rows and advances its controller.
///
/// Requires live controller work initialized with nextSlot 0 and at least
/// three complete `gMadChaserWaveSlots` rows. The carrier binds the declared
/// void(s16, s16, s16) `OVERLAY_ENCOUNTER_SPAWN_SLOT` callback around inclusion.
/// Each argument is evaluated once; the binding is undefined after the body.
/// Advances the cursor even when the requested row fails to spawn.
static void _overlayEncounterOpen(Task* controllerTask)
{
    enum {
        OVERLAY_ENCOUNTER_OPEN_SLOT_COUNT = 3,
    };
    s32                             openingSlot;
    OverlayEncounterControllerWork* work = controllerTask->work;
    OverlayEncounterSlot*           slot;

    for (openingSlot = 0; openingSlot < OVERLAY_ENCOUNTER_OPEN_SLOT_COUNT; openingSlot++) {
        slot = &gMadChaserWaveSlots[work->nextSlot];
        OVERLAY_ENCOUNTER_SPAWN_SLOT(work->nextSlot, slot->kind, slot->command);
        work->nextSlot++;
    }
    controllerTask->state++;
}

#undef OVERLAY_ENCOUNTER_SPAWN_SLOT
