/* Included General Store flag prompt; each carrier declares its static instance. */

/// Runs a CAP prompt and toggles the requested flag for a variant key of 10 or above.
///
/// Start in state 0. `spawnArg1.value` is a valid game-flag nibble index
/// (0..503); `spawnArg2.value` is a command index in the loaded CAP table.
/// Both stay fixed through the task's lifetime. After CAP becomes idle, the
/// retained key selects whether to toggle zero to one or nonzero to zero.
/// A later tick releases the task. The room and CAP resources must stay loaded.
static void _generalStoreToggleFlagTask(Task* task)
{
    enum {
        GENERAL_STORE_TOGGLE_START       = 0,
        GENERAL_STORE_TOGGLE_WAIT        = 1,
        GENERAL_STORE_TOGGLE_APPLY       = 2,
        GENERAL_STORE_TOGGLE_FINISH      = 3,
        GENERAL_STORE_TOGGLE_KEY_MINIMUM = 10,
    };
    s32 flagId;
    s32 capCommand;

    flagId     = task->spawnArg1.value;
    capCommand = task->spawnArg2.value;
    switch (task->state) {
        case GENERAL_STORE_TOGGLE_START:
            capRunCommandWithTransition(capCommand);
            task->state = task->state + 1;
            break;
        case GENERAL_STORE_TOGGLE_WAIT:
            if (capIsBusy() != 0) {
                break;
            }
            task->state = task->state + 1;
            break;
        case GENERAL_STORE_TOGGLE_APPLY:
            if (capGetVariantKey() >= GENERAL_STORE_TOGGLE_KEY_MINIMUM) {
                gameFlagSetNibble(flagId, gameFlagGetNibble(flagId) == 0);
            }
            task->state = task->state + 1;
            break;
        case GENERAL_STORE_TOGGLE_FINISH:
            taskKill(task);
            break;
    }
}
