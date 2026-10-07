/* Part of the Glutton library; see glutton.h. */

/// Dispatches the rain blob's five-state coordinate-body lifecycle.
///
/// Requires a live enemy in `spawnArg2.pointer`, coordinate body and state
/// 0..4; states after spawn require projectile work. Paused and hidden actor
/// control skip dispatch; running and other control values dispatch normally.
/// A handler may destroy the task.
static void _gluttonRainTask(Task* task)
{
    EnemyTaskFuncTable5 states;

    states = gGluttonRainStates;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            states.funcs[task->state](task->spawnArg2.pointer, task);
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
        case SCENE_COMBAT_ACTORS_HIDDEN:
            break;
    }
}
