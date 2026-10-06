/* Part of the view figure library; see view_figure.h. */

/// Advances the animation per the work block's `st.state`: step 1 reseeds the
/// slots through `animationSeekSlotWithBlend`, step 2 resets them outright, and either moves
/// on to step 3, which ticks them. The argument is never read.
void viewFigureStepAnim(Task* task)
{
    if (gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        viewFigureReseedAnim();
        gViewFigureWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        viewFigureResetAnim();
        gViewFigureWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _viewFigureTickAnim();
    }
}
