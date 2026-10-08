/* Part of the junk yard library; see junk_yard.h. */

/// Selects progress-dependent junk-yard dialogue for room commands 6 and 8.
///
/// Handles `ROOM_MESSAGE_COMMAND`; the receiver, message ID and second payload
/// are unused. Command 6 selects dialogue from driveway progress. Command 8
/// selects from object state and the night story flags, falling back to its
/// CAP slot with variant 0. Requires the room's relocated CAP table to remain
/// loaded through playback. Unsupported commands also return 0.
static s32 _junkYardCommandMessage(Task* task, s32 messageId, s32 command, s32 unusedSecondArg)
{
    enum {
        JUNK_YARD_COMMAND_PROGRESS_DIALOGUE    = 6,
        JUNK_YARD_COMMAND_NIGHT_DIALOGUE       = 8,
        JUNK_YARD_CAP_BEFORE_DRIVEWAY_PROGRESS = 12,
        JUNK_YARD_CAP_AFTER_DRIVEWAY_PROGRESS  = 6,
        JUNK_YARD_CAP_NIGHT_BEFORE_BURNER      = 8,
        JUNK_YARD_CAP_NIGHT_OBJECT_UNSET       = 9,
        JUNK_YARD_NIGHT_DIALOGUE_OBJECT_SLOT   = 28,
    };
    switch (command) {
        case JUNK_YARD_COMMAND_PROGRESS_DIALOGUE:
            capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) <= 0 ? JUNK_YARD_CAP_BEFORE_DRIVEWAY_PROGRESS : JUNK_YARD_CAP_AFTER_DRIVEWAY_PROGRESS);
            break;
        case JUNK_YARD_COMMAND_NIGHT_DIALOGUE:
            if (areaGetCurrentObjectState(JUNK_YARD_NIGHT_DIALOGUE_OBJECT_SLOT) == 1) {
                if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) == 0 && gameFlagGetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN) != 0) {
                    capRunCommandWithTransition(JUNK_YARD_CAP_NIGHT_BEFORE_BURNER);
                } else {
                    capStartSequenceSlot(command, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                }
            } else {
                capRunCommandWithTransition(JUNK_YARD_CAP_NIGHT_OBJECT_UNSET);
            }
            break;
    }
    return 0;
}
