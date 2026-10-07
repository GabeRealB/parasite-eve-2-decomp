/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Records a room-command request for the Stalker's scripted entrances.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with a borrowed non-null command;
/// ignores context, `messageId` and `unusedArg`. Commands 0..3 latch kinds
/// 1..4; others preserve the previous request. Only Zebra reads the latch.
/// Returns void in the target ABI: senders must ignore the dispatch result.
static void _stalkerZebraIvoryApplyRoomCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        STALKER_ZEBRA_IVORY_ROOM_COMMAND_0 = 0,
        STALKER_ZEBRA_IVORY_ROOM_COMMAND_1 = 1,
        STALKER_ZEBRA_IVORY_ROOM_COMMAND_2 = 2,
        STALKER_ZEBRA_IVORY_ROOM_COMMAND_3 = 3,
        STALKER_ZEBRA_IVORY_ROOM_REQUEST_0 = 1,
        STALKER_ZEBRA_IVORY_ROOM_REQUEST_1 = 2,
        STALKER_ZEBRA_IVORY_ROOM_REQUEST_2 = 3,
        STALKER_ZEBRA_IVORY_ROOM_REQUEST_3 = 4
    };
    StalkerZebraIvoryWork* work = task->work;

    switch (command->command) {
        case STALKER_ZEBRA_IVORY_ROOM_COMMAND_0:
            work->roomCommand = STALKER_ZEBRA_IVORY_ROOM_REQUEST_0;
            break;
        case STALKER_ZEBRA_IVORY_ROOM_COMMAND_1:
            work->roomCommand = STALKER_ZEBRA_IVORY_ROOM_REQUEST_1;
            break;
        case STALKER_ZEBRA_IVORY_ROOM_COMMAND_2:
            work->roomCommand = STALKER_ZEBRA_IVORY_ROOM_REQUEST_2;
            break;
        case STALKER_ZEBRA_IVORY_ROOM_COMMAND_3:
            work->roomCommand = STALKER_ZEBRA_IVORY_ROOM_REQUEST_3;
            break;
    }
}
