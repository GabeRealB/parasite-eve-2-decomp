/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Dispatches one puff setup, flight or destruction handler.
///
/// `task->state` must be 0..2 and `spawnArg2.pointer` its live enemy context.
/// The three callbacks are copied by value before dispatch; a handler may
/// destroy the task and its work. The carrier and parent actor remain loaded.
static void _maggotCaterpillarPuffTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = gMaggotCaterpillarPuffStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
