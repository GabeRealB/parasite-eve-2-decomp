#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Per-frame state of the actor's child task: mirrors the hidden bit 0x80 and
/// the buffers-live bit 0x4 of the parent's `TmdObject` - the task the spawn
/// handler passed as `Task::spawnArg2` - onto the child's own model. When the
/// parent's 0x4 is clear the child's is cleared too and its buffers are
/// reallocated through `Tmd_AllocBuffers`.
void func_actor_443500_801326A0(Task* task)
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
