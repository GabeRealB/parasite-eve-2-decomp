/* Part of the factory lift library; see factory_lift.h. */

/// Message handler: the first message with `actionId` 1 while game flag 0x2C is
/// clear starts cap 0xB, sets the flag and plays sound 0x5217000A.
s32 factoryRoomAction(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if ((request->actionId == 1) && (gameFlagGetNibble(GAME_FLAG_02C) == 0)) {
        capSpawnEventIfIdle(0xB, CAP_EVENT_PAUSE_ACTORS);
        gameFlagSetNibble(GAME_FLAG_02C, 1);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0xA);
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0A), 0, 0);
    }
    return 0;
}
