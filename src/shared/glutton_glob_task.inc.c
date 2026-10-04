/* Part of the Glutton library; see glutton.h. */

/// Dispatcher of the grab enemy (`gGluttonGlobStates`): park the model
/// object while the global game mode is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it. Unlike `gluttonChunkTask` it guards the bookkeeping on the
/// state being non-zero rather than on the work block existing, and clears the
/// model object's flag word rather than leaving it 2.
void gluttonGlobTask(Task* arg0)
{
    EnemyTaskFuncTable5    sp;
    GluttonProjectileWork* work;

    sp   = gGluttonGlobStates;
    work = arg0->work;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg0->extra.tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            arg0->extra.tmd->flags = 0;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (arg0->state != 0) {
        if (work->prevState != arg0->state) {
            work->stateChanged = 1;
        } else {
            work->stateChanged = 0;
        }
        work->prevState = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
