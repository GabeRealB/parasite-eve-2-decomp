#include "common.h"

#include "actors/actor_460200.h"
#include "actors/actors_shared_80132514.h"
#include "actors/actors_shared_8014c874.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorsShared80132c6c(Task* task)
{
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra;
    GsCOORDINATE2* coord  = obj->coords;
    GsCOORDINATE2* sub    = &((TmdObject*)parent->extra)->coords[7];
    MATRIX*        work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = work;
            obj->colorMtx = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
