/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Queues CD command 0x21 once, guarded by `gSceneCombatState.enemySoundBankQueued`: the first parameter
/// is 2 or 3 in place 1 or 2 of stage 4 areas 0x27/0x28 and 1 everywhere
/// else.
void madChaserLoadSoundBank(void)
{
    u8 param1[8];
    u8 param2[8];

    if (gSceneCombatState.enemySoundBankQueued == 0) {
        /* Each branch makes its own call; jump2's cross-jumping merges the
         * identical tails after sched2, which is why the argument setup is
         * duplicated per branch in the target. */
        if (gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER && (u32)(gGameSession->location.loc.area - 0x27) < 2 && gGameSession->location.loc.variant == 1) {
            param1[2] = 0xA;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        } else if (gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER && (u32)(gGameSession->location.loc.area - 0x27) < 2 && gGameSession->location.loc.variant == 2) {
            param1[2] = 0xA;
            param1[0] = 3;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        } else {
            param1[2] = 0xA;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
        }
        gSceneCombatState.enemySoundBankQueued = 1;
    }
}
