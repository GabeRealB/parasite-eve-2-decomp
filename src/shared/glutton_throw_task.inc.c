/* Part of the Glutton library; see glutton.h. */

/// Dispatcher of the enemy `gGluttonThrowStates` drives: run the handler for
/// the task's state, skipped while the global game mode is 2.
void gluttonThrowTask(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = gGluttonThrowStates;
    switch (gSceneCombatState.actorControl) {
        default:
        case SCENE_COMBAT_ACTORS_RUNNING:
        case SCENE_COMBAT_ACTORS_PAUSED:
            sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            break;
    }
}
