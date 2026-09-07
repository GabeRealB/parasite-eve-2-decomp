#include "common.h"

#include "actors/actors_shared_80164b68.h"

#include "main/tmd.h"

void ActorsShared80164b68(Task* task)
{
    ActorsShared80164b68Work* work = (ActorsShared80164b68Work*)task->idMap;

    work->obj_2AC.field_8  = &((TmdObject*)task->extra)->field_8[1];
    work->obj_2AC.field_C  = work->rec_2EC;
    work->obj_2AC.field_10 = 0;
    work->obj_2AC.field_12 = 0;
    work->obj_2AC.field_14 = 0;
    work->obj_2AC.field_18 = 0x3002C;
    work->obj_2AC.field_1C = 0x170;
    work->obj_2AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2AC);
    Gp_InitRec18Table(work->rec_2EC, 8, 0);
    work->obj_2AC.flags |= 0x8000;

    work->obj_3AC.field_8  = &((TmdObject*)task->extra)->field_8[1];
    work->obj_3AC.field_C  = work->rec_3CC;
    work->obj_3AC.field_10 = 0;
    work->obj_3AC.field_12 = 0;
    work->obj_3AC.field_14 = 0;
    work->obj_3AC.field_18 = Gp_PackObjPair(task->spawnArg2, 0);
    work->obj_3AC.field_1C = 0x170;
    work->obj_3AC.flags    = 1;
    Gp_LinkObj(2, &work->obj_3AC);
    Gp_InitRec18Table(work->rec_3CC, 2, 0);
    work->obj_3AC.flags &= 0x7FFF;

    work->obj_2CC.field_8  = &((TmdObject*)task->extra)->field_8[1];
    work->obj_2CC.field_C  = work->rec_2EC;
    work->obj_2CC.field_10 = 0;
    work->obj_2CC.field_12 = 0;
    work->obj_2CC.field_14 = 0;
    work->obj_2CC.field_18 = 0x3002C;
    work->obj_2CC.field_1C = 0x224;
    work->obj_2CC.flags    = 1;
    Gp_LinkObj(2, &work->obj_2CC);
    work->obj_2CC.flags |= 0x4000;
}
