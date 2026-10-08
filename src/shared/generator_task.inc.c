/* Part of the Generator library; see generator.h. */

/// Dispatches the generator body's spawn, active or death state.
///
/// Task::state must be 0..2 and spawnArg2.pointer must hold its live Enemy.
/// The three-entry handler table is copied by value before dispatch.
static void _generatorTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = gGeneratorTaskStates;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
