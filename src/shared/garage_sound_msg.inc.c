/* Part of the garage library; see garage.h. */

/// Handles the garage's CAP sound cues for the current stage.
///
/// Installed for `ROOM_MESSAGE_SOUND` in both garage rooms. Cue 9 queues the
/// general-store bank's entry 9; cue 0x6C queries the CAP variant and discards
/// its result. Other cues do nothing. Always returns 0; receiver, message ID
/// and the second payload are unused.
static s32 _garageSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 unusedArg)
{
    enum { GARAGE_SOUND_CUE_PLAY          = 9,
           GARAGE_SOUND_CUE_QUERY_VARIANT = 0x6C };
    switch (cueKey) {
        case GARAGE_SOUND_CUE_PLAY:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 9), 0, 0);
            break;
        case GARAGE_SOUND_CUE_QUERY_VARIANT:
            capGetVariantKey();
            break;
    }
    return 0;
}
