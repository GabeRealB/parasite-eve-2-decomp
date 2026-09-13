#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"

void func_actor_206100_8014F18C(Task* task)
{
    Actor206100Work* work;

    work = (Actor206100Work*)task->idMap;

    work->obj_364.field_8  = &((TmdObject*)task->extra)->field_8[1];
    work->obj_364.field_C  = (GpRec18*)work->pad_384;
    work->obj_364.field_10 = 0;
    work->obj_364.field_12 = 0;
    work->obj_364.field_14 = 0;
    work->obj_364.field_18 = 0x3003D;
    work->obj_364.field_1C = 0x400;
    work->obj_364.flags    = 1;
    Gp_LinkObj(2, &work->obj_364);
    Gp_InitRec18Table((GpRec18*)work->pad_384, 6, 0);
    work->obj_364.flags &= 0x7FFF;

    work->obj_414.field_8  = &((TmdObject*)task->extra)->field_8[4];
    work->obj_414.field_C  = (GpRec18*)work->pad_384;
    work->obj_414.field_10 = 0;
    work->obj_414.field_12 = 0;
    work->obj_414.field_14 = 0;
    work->obj_414.field_18 = 0x3003D;
    work->obj_414.field_1C = 0x200;
    work->obj_414.flags    = 1;
    Gp_LinkObj(2, &work->obj_414);
    work->obj_414.flags &= 0x7FFF;
}

void func_actor_206100_8014F284(Task* task)
{
    Actor206100Work*       work;
    Actor206100AnimStride* stride;
    s32                    i;

    work   = (Actor206100Work*)task->idMap;
    i      = 1;
    stride = (Actor206100AnimStride*)work + 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_510);
        i++;
        stride->field_1D = (u8)work->field_51A;
        stride++;
    } while (i < 0xF);
    work->field_50E = (u16)work->field_510;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F2F0);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F3C8);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F428);
