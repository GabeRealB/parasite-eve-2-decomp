/* Part of the Glutton library; see glutton.h. */

/// Dispatches the thrown sphere's three-state coordinate-body lifecycle.
///
/// Requires a live enemy in `spawnArg2.pointer`, coordinate body and state
/// 0..2; states after spawn require projectile work. Hidden actor control
/// skips dispatch. Running, paused and other control values dispatch; the
/// flight handler controls which motion runs while paused. A handler may
/// destroy the task.
static void _gluttonThrowTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = gGluttonThrowStates;
    switch (gSceneCombatState.actorControl) {
        default:
        case SCENE_COMBAT_ACTORS_RUNNING:
        case SCENE_COMBAT_ACTORS_PAUSED:
            states.funcs[task->state](task->spawnArg2.pointer, task);
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            break;
    }
}
