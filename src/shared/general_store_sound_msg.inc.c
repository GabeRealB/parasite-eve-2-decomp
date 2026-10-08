/* Included sound handler for the paired General Store rooms. */

/// Queues general-store bank entry 7 for room sound cue 7.
///
/// The current stage selects the bank. Other cue keys do nothing; always
/// returns zero to `ROOM_MESSAGE_SOUND`. Requires the room's loaded sound bank.
static s32 _generalStoreSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum { GENERAL_STORE_SOUND_CUE_7 = 7 };
    if (cueKey == GENERAL_STORE_SOUND_CUE_7) {
        sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, GENERAL_STORE_SOUND_CUE_7), 0, 0);
    }
    return 0;
}
