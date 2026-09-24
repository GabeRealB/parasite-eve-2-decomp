#include "common.h"

#include "actors/actor_421600.h"
#include "main/task.h"
#include "main/tmd.h"

/// Handler for message 0x7D5: set the model's display mode. Modes 0 and 1
/// reset the model flags (0 with 0x80 set) and reallocate its buffers; 2 and 3
/// set flag 4, 3 clearing the others first. The work block's state is reset to
/// 0, or to 0x18 for mode 1.
s32 func_actor_421600_8013E42C(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor421600Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor421600Work*)task->work;
    switch (mode) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}
