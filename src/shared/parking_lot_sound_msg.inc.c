/* Part of the parking lot library; see parking_lot.h. */

/// Selects the carrier-declared s32 (Task*, s32, s32, s32) sound-cue callback.
///
/// Defaults to the day room's static instance. The split night overlay binds
/// `parkingLotSoundMsg`, declared in its private header, around this inclusion.
/// The replacement is a single function identifier without runtime evaluation.
#ifndef PARKING_LOT_SOUND_MSG
#define PARKING_LOT_SOUND_MSG _parkingLotSoundMsg

/// Queues parking-lot bank entry 9 or 10 for the corresponding room sound cue.
///
/// The current stage selects the loaded bank. Other cues do nothing;
/// always returns zero to `ROOM_MESSAGE_SOUND`. The other arguments are unused.
#endif
s32 PARKING_LOT_SOUND_MSG(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum {
        PARKING_LOT_SOUND_CUE_9  = 9,
        PARKING_LOT_SOUND_CUE_10 = 10,
    };
    switch (cueKey) {
        case PARKING_LOT_SOUND_CUE_9:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, PARKING_LOT_SOUND_CUE_9), 0, 0);
            break;
        case PARKING_LOT_SOUND_CUE_10:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, PARKING_LOT_SOUND_CUE_10), 0, 0);
            break;
    }
    return 0;
}
#undef PARKING_LOT_SOUND_MSG
