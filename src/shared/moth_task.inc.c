/* Part of the Moth library; see moth.h. */

/// The moth's task callback: dispatches Task::state through gMothStateHandlers
/// (spawn, update, death).
void mothTask(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = gMothStateHandlers;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
