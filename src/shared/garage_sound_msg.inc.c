/* Part of the garage library; see garage.h. */

/// Handler for message 0x13F2 in the room's message table: on event 9 it plays
/// stage sound 0x52030009, on event 0x6C it reads the cap event key, and it
/// always reports the message as not handled.
s32 garageSoundMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x9:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GENERAL_STORE, 9), 0, 0);
            break;
        case 0x6C:
            capGetVariantKey();
            break;
    }
    return 0;
}
