/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Handler for message 0x7D5: sets the model's display flags for the mode in
/// `arg2` and picks the state that follows. 0 hides the model (flag 0x80
/// alone), rebuilds the buffers and restarts state 0; 1 clears the flags,
/// showing it, rebuilds and starts state 2; 2 raises `TMD_OBJECT_SKIP_AUTO_BUFFER` over the current
/// flags and 3 replaces them with it, both restarting state 0.
s32 desertChaserSetVisibility(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*        obj;
    DesertChaserWork* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->state = 0;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            work->state = 2;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = 0;
            break;
        case 3:
            obj->flags  = 0;
            work->state = 0;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}
