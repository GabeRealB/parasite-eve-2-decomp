/* Part of the Dryfield main street library; see main_street.h. */

/// Queues the main-street sound scripts selected by a room sound cue.
///
/// Cue 9 queues both entries 9 and 12. Cues 101 and 120 queue entry 13 only
/// for CAP variant keys 1 and 0 respectively. Cue 8 or 12 queues its own entry;
/// other keys do nothing. The current stage selects the loaded main-street
/// bank. Handles `ROOM_MESSAGE_SOUND` synchronously and always returns zero.
static s32 _mainStreetCapSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum {
        MAIN_STREET_SOUND_CUE_8             = 8,
        MAIN_STREET_SOUND_CUE_9             = 9,
        MAIN_STREET_SOUND_CUE_12            = 12,
        MAIN_STREET_SOUND_CUE_CAP_101       = 101,
        MAIN_STREET_SOUND_CUE_CAP_120       = 120,
        MAIN_STREET_SOUND_CAP_VARIANT_0     = 0,
        MAIN_STREET_SOUND_CAP_VARIANT_1     = 1,
        MAIN_STREET_SOUND_CONDITIONAL_ENTRY = 13,
    };
    switch (cueKey) {
        case MAIN_STREET_SOUND_CUE_8:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, MAIN_STREET_SOUND_CUE_8), 0, 0);
            break;
        case MAIN_STREET_SOUND_CUE_9:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, MAIN_STREET_SOUND_CUE_9), 0, 0);
            /* fallthrough */
        case MAIN_STREET_SOUND_CUE_12:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, MAIN_STREET_SOUND_CUE_12), 0, 0);
            break;
        case MAIN_STREET_SOUND_CUE_CAP_101:
            if (capGetVariantKey() == MAIN_STREET_SOUND_CAP_VARIANT_1) {
                sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, MAIN_STREET_SOUND_CONDITIONAL_ENTRY), 0, 0);
            }
            break;
        case MAIN_STREET_SOUND_CUE_CAP_120:
            if (capGetVariantKey() == MAIN_STREET_SOUND_CAP_VARIANT_0) {
                sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, MAIN_STREET_SOUND_CONDITIONAL_ENTRY), 0, 0);
            }
            break;
    }
    return 0;
}
