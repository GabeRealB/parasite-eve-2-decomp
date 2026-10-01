/* Part of the Glutton library; see glutton.h. */

/// Dispatcher of the dropped enemy (`gGluttonRainStates`): run the handler
/// for the task's state, skipped while the global game mode is 1 or 2.
void gluttonRainTask(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = gGluttonRainStates;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
        case SCENE_COMBAT_ACTORS_HIDDEN:
            break;
    }
}
