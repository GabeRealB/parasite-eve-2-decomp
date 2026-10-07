/* Part of the parking lot library; see parking_lot.h. */

/// Handler for message 0x13F2 in the room's message table, keyed by `arg2`:
/// point 9 plays stage sound 0x520F0009 and point 10 plays 0x520F000A. Always
/// returns 0.
s32 parkingLotSoundMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 9:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 9), 0, 0);
            break;
        case 10:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_PARKING_LOT, 0x0A), 0, 0);
            break;
    }
    return 0;
}
