/* Part of the room variants library; see room_variants.h. */

/// Plays stage sound 0x521D0008 or 0x521D0009 for events 8 and 9; always
/// answers 0.
s32 roomVariantMotelBalconySoundMsg(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 8), 0, 0);
            break;
        case 0x9:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MOTEL_BALCONY, 9), 0, 0);
            break;
    }
    return 0;
}
