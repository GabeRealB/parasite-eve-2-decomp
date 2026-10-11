/* Part of the parking lot library; see parking_lot.h. */

#ifndef PARKING_LOT_SOUND_MESSAGE_HANDLER
/// Function identifier for the included parking-lot sound-message definition.
///
/// Supply a single unparenthesized identifier, previously declared as
/// s32 (Task*, s32, s32, s32); that declaration establishes its C linkage.
/// The day carrier declares the default instance static in its prologue.
/// The night carrier overrides this binding before inclusion with the instance
/// declared in its overlay-private header and used by its room-message table.
/// The fragment undefines either binding after the definition; it is not restored.
#define PARKING_LOT_SOUND_MESSAGE_HANDLER _parkingLotSoundMsg

/// Queues parking-lot bank entry 9 or 10 for the corresponding room sound cue.
///
/// The current stage selects the loaded bank. Other cues do nothing;
/// always returns zero to `ROOM_MESSAGE_SOUND`. The other arguments are unused.
#endif
s32 PARKING_LOT_SOUND_MESSAGE_HANDLER(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
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
#undef PARKING_LOT_SOUND_MESSAGE_HANDLER
