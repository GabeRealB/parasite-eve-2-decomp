#include "common.h"

#include "actors/actor_202600.h"
#include "actors/actors_shared_80135c4c.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/tmd.h"

/// Setup state of the projectile task: allocates its 0x40-byte work, places
/// its model's coordinate at the spawning model's fifth node (expressed
/// relative to the view coordinate), and links the work's collision object
/// with its single record, carrying the parent work's `field_3AC`. The enemy
/// is destroyed when the allocation fails.
void Actor02600_Fn03E2C(GpEnemy* enemy, Task* task)
{
    Task*                        parent;
    TmdObject*                   parentObj;
    GsCOORDINATE2*               coord;
    Actor202600Work*             parentWork;
    GsCOORDINATE2*               parentCoord;
    ActorsShared80135c4cObjWork* work;
    u16                          pair;

    parent      = task->parent;
    parentObj   = parent->extra;
    coord       = ((TmdObject*)task->extra)->coords;
    parentWork  = (Actor202600Work*)parent->work;
    parentCoord = &parentObj->coords[4];
    work        = memCalloc(sizeof(*work), false);
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
    work->obj.key      = Gp_PackPair(&Actor02600_D08950, 2);
    work->obj.radius   = 0x100;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= 0xC000;
    task->state      = 1;
}
