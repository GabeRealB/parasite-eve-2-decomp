/* Part of the water tower library; see water_tower.h. */

/// The room's handler for message 0x13F2: plays the stage sound for script
/// events 8 and 13 and answers 0 for every event.
s32 waterTowerSoundMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 8:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 8), 0, 0);
            break;
        case 13:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 0x0D), 0, 0);
            break;
    }
    return 0;
}
