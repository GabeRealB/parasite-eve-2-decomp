/* Part of the Mad Chaser library; see mad_chaser.h. */

#ifdef MAD_CHASER_SHRINK_DEATH_DESPAWN_HANDLER
/// Hands the shrunken enemy to timed despawn at behavior and sub-state zero.
///
/// Requires live task-owned Mad Chaser work at scripted shrink-death behavior 6.
/// The preceding shrink behavior has hidden the model. Enters task state
/// MAD_CHASER_TASK_DESPAWN, whose first tick resets the frame counter and whose
/// next 36 ticks release the enemy. All allocations remain live through this
/// call; animation requests and frame counters are retained.
static void MAD_CHASER_SHRINK_DEATH_DESPAWN_HANDLER(Task* task)
{
    _madChaserEnterTaskState(task, MAD_CHASER_TASK_DESPAWN);
}
#else
/// Puts the task in state 5, the despawn phase, with `state` and
/// `subState` cleared.
void madChaserStartDespawn(Task* arg0)
{
    MadChaserWork* work;

    work           = (MadChaserWork*)arg0->work;
    arg0->state    = 5;
    work->state    = 0;
    work->subState = 0;
}
#endif
