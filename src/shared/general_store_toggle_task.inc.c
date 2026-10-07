/* Part of the general store library; see general_store.h. */

/// A task that runs cap command `spawnArg2` and waits for it to finish; if the
/// cap then reports an event key of 0xA or above, it toggles game-flag nibble
/// `spawnArg1` between 0 and 1. The task then kills itself.
void storeToggleTask(Task* task)
{
    s32 flag;
    s32 cmd;

    flag = task->spawnArg1.value;
    cmd  = task->spawnArg2.value;
    switch (task->state) {
        case 0:
            capRunCommandWithTransition(cmd);
            task->state = task->state + 1;
            break;
        case 1:
            if (capIsBusy() != 0) {
                break;
            }
            task->state = task->state + 1;
            break;
        case 2:
            if (capGetVariantKey() >= 0xA) {
                gameFlagSetNibble(flag, gameFlagGetNibble(flag) == 0);
            }
            task->state = task->state + 1;
            break;
        case 3:
            taskKill(task);
            break;
    }
}
