/* Part of the cap dialogue library; see cap_dialogue.h. */

/// Repeats a room dialogue command until CAP returns the exit choice.
///
/// spawnArg1 is a valid command index in the live CAP resources. The spawner
/// holds scripted player control; this task restores it and kills itself when
/// choice 15 is reported after playback ends. Other choices restart the command.
/// The task waits on global CAP availability and owns no work or playback data.
static void _capDialogueLoopTask(Task* task)
{
    enum {
        CAP_DIALOGUE_LOOP_START       = 0,
        CAP_DIALOGUE_LOOP_WAIT        = 1,
        CAP_DIALOGUE_LOOP_CHOICE      = 2,
        CAP_DIALOGUE_LOOP_EXIT_CHOICE = 15,
    };
    switch (task->state) {
        case CAP_DIALOGUE_LOOP_START:
            capRunCommandWithTransition(task->spawnArg1.value);
            task->state += 1;
            break;
        case CAP_DIALOGUE_LOOP_WAIT:
            if (capIsBusy() != 0) {
                break;
            }
            task->state += 1;
            break;
        case CAP_DIALOGUE_LOOP_CHOICE:
            if (capGetVariantKey() == CAP_DIALOGUE_LOOP_EXIT_CHOICE) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            } else {
                task->state = CAP_DIALOGUE_LOOP_START;
            }
            break;
    }
}
