#include "common.h"

#include "actors/actors_shared_80132390.h"

#include "main/tmd.h"

void ActorsShared80132390(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = (TmdObject*)((Task*)task->spawnArg2)->extra;
    object       = (TmdObject*)task->extra;

    if (!(parentObject->field_C & 0x80)) {
        object->field_C &= 0xFF7F;
    } else {
        object->field_C |= 0x80;
    }
    if (!(parentObject->field_C & 4)) {
        object->field_C &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->field_C |= 4;
}
