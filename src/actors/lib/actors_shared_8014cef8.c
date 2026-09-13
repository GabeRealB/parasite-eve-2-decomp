#include "common.h"

#include "actors/actors_shared_8014cef8.h"

#include "main/tmd.h"

void ActorsShared8014cef8(Task* task)
{
    char           pad[0x10];
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra;
    GsCOORDINATE2* coord  = obj->field_8;
    GsCOORDINATE2* sub    = &((TmdObject*)parent->extra)->field_8[4];
    MATRIX*        work   = (MATRIX*)parent->idMap;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->field_1C = work;
            obj->field_C  = 0;
            obj->field_20 = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
