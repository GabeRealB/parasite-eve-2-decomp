/* Part of the view figure library; see view_figure.h. */

/// Restarts the published figure's nineteen body tracks at normal playback rate.
///
/// Requires live `_gViewFigureWork`, a context bound to its twenty-part rig and
/// a loaded `st.animId` set supporting slots 1..19. Slot 0 remains under view
/// placement. Resets each driven slot's track, encoding and playback flags,
/// then records `appliedAnimId`; no pose is ticked or written by the reset.
/// The model, set data, slots and pose buffers remain borrowed for playback.
static void _viewFigureResetAnim(void)
{
    enum { VIEW_FIGURE_RESET_PRESET_RATE = 1,
           VIEW_FIGURE_RESET_FIRST_SLOT  = 1 };
    s32 slotIndex;

    for (slotIndex = VIEW_FIGURE_RESET_FIRST_SLOT; slotIndex < ARRAY_SIZE(_gViewFigureWork->rig.slots); slotIndex++) {
        // The reset replaces this preliminary rate with ANIMATION_RATE_ONE.
        _gViewFigureWork->rig.slots[slotIndex].rate = VIEW_FIGURE_RESET_PRESET_RATE;
        animationResetSlot(&_gViewFigureWork->rig.anim, slotIndex, _gViewFigureWork->st.animId);
    }
    _gViewFigureWork->st.appliedAnimId = _gViewFigureWork->st.animId;
}
