/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Queues the driveway's two CAP sound cues for the current stage.
///
/// Handles `ROOM_MESSAGE_SOUND` in both driveway rooms. Cue keys 8 and 10
/// select their same-numbered daytime sound entries; the sound API resolves
/// the current stage. Other cues do nothing. Always returns 0; receiver, ID
/// and the second payload are unused.
static s32 _drivewayScriptSound(Task* task, s32 messageId, s32 cueKey, s32 unusedArg)
{
    enum { DRIVEWAY_SOUND_CUE_8  = 8,
           DRIVEWAY_SOUND_CUE_10 = 10 };
    switch (cueKey) {
        case DRIVEWAY_SOUND_CUE_8:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 8), 0, 0);
            break;
        case DRIVEWAY_SOUND_CUE_10:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 0x0A), 0, 0);
            break;
    }
    return 0;
}
