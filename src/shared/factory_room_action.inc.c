/* Part of the factory lift library; see factory_lift.h. */

s32 factoryRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { FACTORY_ACTION_START_SCENE = 1,
           FACTORY_ACTION_CAP_COMMAND = 11,
           FACTORY_ACTION_OBJECTIVE   = 10 };

    if ((request->actionId == FACTORY_ACTION_START_SCENE) && (gameFlagGetNibble(GAME_FLAG_02C) == 0)) {
        capSpawnEventIfIdle(FACTORY_ACTION_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
        gameFlagSetNibble(GAME_FLAG_02C, 1);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, FACTORY_ACTION_OBJECTIVE);
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0A), 0, 0);
    }
    return 0;
}
