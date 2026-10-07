/* Part of the view figure library; see view_figure.h. */

/// Captures the figure's body poses and blends each track to the requested clip start.
///
/// Requires live `gViewFigureWork` with initialized slots 1..19 of its twenty-
/// part rig and a loaded `st.animId` set supporting their current encodings.
/// Slot 0 stays under view placement. Each driven slot first ticks and captures
/// its pose, then blends to track-relative record zero over eight normal-rate
/// frames while retaining its rate and capture status. Records `appliedAnimId`
/// after all slots are sought. Borrowed clip and pose-buffer storage must remain
/// live throughout playback; scratch and GTE requirements are those of
/// `animationSeekSlotWithBlend`.
static void _viewFigureReseedAnim(void)
{
    enum { VIEW_FIGURE_RESEED_FIRST_SLOT   = 1,
           VIEW_FIGURE_RESEED_BLEND_FRAMES = 8 };
    s32 slotIndex;

    for (slotIndex = VIEW_FIGURE_RESEED_FIRST_SLOT; slotIndex < ARRAY_SIZE(gViewFigureWork->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&gViewFigureWork->rig.anim, slotIndex, gViewFigureWork->st.animId, 0, VIEW_FIGURE_RESEED_BLEND_FRAMES);
    }
    gViewFigureWork->st.appliedAnimId = gViewFigureWork->st.animId;
}
