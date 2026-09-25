#include "common.h"

#include "actors/actor_323000.h"
#include "main/tmd.h"

/// Handler for message 0x7D5: sets the model's display flags for the mode in
/// `arg2` and picks the state that follows. 0 sets flag 0x80, rebuilds the
/// buffers and restarts state 0; 1 clears the flags, rebuilds and starts
/// state 2; 2 raises flag 4 over the current flags and 3 replaces them with
/// it, both restarting state 0.
s32 func_actor_323000_80164844(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor323000Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor323000Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 2;
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
