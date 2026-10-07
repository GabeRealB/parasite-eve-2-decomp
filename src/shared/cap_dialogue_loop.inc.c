/* Part of the cap dialogue library; see cap_dialogue.h. */

/// Runs cap command `spawnArg1` and waits for it to finish. When the cap
/// reports event key 0xF it sends `playerActorSetScriptedControl(1)`, undoing the
/// `playerActorSetScriptedControl(0)` its spawner sent, and kills itself; any other key
/// runs the command again.
void capDialogueLoopTask(Task* task)
{
    switch (task->state) {
        case 0:
            capRunCommandWithTransition(task->spawnArg1.value);
            task->state += 1;
            break;
        case 1:
            if (capIsBusy() != 0) {
                break;
            }
            task->state += 1;
            break;
        case 2:
            if (capGetVariantKey() == 0xF) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            } else {
                task->state = 0;
            }
            break;
    }
}
