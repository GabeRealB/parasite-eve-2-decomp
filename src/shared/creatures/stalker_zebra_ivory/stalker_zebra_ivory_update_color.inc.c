/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Updates body lighting and colour from part 1's cached translation.
///
/// Requires a live model with at least two coordinates and its Enemy spawn
/// argument. Copies cached XYZ without refreshing the part; the lighting
/// query interprets the sample as world coordinates. Reserves one VECTOR on
/// the initialized scratch stack, plus the lighting query's nested workspace,
/// and releases it before returning. No sample pointer survives the call.
static void _stalkerZebraIvoryUpdateColor(Task* task)
{
    GfxCoord* sampleCoord;
    void**    scratchCursor;
    u8*       scratchTop;
    VECTOR*   samplePosition;

    sampleCoord                                  = &task->extra.tmd->coords[1];
    scratchCursor                                = SCRATCH_STACK_CURSOR_SLOT;
    scratchTop                                   = SCRATCH_STACK_CURSOR_AT(scratchCursor, void);
    samplePosition                               = (VECTOR*)(scratchTop - sizeof(VECTOR));
    samplePosition->vx                           = sampleCoord->workm.t[0];
    samplePosition->vy                           = sampleCoord->workm.t[1];
    samplePosition->vz                           = sampleCoord->workm.t[2];
    SCRATCH_STACK_CURSOR_AT(scratchCursor, void) = samplePosition;
    worldCoordUpdateActorColor(task->spawnArg2.pointer, samplePosition, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchCursor, sizeof(VECTOR));
}

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
