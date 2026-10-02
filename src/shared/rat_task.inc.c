/* Part of the Rat library; see rat.h. */

/// The rat's task callback: dispatches Task::state through gRatStateHandlers
/// (spawn, update, death).
void ratTask(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = gRatStateHandlers;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}
