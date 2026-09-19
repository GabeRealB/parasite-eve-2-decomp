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
    coord       = ((TmdObject*)task->extra)->coords;
    parentWork  = (ActorsShared80135c4cParentWork*)parent->work;
    parentCoord = &parentObj->coords[4];
    work        = Mem_Calloc(sizeof(*work), false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    coord->sub = &gGfxViewCoord;
    Gp_WorldToLocal(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->flg         = 0;
    work->field_3A     = 0xC0;
    pair               = parentWork->field_3AC;
    work->obj.coord    = coord;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->obj.ctx.recs = &work->rec;
    work->field_3C     = pair;
    work->obj.key      = Gp_PackPair(&ActorsShared80135c4cPair, 2);
    work->obj.radius   = 0x100;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= 0xC000;
    task->state      = 1;
}
