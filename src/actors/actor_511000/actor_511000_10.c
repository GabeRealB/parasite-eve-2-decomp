#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Tick state of the child in the second state table: copies the spawner's
/// model flag bits 0x80 (hidden) and 0x4 (draw buffers allocated) onto this
/// task's model, rebuilding the buffers through `Tmd_AllocBuffers` when the
/// spawner's are gone.
void func_actor_511000_80132390(Task* task)
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
