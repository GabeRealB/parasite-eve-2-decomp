/* Part of the actor messages library; see actor_messages.h. */

s32 actorMsgSetPairVisibility(Task* task, s32 msgId, s32 flags, s32 unusedArg)
{
    TmdObject* model;
    TmdObject* helperModel;

    model       = gActorSelfTask->extra.tmd;
    helperModel = gActorHelperTask->extra.tmd;

    if (flags & ACTOR_MESSAGE_PAIR_SHOW) {
        model->flags       = 0;
        helperModel->flags = 0;
    } else {
        model->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        helperModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        helperModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
