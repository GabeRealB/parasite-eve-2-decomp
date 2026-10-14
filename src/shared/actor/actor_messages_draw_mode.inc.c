/* Part of the actor messages library; see actor_messages.h. */

void actorMsgSetDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    switch (mode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags = (model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            return;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}
