#include "common.h"

#include "actors/actor_110600.h"

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138680);

s32 func_actor_110600_801387C0(Task* arg0)
{
    Actor110600Work* work;

    work = (Actor110600Work*)arg0->idMap;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_0 = 4;
    return 1;
}

void func_actor_110600_801387F4(Task* task)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    Task*            helper;
    Task*            helper2;

    work  = (Actor110600Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        helper = work->field_BD4;
        if (helper != NULL) {
            helper->state++;
        }
        helper2 = work->field_BD8;
        if (helper2 != NULL) {
            helper2->state++;
        }
        Gp_UnlinkObj(&work->field_A90);
        Gp_UnlinkObj(&work->field_8B8);
        Gp_UnlinkObj(&work->field_950);
        enemy->field_54 = 0;
    }
    Display_ClampField126(0);
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_110600_801388A4(Actor110600* arg0)
{
    TmdObject*       obj;
    Actor110600Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = (u16)(obj->field_C | 0x80);
        work->field_A90.flags        = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags        = (u16)(work->field_950.flags & 0xBFFF);
    }
}

s32 func_actor_110600_80138900(void)
{
    s16* p;
    s16  next;
    s32  cur;

    p    = &D_actor_110600_8014865C;
    next = (u16)*p + 1;
    *p   = next;
    if (next == 5) {
        *p = 0;
    }
    cur = (u16)*p;
    if ((cur & 1) == 0) {
        Display_ClampField126(0);
    } else {
        Display_ClampField126(1);
    }
    if (D_actor_110600_8014865C != 0) {
        return 0;
    }
    Display_ClampField126(0);
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138980);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138A70);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138AFC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138BD0);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138CA4);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600_2", func_actor_110600_80138D7C);
