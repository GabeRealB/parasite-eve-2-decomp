/* Part of the reversing walker library; see reversing_walker.h. */

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 sets the 0x80 flag, under which the tick skips the shadow and the
/// part update, and clears the 4 flag; 1 clears 0x80, allocates the model
/// buffers and clears 4; 2 sets 0x80 and 4 and latches the mode into the
/// `freeCountdown` countdown, which frees the buffers when it runs out; 3 clears
/// 0x80 and sets 4. Anything else returns 1 and leaves the object alone; the
/// handled modes return 0.
s32 reverseWalkVisibilityMsg(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags                                   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ((Actor350500Work*)task->work)->freeCountdown = mode;
            obj->flags                                   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}
