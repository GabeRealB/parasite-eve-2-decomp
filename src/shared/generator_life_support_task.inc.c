/* Part of the Generator library; see generator.h. */

/// Task handler of the Life Support system part (the coordinate-bodied second
/// TaskDesc): copies gGeneratorLifeSupportStates onto the stack and calls the
/// entry for the task's state with the enemy in spawnArg2 and the task.
void generatorLifeSupportTask(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = gGeneratorLifeSupportStates;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
