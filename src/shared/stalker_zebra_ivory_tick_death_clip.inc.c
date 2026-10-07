/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Advances the death clip and enters the next death phase at a slot boundary.
///
/// Requires a live initialized body rig in death phase 2. Tests slot 1 after
/// ticking; a boundary, loop jump or settled end advances `state` without
/// resetting `subState`.
static void _stalkerZebraIvoryTickDeathClip(Task* task)
{
    StalkerZebraIvoryWork* work = task->work;

    _stalkerZebraIvoryTickAnim(task);
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        work->state = work->state + 1;
    }
}
