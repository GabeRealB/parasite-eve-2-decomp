/* Part of the paced walk library; see paced_walk.h. */

/// Latches scripted smoking on the paced walker for command word 1.
///
/// Requires live PacedWalkWork and a borrowed readable command through this
/// synchronous call. Other command words leave the latch unchanged; no command
/// clears it. The message ID and trailing argument are ignored. Returns zero.
static s32 _pacedWalkStartSmoking(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum { PACED_WALK_COMMAND_START_SMOKING = 1 };
    PacedWalkWork* work;
    u16            commandValue;

    commandValue = command->command;
    work         = task->work;
    if (commandValue == PACED_WALK_COMMAND_START_SMOKING) {
        work->smoking = commandValue;
    }
    return 0;
}
