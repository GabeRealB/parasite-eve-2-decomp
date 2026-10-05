/* Part of the actor messages library; see actor_messages.h. */

/// Applies a model-visibility request to the actor: 0 hides the model and
/// rebuilds its buffers, 1 shows it and rebuilds them with the state set to
/// 0x18, 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on top of the current flags, and
/// 3 clears every other flag before setting it. All but 1 reset the state to 0.
s32 actorMsgSetVisibility(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*         obj;
    ActorMsgStateWork* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_MESSAGE_STATE_HIDDEN;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            work->state = ACTOR_MESSAGE_STATE_PATROL;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = ACTOR_MESSAGE_STATE_HIDDEN;
            break;
        case 3:
            obj->flags  = 0;
            work->state = ACTOR_MESSAGE_STATE_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}
