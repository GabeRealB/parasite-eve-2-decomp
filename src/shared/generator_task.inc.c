/* Part of the Generator library; see generator.h. */

/// Task handler of the generator body: copies gGeneratorTaskStates (spawn,
/// generatorTickState, generatorDeathState) onto the stack and calls the entry
/// for the task's state with the enemy in spawnArg2 and the task.
void generatorTask(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = gGeneratorTaskStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
