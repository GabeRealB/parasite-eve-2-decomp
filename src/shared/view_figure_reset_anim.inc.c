/* Part of the view figure library; see view_figure.h. */

/// Sets the rate of animation slots 1..0x13 to 1 and resets each of them to
/// the current animation id, then records that id as the one now playing.
void viewFigureResetAnim(void)
{
    s32 i;

    i = 1;
    do {
        gViewFigureWork->rig.slots[i].rate = 1;
        animationResetSlot(&gViewFigureWork->rig.anim, i, gViewFigureWork->st.animId);
        i++;
    } while (i < 0x14);
    gViewFigureWork->st.appliedAnimId = gViewFigureWork->st.animId;
}
