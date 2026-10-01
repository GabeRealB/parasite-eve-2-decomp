/* Part of the water tower library; see water_tower.h. */

/// The room's handler for message 0x13F2: plays the stage sound for script
/// events 8 and 13 and answers 0 for every event.
s32 waterTowerSoundMsg(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52140008, 0, 0);
            break;
        case 13:
            Gp_EnqueueStageSnd6(0x5214000D, 0, 0);
            break;
    }
    return 0;
}
