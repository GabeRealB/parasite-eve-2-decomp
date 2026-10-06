/* Part of the Dryfield main street library; see main_street.h. */

/// Plays the stage sound a CAP script cue asks for: cues 8, 9 and 0xC play
/// their own sound (9 also plays 0xC's), and cues 0x65 and 0x78 play sound
/// 0xD when the event key is 1 and 0 respectively.
s32 mainStreetCapSoundCue(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 8), 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 9), 0, 0);
            /* fallthrough */
        case 0xC:
            Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 0x0C), 0, 0);
            break;
        case 0x65:
            if (capGetVariantKey() == 1) {
                Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 0x0D), 0, 0);
            }
            break;
        case 0x78:
            if (capGetVariantKey() == 0) {
                Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_MAIN_STREET, 0x0D), 0, 0);
            }
            break;
    }
    return 0;
}
