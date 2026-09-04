#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

void Room_Script13(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->field_8;
    dest            = &parentExtra->field_8[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->field_1C = parentExtra->field_1C;
    extra->field_20 = parentExtra->field_20;
    Task_Reparent(parent, task);
    task->state += 1;
}
