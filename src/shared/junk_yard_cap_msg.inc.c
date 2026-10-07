/* Part of the junk yard library; see junk_yard.h. */

/// Handler for message 0x13F0 in the room's message table, keyed by `arg2`.
/// Point 6 plays CAP command 0xC until nibble 0x3A is set, and 6 after. Point 8
/// plays command 9 unless bit flag 0x1C is set; with it set, command 8 plays
/// only while nibble 0x73 is still clear and 0x7C is set, and otherwise the
/// point's own CAP slot starts. Always returns 0.
s32 junkYardCapMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 6:
            Gp_RunCapCmd1(gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) <= 0 ? 0xC : 6);
            break;
        case 8:
            if (areaGetCurrentObjectState(0x1C) == 1) {
                if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) == 0 && gameFlagGetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN) != 0) {
                    Gp_RunCapCmd1(8);
                } else {
                    capStartSequenceSlot(arg2, 1, 0);
                }
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}
