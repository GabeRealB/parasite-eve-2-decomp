/* Part of the view figure library; see view_figure.h. */

/// Advances the published figure's nineteen animated parts and writes their poses.
///
/// Requires live `gViewFigureWork` with its context bound to the twenty-part
/// model and initialized slots 1 through 19. Slot 0 is the root controlled by
/// view placement. Keep the model, clip data and pose storage live; scratch
/// stack and GTE requirements are those of `animationTickSlot`.
static void _viewFigureTickAnim(void)
{
    enum { VIEW_FIGURE_FIRST_ANIM_SLOT = 1 };

    s32 slotIndex;

    for (slotIndex = VIEW_FIGURE_FIRST_ANIM_SLOT; slotIndex < ARRAY_SIZE(gViewFigureWork->rig.slots); slotIndex++) {
        animationTickSlot(&gViewFigureWork->rig.anim, slotIndex);
    }
}
