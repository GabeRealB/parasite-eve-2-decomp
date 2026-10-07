/* Motel balcony sound cues, shared by the day and night room carriers. */

/// Selects the carrier-declared s32 (Task*, s32, s32, s32) sound-cue handler.
///
/// The day balcony uses the private default. The night balcony binds its
/// overlay-private `motelBalconyCueSoundMsg` around this fragment.
/// The replacement is one function identifier, without argument evaluation.
#ifndef MOTEL_BALCONY_CUE_SOUND_MSG
#define MOTEL_BALCONY_CUE_SOUND_MSG _motelBalconyCueSoundMsg

/// Queues the balcony sound script selected by CAP cue 8 or 9.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue keys do nothing. The current
/// session supplies the stage for balcony-bank entries 8 and 9. Returns 0;
/// `task`, `messageId` and `secondArg` are unused. Requires gameplay and the
/// loaded balcony sound bank; queue admission follows the sound request API.
#endif
s32 MOTEL_BALCONY_CUE_SOUND_MSG(Task* task, s32 messageId, s32 cueKey, s32 secondArg)
{
    enum {
        MOTEL_BALCONY_SOUND_CUE_8 = 8,
        MOTEL_BALCONY_SOUND_CUE_9 = 9,
    };
    switch (cueKey) {
        case MOTEL_BALCONY_SOUND_CUE_8:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, MOTEL_BALCONY_SOUND_CUE_8), 0, 0);
            break;
        case MOTEL_BALCONY_SOUND_CUE_9:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, MOTEL_BALCONY_SOUND_CUE_9), 0, 0);
            break;
    }
    return 0;
}
#undef MOTEL_BALCONY_CUE_SOUND_MSG
