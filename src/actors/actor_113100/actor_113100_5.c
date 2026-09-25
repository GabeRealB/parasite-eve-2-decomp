#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Tick state of the child task whose state table is
/// `D_actor_113100_80131E30`: copies the parent's (`spawnArg2`) model flag bits
/// 0x80 and 0x4 onto the child's own model. Bit 0x80 is the deferred-kill flag
/// that hides the model; when the parent's bit 0x4 is clear the child's is
/// cleared too and `Tmd_AllocBuffers` is called on the child's model.
void func_actor_113100_80132E00(Task* task)
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
