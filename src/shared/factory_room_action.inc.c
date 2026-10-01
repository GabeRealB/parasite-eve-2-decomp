/* Part of the factory lift library; see factory_lift.h. */

/// Message handler: the first message with `actionId` 1 while game flag 0x2C is
/// clear starts cap 0xB, sets the flag and plays sound 0x5217000A.
s32 factoryRoomAction(Task* task, s32 msgId, TaskMessageArg firstArg, TaskMessageArg arg3)
{
    const DirectionActionRequest* request = firstArg.pointer;

    if ((request->actionId == 1) && (GameFlag_GetNibble(0x2C) == 0)) {
        Gp_SpawnIfCapIdle(0xB, 1);
        GameFlag_SetNibble(0x2C, 1);
        func_800E3FAC(0xA2, 0xA);
        SndEvt_EnqueueType6(0x5217000A, 0, 0);
    }
    return 0;
}
