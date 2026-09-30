/* Part of the view figure library; see view_figure.h. */

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 6 and above before changing playback state.
s32 viewFigurePlayMessage(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Task* actor;

    if (args->animationId < 6) {
        gViewFigureWork->st.animId  = args->animationId;
        actor                       = gActorSelfTask;
        gViewFigureWork->st.state   = 2;
        gViewFigureWork->st.field_6 = 0;
        viewFigureStepAnim(actor);
        return 0;
    }
    return -1;
}
