#include "common.h"

#include "actors/actor_02400_spawn.h"
#include "main/mem.h"

extern u32        Gp_LcgState;
extern GpU16Pair  Actor02400_BodyPairs;
extern GpPairSrcE Actor02400_Params0;
extern GpPairSrcE Actor02400_Params1;

void ActorsShared8013277c(GpEnemy* enemy, Task* task)
{
    TmdObject*           obj;
    GsCOORDINATE2*       coord;
    Actor02400SpawnWork* work;

    obj   = task->extra;
    coord = obj->coords;
    work  = memCalloc(0x154, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work    = work;
    obj->flags    = 0;
    coord->flg    = 0;
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    work->variant = enemy->place->mode & 1;
    if (work->variant != 0) {
        obj->clut += 1;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
    }
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->bodyPos.vy = -0x96;
    enemy->coord      = coord;
    enemy->node.flags = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vz = 0;
    enemy->recs       = work->rec60;
    enemy->param      = work->variant == 0 ? &Actor02400_Params0 : &Actor02400_Params1;
    enemy->hp         = enemy->param->hpMax;
    SCHED_BARRIER();
    work->field_F8 = &((TmdObject*)task->extra)->coords[1];
    work->field_FC = 0x200;
    work->field_FE = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_128      = 0x600;
    work->field_12A      = 0x600;
    work->field_12C      = 0x600;
    work->field_100      = coord->coord;
    Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
    work->field_140      = (Gp_LcgState >> 16) & 0xF;
    work->obj40.key      = 0x30018;
    work->obj40.coord    = coord;
    work->obj40.ctx.recs = work->rec60;
    work->obj40.pos.vy   = -0xC8;
    work->obj40.pos.vx   = 0;
    work->obj40.pos.vz   = 0;
    work->obj40.radius   = 0xC8;
    work->obj40.flags    = 1;
    Gp_LinkObj(2, &work->obj40);
    Gp_InitRec18Table(work->rec60, 4, 0);
    work->obj40.flags   |= 0xC200;
    work->objC0.coord    = &((TmdObject*)task->extra)->coords[3];
    work->objC0.ctx.recs = &work->recE0;
    work->objC0.pos.vx   = 0;
    work->objC0.pos.vy   = 0;
    work->objC0.pos.vz   = 0x1F4;
    work->objC0.key      = Gp_PackPair(&Actor02400_BodyPairs, work->variant * 2);
    work->objC0.radius   = 0x64;
    work->objC0.flags    = 1;
    Gp_LinkObj(3, &work->objC0);
    Gp_InitRec18Table(&work->recE0, 1, 0);
    work->objC0.flags |= 0x8000;
    task->state        = 1;
}
