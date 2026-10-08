/* Part of the trailer coach library; see trailer_coach.h. */

/// Applies the trailer coach's view-dependent ordering-table depth scale.
///
/// Uses the live save's view: day view 8 or night view 5 selects 1x, all other
/// views select 8x. Runs once per room-task update without advancing task state.
static void _trailerCoachSetDepthShift(Task* unusedTask)
{
    // Retain the unused local storage present in this leaf's 16-byte stack frame.
    char unusedStackSpace[16];

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == TRAILER_COACH_FINE_DEPTH_VIEW) {
        gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_1X;
    } else {
        gDisplayState.otDepthShift = DISPLAY_DEPTH_SHIFT_8X;
    }
}
