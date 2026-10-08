/* Part of the cellar library; see cellar.h. */

/// Selects the cellar's P08 dialogue in response to ROOM_MESSAGE_COMMAND.
///
/// Command 13 chooses an introductory CAP command while progress is below 2,
/// otherwise the acquisition or already-owned command according to the saved
/// P08 limit. Command 8 and every other command do nothing. All replies are zero;
/// the task, message ID and second payload are ignored. CAP resources must be live.
static s32 _cellarHandleCommand(Task* task, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        CELLAR_COMMAND_NO_ACTION    = 8,
        CELLAR_COMMAND_P08_DIALOGUE = 13,
        CELLAR_P08_ITEM_ID          = 0x83,
        CELLAR_P08_PROGRESS_READY   = 2,
        CELLAR_CAP_P08_INTRO        = 13,
        CELLAR_CAP_P08_AVAILABLE    = 14,
        CELLAR_CAP_P08_OWNED        = 4,
    };
    if (commandId != CELLAR_COMMAND_NO_ACTION) {
        if (commandId == CELLAR_COMMAND_P08_DIALOGUE) {
            if (gameFlagGetNibble(GAME_FLAG_11B) >= CELLAR_P08_PROGRESS_READY) {
                if (inventoryIsItemLimitReached(CELLAR_P08_ITEM_ID) == 0) {
                    capRunCommandWithTransition(CELLAR_CAP_P08_AVAILABLE);
                } else {
                    capRunCommandWithTransition(CELLAR_CAP_P08_OWNED);
                }
            } else {
                capRunCommandWithTransition(CELLAR_CAP_P08_INTRO);
            }
        }
    }
    return 0;
}
