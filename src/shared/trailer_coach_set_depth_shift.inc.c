/* Part of the trailer coach library; see trailer_coach.h. */

/// Per-frame: the ordering table's depth shift is 1x in the view
/// TRAILER_COACH_FINE_DEPTH_VIEW and 8x everywhere else. `pad` reserves the
/// stack the original frame has.
void trailerCoachSetDepthShift(Task* task)
{
    char pad[0x10];

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == TRAILER_COACH_FINE_DEPTH_VIEW) {
        gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
    } else {
        gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_8X;
    }
}
