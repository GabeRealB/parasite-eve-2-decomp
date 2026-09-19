#include "common.h"

#include "actors/actor_403000.h"
#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D324);

/// Same placement opcode as `ActorsShared80164954` (Y then X then Z, re-fetch
/// the coordinate for every field), except the heading is cached at
/// `Actor403000Work::yaw` (0xC) rather than 0x16, and `ratan2` reads both
/// matrix components from one saved `GsCOORDINATE2*`.
s32 func_actor_403000_8013D364(Task* task, s32 arg1, ActorShared80164954Placement* placement)
{
    GsCOORDINATE2*   coord;
    Actor403000Work* work;

    work                                          = (Actor403000Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    coord                                  = ((TmdObject*)task->extra)->coords;
    work->yaw                              = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    return 1;
}

s32 func_actor_403000_8013D464(Task* task, s32 arg1, Actor403000Msg* msg)
{
    Actor403000Work* work = (Actor403000Work*)task->work;

    work->field_AC6 = msg->field_4;
    work->field_0   = 1;
    work->field_2   = -1;
    return 0;
}

s16 func_actor_403000_8013D48C(Task* task)
{
    Actor403000Work* work  = (Actor403000Work*)task->work;
    s16              found = 0;
    s16              i;
    s32              value;

    for (i = 0; i < 5; i++) {
        value = work->records[i].key;
        if (value == 0) {
            break;
        }
        if ((value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

void func_actor_403000_8013D4F4(Task* task)
{
    Actor403000Work* work  = (Actor403000Work*)task->work;
    GpEnemy*         enemy = (GpEnemy*)task->spawnArg2;

    if (work != NULL) {
        Gp_UnlinkObj(&work->objB50.obj);
        Gp_UnlinkObj(&work->objBE8.obj);
        Gp_UnlinkObj(&work->objC80.obj);
        Gp_UnlinkObj(&work->objD18.obj);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_403000_8013D564(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_403000_80158CE0[(s16)arg1].vx;
    arg0->vy = D_actor_403000_80158CE0[(s16)arg1].vy;
    arg0->vz = D_actor_403000_80158CE0[(s16)arg1].vz;
}
