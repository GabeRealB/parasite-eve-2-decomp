/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Queues the Mad Chaser sound-bank bundle once for the current encounter.
///
/// Uses global-CDF group 10, suffix 44, file 1 by default; Shelter B3 dumping
/// hole and garbage incinerator variants 1 and 2 select files 2 and 3.
/// The scene's shared enemy-bank latch is set when queued, before load completion.
/// Requires a live session and room in the CD queue; argument bytes are copied
/// synchronously, so the local blocks need not survive the call.
static void _madChaserQueueSoundBank(void)
{
    enum {
        MAD_CHASER_SOUND_FILE_GROUP     = 10,
        MAD_CHASER_SOUND_FILE_SUFFIX    = 44,
        MAD_CHASER_SOUND_FILE_DEFAULT   = 1,
        MAD_CHASER_SOUND_FILE_VARIANT_1 = 2,
        MAD_CHASER_SOUND_FILE_VARIANT_2 = 3,
    };
    u8 fileKeyBytes[4];
    u8 loadArgs[sizeof(((CdCmdEntry*)0)->args.bytes)];

    if (gSceneCombatState.enemySoundBankQueued == 0) {
        // The queue copies each selected file key and its load options immediately.
        if (gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER && (u32)(gGameSession->location.loc.area - GAME_AREA_SHELTER_B3_DUMPING_HOLE) < 2 && gGameSession->location.loc.variant == 1) {
            fileKeyBytes[2] = MAD_CHASER_SOUND_FILE_GROUP;
            fileKeyBytes[0] = MAD_CHASER_SOUND_FILE_VARIANT_1;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = MAD_CHASER_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        } else if (gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER && (u32)(gGameSession->location.loc.area - GAME_AREA_SHELTER_B3_DUMPING_HOLE) < 2 && gGameSession->location.loc.variant == 2) {
            fileKeyBytes[2] = MAD_CHASER_SOUND_FILE_GROUP;
            fileKeyBytes[0] = MAD_CHASER_SOUND_FILE_VARIANT_2;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = MAD_CHASER_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        } else {
            fileKeyBytes[2] = MAD_CHASER_SOUND_FILE_GROUP;
            fileKeyBytes[0] = MAD_CHASER_SOUND_FILE_DEFAULT;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = MAD_CHASER_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        }
        gSceneCombatState.enemySoundBankQueued = 1;
    }
}
