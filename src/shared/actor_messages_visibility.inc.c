/* Part of the actor messages library; see actor_messages.h. */

s32 actorMsgSetVisibility(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    TmdObject*         model;
    ActorMsgStateWork* work;

    model = task->extra.tmd;
    work  = task->work;
    switch (mode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_MESSAGE_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_MESSAGE_STATE_PATROL;
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_MESSAGE_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = ACTOR_MESSAGE_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}
