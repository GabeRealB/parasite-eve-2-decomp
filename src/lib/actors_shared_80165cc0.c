#include "common.h"
#include "main/task.h"
#include "gameplay/1BC.h"
#include "actors/actors_shared_80165cc0.h"
#include "actors/actors_shared_80164b68.h"
#include "main/tmd.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void ActorsShared80165cc0(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* start;
    s32                       i;
    s32                       j;
    s32                       k;

    work = (ActorsShared80168d3cWork*)arg0->work;
    if (work->field_414 == 1) {
        start = work;
        if (start->field_416 == start->field_418) {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
        } else {
            for (i = 1; i < 9; i++) {
                (&start->slot_B4)[i].rate = start->field_41C;
                func_800B4114(&start->anim, i, start->field_418, 0, start->field_426);
            }
            start->field_426 = 0;
        }
        goto advance;
    }
    if (work->field_414 == 2) {
        start = work;
        for (j = 1; j < 9; j++) {
            Gp_AnimResetSlot(&start->anim, j, start->field_418);
            (&start->slot_B4)[j].rate = start->field_41C;
        }
    advance:
        start->field_416 = start->field_418;
        work->field_414  = 3;
        work->field_41A  = 0;
    } else if (work->field_414 == 3) {
        work->field_41A++;
    }
    for (k = 1; k < 9; k++) {
        (&work->slot_B4)[k].rate = work->field_41C;
        Gp_AnimTickIndex(&work->anim, k);
    }
}

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
