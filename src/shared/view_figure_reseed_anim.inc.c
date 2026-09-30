/* Part of the view figure library; see view_figure.h. */

/// Reseeds animation slots 1..0x13 of the work block's animation context from
/// the current animation id through `func_800B4114` (arguments 0 and 8), then
/// records that id as the one now playing.
void viewFigureReseedAnim(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&gViewFigureWork->rig.anim, i, (s16)gViewFigureWork->st.animId, 0, 8);
        i++;
    } while (i < 0x14);
    gViewFigureWork->st.appliedAnimId = gViewFigureWork->st.animId;
}
