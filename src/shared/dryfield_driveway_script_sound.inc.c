/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Script-event hook: events 8 and 10 each queue their stage sound; every
/// event returns 0.
s32 drivewayScriptSound(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 8:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 8), 0, 0);
            break;
        case 10:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_DRIVEWAY, 0x0A), 0, 0);
            break;
    }
    return 0;
}
