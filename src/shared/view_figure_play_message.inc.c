/* Part of the view figure library; see view_figure.h. */

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 6 and above before changing playback state.
s32 viewFigurePlayMessage(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    Task* actor;

    if (args->animationId < 6) {
        gViewFigureWork->st.animId  = args->animationId;
        actor                       = gActorSelfTask;
        gViewFigureWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
        gViewFigureWork->st.field_6 = 0;
        _viewFigureStepAnim(actor);
        return 0;
    }
    return -1;
}
