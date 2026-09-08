#include "common.h"

#include "actors/actors_shared_801328bc.h"

#include "main/tmd.h"

void ActorsShared801328bc(Task* task)
{
    char           pad[0x10];
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra;
    GsCOORDINATE2* coord  = obj->field_8;
    GsCOORDINATE2* sub    = &((TmdObject*)parent->extra)->field_8[7];
    MATRIX*        work   = (MATRIX*)parent->idMap;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->field_1C = work;
            obj->field_20 = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
