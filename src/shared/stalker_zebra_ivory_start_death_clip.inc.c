/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Releases the enemy's battle reference with rewards and starts its death clip.
///
/// The current `animClip` must index the carrier's `gStalkerZebraIvoryResumeClips`
/// mapping (0..34 in both carriers). Restarts the mapped clip at normal rate,
/// ticks the body rig once and advances to the death-clip wait phase. Does not
/// reset `subState`.
static void _stalkerZebraIvoryStartDeathClip(Task* task)
{
    StalkerZebraIvoryWork* work = task->work;

    sceneReleaseBattleRefWithRewards(task, 0);
    _stalkerZebraIvoryRequestClipRestart(task, gStalkerZebraIvoryResumeClips[work->animClip], ANIMATION_RATE_ONE);
    _stalkerZebraIvoryTickAnim(task);
    work->state = work->state + 1;
}
