/* Included underpass switch command handler; each carrier declares its static instance. */

/// Starts the underpass switch prompt selected by a CAP room command.
///
/// `ROOM_MESSAGE_COMMAND` supplies integer `switchId`: 1 asks CAP command 1
/// about switch 1, and 2 asks command 2 about switch 2. The deferred task
/// toggles the corresponding nibble for a retained CAP key of 10 or above.
/// Other keys do nothing. All other parameters are ignored; returns zero.
/// Spawn failure is unchecked. Keep the room and CAP file loaded for the task.
static s32 _underpassSwitchMsg(Task* unusedTask, s32 unusedMessageId, s32 switchId, s32 unusedSecondArg)
{
    enum {
        UNDERPASS_SWITCH_COMMAND_FIRST  = 1,
        UNDERPASS_SWITCH_COMMAND_SECOND = 2,
        UNDERPASS_SWITCH_TASK_INDEX     = 0,
    };
    switch (switchId) {
        case UNDERPASS_SWITCH_COMMAND_FIRST:
            taskSpawnFromTable(gUnderpassSwitchTaskDesc, UNDERPASS_SWITCH_TASK_INDEX, GAME_FLAG_UNDERPASS_SWITCH_1, UNDERPASS_SWITCH_COMMAND_FIRST);
            break;
        case UNDERPASS_SWITCH_COMMAND_SECOND:
            taskSpawnFromTable(gUnderpassSwitchTaskDesc, UNDERPASS_SWITCH_TASK_INDEX, GAME_FLAG_UNDERPASS_SWITCH_2, UNDERPASS_SWITCH_COMMAND_SECOND);
            break;
    }
    return 0;
}
