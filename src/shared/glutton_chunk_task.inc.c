/* Part of the Glutton library; see glutton.h. */

/// Dispatcher of the `gGluttonChunkStates` enemy: park the model object
/// while the global game mode is 1 or 2, otherwise note in the work block
/// whether the state changed since the last step and run the handler for it.
/// The model object's flag word is left at 2 while the enemy runs.
void gluttonChunkTask(Task* arg0)
{
    EnemyTaskFuncTable4    sp;
    GluttonProjectileWork* work;

    sp = gGluttonChunkStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (arg0->work != NULL) {
        work = arg0->work;
        if (work->prevState != arg0->state) {
            work->stateChanged = 1;
        } else {
            work->stateChanged = 0;
        }
        work->prevState = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
