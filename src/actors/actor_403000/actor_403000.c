#include "common.h"

#include "actors/actor_403000.h"
#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132348);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", D_actor_403000_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801324EC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801327B0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801330D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801332E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133444);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801336B4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801337E0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133AF8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133FC0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134204);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801343B8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134910);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134E00);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134F44);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80135F08);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013603C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801365D0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136B14);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136D68);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80137084);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801377C8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801384E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801386E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80138DB0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801399A0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80139AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A08C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A678);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013ACBC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B238);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B74C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013BDE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C050);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C2D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C864);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", ActorsShared80135df4Table);

void func_actor_403000_8013D260(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D268);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D324);

/// Same placement opcode as `ActorsShared80164954` (Y then X then Z, re-fetch
/// the coordinate for every field), except the heading is cached at
/// `Actor403000Work::yaw` (0xC) rather than 0x16, and `ratan2` reads both
/// matrix components from one saved `GsCOORDINATE2*`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorShared80164954Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor403000Work* work;

    work                                           = (Actor403000Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    coord                                   = ((TmdObject*)task->extra)->field_8;
    work->yaw                               = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    return 1;
}

s32 func_actor_403000_8013D464(Task* task, s32 arg1, Actor403000Msg* msg)
{
    Actor403000Work* work = (Actor403000Work*)task->idMap;

    work->field_AC6 = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013D48C);

void func_actor_403000_8013D4F4(Task* task)
{
    Actor403000Work* work  = (Actor403000Work*)task->idMap;
    GpEnemy*         enemy = (GpEnemy*)task->spawnArg2;

    if (work != NULL) {
        Gp_UnlinkObj(&work->objB50.obj);
        Gp_UnlinkObj(&work->objBE8.obj);
        Gp_UnlinkObj(&work->objC80.obj);
        Gp_UnlinkObj(&work->objD18.obj);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_403000_80158CE0[(s16)arg1].vx;
    arg0->vy = D_actor_403000_80158CE0[(s16)arg1].vy;
    arg0->vz = D_actor_403000_80158CE0[(s16)arg1].vz;
}
