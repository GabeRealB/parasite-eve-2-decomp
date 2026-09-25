#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Keeps a child model's visibility in step with its parent's: bits 0x80
/// (hidden) and 0x4 (buffers released) of the parent task's `TmdObject` - the
/// task named by `spawnArg2` - are copied onto the calling task's own object.
/// When bit 0x4 comes off, the child's draw buffers are rebuilt through
/// `Tmd_AllocBuffers`.
void func_actor_310600_801628B0(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = (TmdObject*)((Task*)task->spawnArg2)->extra;
    object       = (TmdObject*)task->extra;

    if (!(parentObject->flags & 0x80)) {
        object->flags &= 0xFF7F;
    } else {
        object->flags |= 0x80;
    }
    if (!(parentObject->flags & 4)) {
        object->flags &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= 4;
}
