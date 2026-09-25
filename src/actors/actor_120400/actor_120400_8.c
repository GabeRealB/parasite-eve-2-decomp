#include "common.h"

#include "actors/actor_120400.h"

#include "main/tmd.h"

/// Per-frame tick of a child task: copies the parent model's hidden bit (0x80)
/// and bit 0x4 onto the child's own model. While the parent's bit 0x4 is
/// clear the child's display buffers are (re)allocated as well.
void func_actor_120400_801326B0(Task* task)
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
