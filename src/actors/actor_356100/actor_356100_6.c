#include "common.h"

#include "actors/actor_356100.h"
#include "main/task.h"
#include "main/tmd.h"

/// Applies a model-visibility request to the actor: 0 hides the model and
/// rebuilds its buffers, 1 shows it and rebuilds them with the state set to
/// 0x18, 2 sets model flag 4 on top of the current flags, and 3 clears every
/// other flag before setting 4. All but 1 reset the state to 0.
s32 func_actor_356100_80169E64(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor356100Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor356100Work*)task->work;
    switch (arg2) {
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
