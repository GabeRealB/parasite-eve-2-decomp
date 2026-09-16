#include "common.h"

#include "actors/actor_421600.h"
#include "main/task.h"

s32 func_actor_421600_8013E654(Task* task)
{
    Actor421600Work* work = (Actor421600Work*)task->idMap;

    work->field_EAC = 0x1E;
    return 1;
}

void func_actor_421600_8013E668(Task* task)
{
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = (Actor421600Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_E94 != NULL) {
            Task_Kill(work->field_E94);
        }
        if (work->field_E98 != NULL) {
            Task_Kill(work->field_E98);
        }
        Gp_UnlinkObj(&work->field_8EC);
        Gp_UnlinkObj(&work->field_A2C);
        Gp_UnlinkObj(&work->field_B6C);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E700);

void func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_421600_80151158[(s16)arg1].vx;
    arg0->vy = D_actor_421600_80151158[(s16)arg1].vy;
    arg0->vz = D_actor_421600_80151158[(s16)arg1].vz;
}

s8 func_actor_421600_8013E830(s32 arg0, s32 arg1)
{
    s8* p;
    s32 a;
    s32 b;

    p = D_actor_421600_801511D0;
    a = arg0 > 0;
    b = arg1 < 1;
    return p[a + (b << 1)];
}

void func_actor_421600_8013E858(Actor421600* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;
    GpEnemy*         enemy;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        enemy                  = arg0->field_20;
        enemy->node.field_4    = 1;
        obj->field_C          |= 0x80;
        work->field_B6C.flags &= 0xBFFF;
        enemy->field_40        = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E8AC);

void func_actor_421600_8013E9D8(Actor421600* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;
    s32              state;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.field_1C = 0x19C;
        work->field_828          = 1;
        work->field_832          = 0x10;
        work->field_82A          = 0;
        work->field_82E          = 4;
        work->field_B6C.flags   |= 0x4000;
        func_actor_421600_80134604(arg0);
        return;
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        state = work->field_E90 & 0xFFFFFF;
        if (state == 0x11402) {
            state = 5;
        } else {
            state = 2;
        }
        work->field_0 = state;
    }
}

void func_actor_421600_8013EAAC(Actor421600* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.field_1C = 0x19C;
        work->field_828          = 1;
        work->field_832          = 0x10;
        work->field_82A          = 0;
        work->field_82E          = 8;
        work->field_B6C.flags   |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_8013285C(arg0->field_2C->field_8, &work->field_B8C, 0xC);
    arg0->field_2C->field_8->flg = 0;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
}

void func_actor_421600_8013EB7C(Actor421600* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.field_1C = 0x19C;
        work->field_828          = 1;
        work->field_832          = 0x10;
        work->field_82A          = 0;
        work->field_82E          = 0xC;
        work->field_B6C.flags   |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 2;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013EC28);

void func_actor_421600_8013ED24(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8EC.field_1C = 0x19C;
        work->field_B6C.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_828          = 1;
        work->field_82E          = 0x13;
        work->field_832          = 0x10;
        work->field_840          = 0;
        work->field_83E          = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        if (enemy->field_40 > 0) {
            if (enemy->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void func_actor_421600_8013EE0C(Actor421600* arg0)
{
    TmdObject*       obj;
    Actor421600Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        obj->field_C             = 0;
        work->field_8EC.field_1C = 0x19C;
        work->field_B6C.flags   |= 0x4000;
        enemy->node.field_4      = 1;
        work->field_828          = 1;
        work->field_82E          = 0x16;
        work->field_832          = 0x10;
        work->field_840          = 0;
        work->field_83E          = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x15;
    }
}
