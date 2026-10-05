/* Part of the Dryfield main street library; see main_street.h. */

/// Acts only on `arg2 == 1`. Once nibble 0x7B has reached 2 it ages flag
/// 0x119, sets current-bit flag 0x1B unless collected bit 0x119 is held,
/// spawns CAP entry 1 and the task above; before that it spawns CAP entry
/// 0x14 instead.
s32 mainStreetTalkMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= 2) {
            Gp_AgeFlag119();
            if (Gp_HasCollectedBit(0x119) == 0) {
                Gp_SetCurBit2Flag(0x1B, 1);
            }
            Gp_SpawnIfCapIdle(1, 1);
            taskSpawnFromTable(&gMainStreetPlayTimeTaskDesc, 0, 0, 0);
        } else {
            Gp_SpawnIfCapIdle(0x14, 1);
        }
    }
    return 0;
}
