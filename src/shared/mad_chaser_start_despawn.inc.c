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
/// Hands the ordinary-death body to timed despawn at behavior and sub-state zero.
///
/// Requires live task-owned work in ordinary-death behavior 6 after shrink has
/// hidden the model. Enters `MAD_CHASER_TASK_DESPAWN`, whose first step resets
/// the counter before the 36-update destruction delay. Animation requests and
/// frame counters are retained here; enemy, work and model storage remain live.
static void _madChaserStartDespawn(Task* task)
{
    _madChaserEnterTaskState(task, MAD_CHASER_TASK_DESPAWN);
}
#endif
