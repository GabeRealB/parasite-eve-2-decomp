/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Releases a battle hold with rewards and resumes the mapped clip at normal speed.
///
/// `gStalkerZebraIvoryResumeClips` selects the clip from the current one; this
/// call restarts and ticks it before advancing the state.
void stalkerZebraIvoryResumeClip(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    sceneReleaseBattleRefWithRewards(arg0, 0);
    _stalkerZebraIvoryRequestClipRestart(arg0, gStalkerZebraIvoryResumeClips[work->animClip], ANIMATION_RATE_ONE);
    _stalkerZebraIvoryTickAnim(arg0);
    work->state = work->state + 1;
}
