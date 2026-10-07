/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Chooses a ceiling drop beyond 2000 coordinate units, otherwise a grab.
///
/// Called at the end of ceiling-exit behavior. Uses the cached horizontal
/// target distance; equality chooses the grab. Disables the capsule's grid
/// probe before selecting the behavior and resetting `subState`.
static void _stalkerZebraIvorySelectCeilingExit(Task* task)
{
    enum { STALKER_ZEBRA_IVORY_CEILING_DROP_DISTANCE = 2000 };
    StalkerZebraIvoryWork* work = task->work;

    _stalkerZebraIvoryDisableCapsuleGrid(task);
    if (work->playerDistance > STALKER_ZEBRA_IVORY_CEILING_DROP_DISTANCE) {
        _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_CEILING_DROP);
    } else {
        _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_GRAB);
    }
}
