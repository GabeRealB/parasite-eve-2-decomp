/* Part of the view figure library; see view_figure.h. */

/// Begins teardown of the helper model, then releases the figure and its enemy.
///
/// `task` must own a live `Enemy` in `spawnArg2.pointer`; `gActorHelperTask`
/// must be its live helper from the spawn state. Default task teardown releases
/// their work and follows `taskKill`'s immediate/deferred model-release rules.
/// The published task and work pointers retain their values and must no longer
/// be used after this callback.
static void _viewFigureExit(Task* task)
{
    // The helper's model root borrows a coordinate owned by the figure's model.
    taskKill(gActorHelperTask);
    enemyDestroy(task->spawnArg2.pointer, task);
}
