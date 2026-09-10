#include "common.h"

#include "actors/actors_shared_80135c4c.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/tmd.h"

/// Creates the task's collision object, attaches it to model part four, and
/// initializes its single collision record.
void ActorsShared80135c4c(GpEnemy* enemy, Task* task)
{
    Task*                           parent;
    TmdObject*                      parentObj;
    GsCOORDINATE2*                  coord;
    ActorsShared80135c4cParentWork* parentWork;
    GsCOORDINATE2*                  parentCoord;
    ActorsShared80135c4cObjWork*    work;
    u16                             pair;

    parent      = task->parent;
    parentObj   = parent->extra;
    coord       = ((TmdObject*)task->extra)->field_8;
    parentWork  = (ActorsShared80135c4cParentWork*)parent->idMap;
    parentCoord = &parentObj->field_8[4];
    work        = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->idMap       = (TaskIdMap*)work;
    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    coord->sub = &Gfx_ViewCoord;
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->flg         = 0;
    work->field_3A     = 0xC0;
    pair               = parentWork->field_3AC;
    work->obj.field_8  = coord;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.field_C  = &work->rec;
    work->field_3C     = pair;
    work->obj.field_18 = Gp_PackPair(&ActorsShared80135c4cPair, 2);
    work->obj.field_1C = 0x100;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= 0xC000;
    task->state      = 1;
}
