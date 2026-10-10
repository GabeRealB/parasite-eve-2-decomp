/* Part of the view figure library; see view_figure.h. */

/// Applies a pending figure animation request or ticks its playing body tracks.
///
/// Uses the live published `_gViewFigureWork`, independently of the unread task
/// argument. BLEND captures and blends slots 1..19 over eight normal-rate
/// frames; RESET restarts them. Both select TICK and return before an ordinary
/// slot tick. TICK advances those tracks; other request states do nothing.
/// The twenty-part rig, loaded clips and pose storage must remain live, with
/// the scratch stack initialized for the animation helpers. Overwrites GTE state.
static void _viewFigureStepAnim(Task* task)
{
    if (_gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _viewFigureReseedAnim();
        _gViewFigureWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _viewFigureResetAnim();
        _gViewFigureWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _viewFigureTickAnim();
    }
}
