/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Releases the enemy's battle reference with rewards and starts its death clip.
///
/// The current `animClip` must index the carrier's `_gStalkerZebraIvoryDeathClipsByPose`
/// mapping (0..34 in both carriers). Restarts the mapped clip at normal rate,
/// ticks the body rig once and advances to the death-clip wait phase. Does not
/// reset `subState`.
static void _stalkerZebraIvoryStartDeathClip(Task* task)
{
    StalkerZebraIvoryWork* work = task->work;

    sceneReleaseBattleRefWithRewards(task, 0);
    _stalkerZebraIvoryRequestClipRestart(task, _gStalkerZebraIvoryDeathClipsByPose[work->animClip], ANIMATION_RATE_ONE);
    _stalkerZebraIvoryTickAnim(task);
    work->state = work->state + 1;
}

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
