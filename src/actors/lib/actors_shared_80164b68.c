#include "common.h"

#include "actors/actors_shared_80164b68.h"

#include "main/tmd.h"

void ActorsShared80164b68(Task* task)
{
    ActorsShared80164b68Work* work = (ActorsShared80164b68Work*)task->work;

    work->obj_2AC.coord    = &((TmdObject*)task->extra)->coords[1];
    work->obj_2AC.ctx.recs = work->rec_2EC;
    work->obj_2AC.pos.vx   = 0;
    work->obj_2AC.pos.vy   = 0;
    work->obj_2AC.pos.vz   = 0;
    work->obj_2AC.key      = 0x3002C;
    work->obj_2AC.radius   = 0x170;
    work->obj_2AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2AC);
    Gp_InitRec18Table(work->rec_2EC, 8, 0);
    work->obj_2AC.flags |= 0x8000;

    work->obj_3AC.coord    = &((TmdObject*)task->extra)->coords[1];
    work->obj_3AC.ctx.recs = work->rec_3CC;
    work->obj_3AC.pos.vx   = 0;
    work->obj_3AC.pos.vy   = 0;
    work->obj_3AC.pos.vz   = 0;
    work->obj_3AC.key      = Gp_PackObjPair(task->spawnArg2, 0);
    work->obj_3AC.radius   = 0x170;
    work->obj_3AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_3AC);
    Gp_InitRec18Table(work->rec_3CC, 2, 0);
    work->obj_3AC.flags &= 0x7FFF;

    work->obj_2CC.coord    = &((TmdObject*)task->extra)->coords[1];
    work->obj_2CC.ctx.recs = work->rec_2EC;
    work->obj_2CC.pos.vx   = 0;
    work->obj_2CC.pos.vy   = 0;
    work->obj_2CC.pos.vz   = 0;
    work->obj_2CC.key      = 0x3002C;
    work->obj_2CC.radius   = 0x224;
    work->obj_2CC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2CC);
    work->obj_2CC.flags |= 0x4000;
}
