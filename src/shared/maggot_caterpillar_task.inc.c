/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Dispatches one body setup, active-frame or dying handler.
///
/// `actor->state` must be 0..2 and `spawnArg2.pointer` its live owning enemy.
/// Copies all three callbacks before dispatch. Setup failure or completed
/// death may destroy the task; neither argument is used after dispatch.
static void _maggotCaterpillarTask(Task* actor)
{
    EnemyTaskFuncTable3 states;

    states = gMaggotCaterpillarStates;
    states.funcs[actor->state](actor->spawnArg2.pointer, actor);
}
