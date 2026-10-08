/* Included General Store command handler; each carrier declares its static instance. */

/// Handles the General Store's CAP commands for companion dialogue and flag toggling.
///
/// `ROOM_MESSAGE_COMMAND` supplies integer `commandId`: 24 queues CAP command
/// 24 with a companion or 25 without one, only while CAP is idle; 9 requests
/// the flag-toggle prompt task. Other values do nothing. All other parameters
/// are ignored, and every command returns zero. Spawn failure is unchecked;
/// the room and its CAP resources must remain loaded for the deferred tasks.
static s32 _generalStoreCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        GENERAL_STORE_COMMAND_COMPANION_DIALOGUE = 24,
        GENERAL_STORE_CAP_COMPANION_ABSENT       = 25,
        GENERAL_STORE_COMMAND_TOGGLE_FLAG        = 9,
        GENERAL_STORE_CAP_TOGGLE_FLAG            = 9,
        GENERAL_STORE_TOGGLE_FLAG_TASK_INDEX     = 0,
    };
    s32   capCommand;
    Task* companionTask;

    if (commandId == GENERAL_STORE_COMMAND_COMPANION_DIALOGUE) {
        companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
        capCommand    = GENERAL_STORE_CAP_COMPANION_ABSENT;
        if (companionTask != NULL) {
            capCommand = GENERAL_STORE_COMMAND_COMPANION_DIALOGUE;
        }
        capSpawnEventIfIdle(capCommand, CAP_EVENT_NO_FLAGS);
    }
    if (commandId == GENERAL_STORE_COMMAND_TOGGLE_FLAG) {
        taskSpawnFromTable(gStoreTaskDescs, GENERAL_STORE_TOGGLE_FLAG_TASK_INDEX, GAME_FLAG_053, GENERAL_STORE_CAP_TOGGLE_FLAG);
    }
    return 0;
}
