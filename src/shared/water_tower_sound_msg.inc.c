/* Part of the water tower library; see water_tower.h. */

s32 waterTowerSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 unusedArg)
{
    enum {
        WATER_TOWER_SOUND_CUE_8  = 8,
        WATER_TOWER_SOUND_CUE_13 = 13
    };

    switch (cueKey) {
        case WATER_TOWER_SOUND_CUE_8:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, WATER_TOWER_SOUND_CUE_8), 0, 0);
            break;
        case WATER_TOWER_SOUND_CUE_13:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, WATER_TOWER_SOUND_CUE_13), 0, 0);
            break;
    }
    return 0;
}
