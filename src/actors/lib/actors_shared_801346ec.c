#include "common.h"

#include "actors/actors_shared_801346ec.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorsShared801346ec(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (arg2 != 0) {
        obj->flags = obj->flags & 0xFF7F;
        return;
    }
    obj->flags = obj->flags | 0x80;
}
