/* Part of the view figure library; see view_figure.h. */

/// Advances the animation per the work block's `st.state`: step 1 reseeds the
/// slots through `func_800B4114`, step 2 resets them outright, and either moves
/// on to step 3, which ticks them. The argument is never read.
void viewFigureStepAnim(Task* task)
{
    if (gViewFigureWork->st.state == 1) {
        viewFigureReseedAnim();
        gViewFigureWork->st.state = 3;
        return;
    }
    if (gViewFigureWork->st.state == 2) {
        viewFigureResetAnim();
        gViewFigureWork->st.state = 3;
        return;
    }
    if (gViewFigureWork->st.state == 3) {
        viewFigureTickAnim();
    }
}
