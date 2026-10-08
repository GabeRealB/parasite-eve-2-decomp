/* Room sound cues shared by the day and night Dryfield toilets. */

/// Queues toilet-bank entry 5 for room sound cue 5.
///
/// The current stage selects the bank. Other cue keys do nothing; always
/// returns zero to `ROOM_MESSAGE_SOUND`. Requires the room's loaded sound bank.
static s32 _toiletSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum { TOILET_SOUND_CUE_5 = 5 };
    if (cueKey == TOILET_SOUND_CUE_5) {
        sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_TOILET, TOILET_SOUND_CUE_5), 0, 0);
    }
    return 0;
}
