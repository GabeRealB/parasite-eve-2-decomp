/* Part of the view figure library; see view_figure.h. */

/// Restarts the singleton view figure's body animation from a message request.
///
/// Requires live published figure work, its initialized rig and a request
/// whose `animationId` selects a loaded body set (1..5 in both carriers).
/// Reads only `animationId`; bank, blend and collision options are ignored.
/// The signed test rejects IDs >= 6 with -1, while negative IDs and zero still
/// enter the restart path. Accepted requests return zero after resetting slots
/// 1..19. Borrows the request only during dispatch; the receiver is unused.
static s32 _viewFigurePlayMessage(Task* unusedTask, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum { VIEW_FIGURE_ANIMATION_ID_LIMIT = 6 };
    Task* figureTask;

    if (request->animationId < VIEW_FIGURE_ANIMATION_ID_LIMIT) {
        gViewFigureWork->st.animId  = request->animationId;
        figureTask                  = gActorSelfTask;
        gViewFigureWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
        gViewFigureWork->st.field_6 = 0;
        _viewFigureStepAnim(figureTask);
        return 0;
    }
    return -1;
}
