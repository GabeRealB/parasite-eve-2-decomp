/* Part of the pair walk library; see pair_walk.h. */

/// Releases a pair walker's enemy work and tears down its task and child models.
///
/// `task->spawnArg2.pointer` must borrow the live primary-heap enemy belonging
/// to this task. Teardown frees `Task::work` and dispatches linked children's
/// exit callbacks; the enemy and work pointers are invalid on return. Model
/// storage release follows `taskKill`'s immediate or delayed policy.
static void _pairWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}
