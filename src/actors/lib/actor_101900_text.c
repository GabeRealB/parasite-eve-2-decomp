#include "common.h"

#include "actors/actor_101900.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "main/gfx.h"

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn00260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0056C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn008B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn00E00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn00FA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn016F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn01950);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn01A7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn01C94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn02018);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn02664);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn02A50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03710);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03854);

void Actor01900_Fn03C04(GameSessionFrom4* session, Actor01900Delta* delta)
{
    Actor01900HeightClamp* row;
    s32                    offset;
    s32                    lo;
    s16                    i;

    for (i = 0; i < 2; i++) {
        row = &Actor01900_D172CC[i];
        if (session->field_3 == row->field_0 && session->field_2 == row->field_2) {
            lo     = row->lo;
            offset = delta->field_1C;
            if (offset < lo) {
                delta->field_1C = lo;
            } else if (row->hi < offset) {
                delta->field_1C = row->hi;
            }
            return;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03C98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn03FF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn042BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn04D14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0551C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn05B4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn05F38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06100);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06634);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06904);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06B4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn06F40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn07810);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn07BA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn080A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn083E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn08724);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0892C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn09694);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn09BE8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn09D3C);

void Actor01900_Fn0A314(void)
{
}

s32 Actor01900_Fn0A31C(Actor01900* arg0, s32 arg1, Actor01900Msg7D3* arg2)
{
    Actor01900Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

s32 Actor01900_Fn0A38C(Actor01900* arg0, s32 arg1, s32 arg2)
{
    TmdObject*      obj  = arg0->field_2C;
    Actor01900Work* work = arg0->field_1C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->field_C  = 0;
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0A44C);

s32 Actor01900_Fn0A49C(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*            coord;
    s32                       mx;
    s32                       mz;
    ActorsShared80169f74Work* work;

    work                                           = (ActorsShared80169f74Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    coord                                   = ((TmdObject*)task->extra)->field_8;
    mx                                      = coord->coord.m[2][0];
    mz                                      = coord->coord.m[2][2];
    work->yaw                               = ratan2(-mx, mz);
    return 1;
}

s32 Actor01900_Fn0A59C(void)
{
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0A5A4);

void Actor01900_Fn0A6CC(Task* task)
{
    Actor01900Work* work;
    GpEnemy*        enemy;

    work  = (Actor01900Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C38 != NULL) {
            Task_Kill(work->field_C38);
        }
        if (work->field_C3C != NULL) {
            Task_Kill(work->field_C3C);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0A764);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0A7C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0A868);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0A914);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0A9C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0AA78);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0AB1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0ABA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_101900_text", Actor01900_Fn0ABE4);
