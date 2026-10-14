/* Motel balcony sound cues, shared by the day and night room carriers. */

#ifndef MOTEL_BALCONY_CUE_SOUND_MSG
/// Selects the function identifier for the balcony sound-message definition.
///
/// The day balcony uses this default; the night balcony binds its own instance
/// before inclusion. The replacement must be one unparenthesized identifier.
/// First declare s32 handler(Task*, s32, s32, s32) in the carrier: static for
/// a private instance, or externally linked for one shared between overlay TUs.
/// The carrier supplies the task types and stage-relative sound API declarations.
/// This binding performs no calls or argument evaluation and is undefined after
/// emitting the definition, whether supplied by the carrier or defaulted here.
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
