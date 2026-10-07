/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Releases the player-state lock, restarts the clip `gStalkerZebraIvoryResumeClips`
/// gives for the current one at step 0x10, ticks it and moves to the next
/// state.
void stalkerZebraIvoryResumeClip(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    sceneReleaseBattleRefWithRewards(arg0, 0);
    stalkerZebraIvoryPlayClip(arg0, gStalkerZebraIvoryResumeClips[work->animClip], 0x10);
    stalkerZebraIvoryTickAnim(arg0);
    work->state = work->state + 1;
}
