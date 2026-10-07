/* Part of the Glutton library; see glutton.h. */

/// Records whether this tick enters a new spinner task state.
///
/// Borrows live arguments for this call; no pointer is retained.
static __inline__ void _gluttonRecordSpinnerTaskState(GluttonSpinnerWork* work, Task* task)
{
    if (work->prevState != task->state) {
        work->stateChanged = 1;
    } else {
        work->stateChanged = 0;
    }
    work->prevState = task->state;
}

/// Dispatches a spinner and records entry into each task state.
///
/// Requires a live enemy in `spawnArg2.pointer`, TMD body and state 0..3;
/// states after spawn require spinner work. Paused ticks retain normal
/// drawing and skip dispatch; hidden ticks suppress drawing and dispatch.
/// Running ticks restore normal drawing. A handler may destroy the task.
/// Other control values dispatch without changing the model flags.
static void _gluttonSpinnerTask(Task* task)
{
    EnemyTaskFuncTable4 states;
    GluttonSpinnerWork* work;

    states = gGluttonSpinnerStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            task->extra.tmd->flags = 0;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (task->work != NULL) {
        work = task->work;
        _gluttonRecordSpinnerTaskState(work, task);
    }
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
