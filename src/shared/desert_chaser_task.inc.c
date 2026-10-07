/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Dispatches the enemy task's lifecycle state through its carrier's table.
///
/// Requires a Task::state within the table and a live attached Enemy. Cutscene
/// and Water Tower builds have spawn, frame update and teardown states. The
/// regular build inserts a wait for a pending player/battle release before
/// teardown. The stack copy retains each table's Enemy/Task callback contract.
static void _desertChaserTask(Task* task)
{
    DesertChaserTaskStates states;

    states = gDesertChaserTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
