/* Part of the underpass switches library; see underpass_switches.h. */

/// Queues underpass-bank entry 2 for room sound cue 2.
///
/// The current stage selects the bank. Other cue keys do nothing; always
/// returns zero to `ROOM_MESSAGE_SOUND`. Requires the room's loaded sound bank.
static s32 _underpassSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum { UNDERPASS_SOUND_CUE_2 = 2 };
    if (cueKey == UNDERPASS_SOUND_CUE_2) {
        sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_UNDERPASS, UNDERPASS_SOUND_CUE_2), 0, 0);
    }
    return 0;
}
