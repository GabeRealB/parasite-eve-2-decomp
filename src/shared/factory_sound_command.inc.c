/* Part of the factory lift library; see factory_lift.h. */

/// Message handler: command 7 plays sound 0x52170007, and command 21 plays
/// 0x52170015 and sets game flag 0x4A to 2.
s32 factorySoundCommand(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 7:
            Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 7), 0, 0);
            break;
        case 21:
            Gp_EnqueueStageSnd6(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x15), 0, 0);
            gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, 2);
            break;
    }
    return 0;
}
