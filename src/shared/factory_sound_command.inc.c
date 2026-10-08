/* Part of the factory lift library; see factory_lift.h. */

s32 factorySoundCommand(Task* task, s32 messageId, s32 soundCue, s32 secondArg)
{
    enum { FACTORY_SOUND_CUE_7                    = 7,
           FACTORY_SOUND_CUE_LAMP_SECOND_POSITION = 21,
           FACTORY_SOUND_LAMP_SECOND_POSITION     = 2 };

    switch (soundCue) {
        case FACTORY_SOUND_CUE_7:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, FACTORY_SOUND_CUE_7), 0, 0);
            break;
        case FACTORY_SOUND_CUE_LAMP_SECOND_POSITION:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, FACTORY_SOUND_CUE_LAMP_SECOND_POSITION), 0, 0);
            gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, FACTORY_SOUND_LAMP_SECOND_POSITION);
            break;
    }
    return 0;
}
