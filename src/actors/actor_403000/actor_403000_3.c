#include "common.h"

#include "actors/actor_403000.h"

void func_actor_403000_8013D5F8(Actor403000* arg0)
{
    TmdObject*       obj;
    Actor403000Work* work;
    GpEnemy*         enemy;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        enemy                   = arg0->field_20;
        work->field_FCA         = 1;
        obj->field_C           |= 0x80;
        work->objD18.obj.flags &= 0xBFFF;
        enemy->field_40         = 0;
    }
}

void func_actor_403000_8013D648(Actor403000* arg0)
{
    TmdObject*       obj;
    Actor403000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        work->field_FCA         = 0;
        obj->field_C            = 0;
        work->field_ACA         = 0x30;
        work->field_AC6         = 0xF;
        work->field_AC0         = 2;
        work->field_6           = 0;
        work->objD18.obj.flags |= 0x4000;
    }
    work->field_6++;
    func_actor_403000_80133AF8(arg0);
    if ((work->field_60.word & 0x102) || (s16)work->field_6 >= 5) {
        if (enemy->field_40 > 0) {
            if (enemy->field_4C & 2) {
                work->field_0 = 0x10;
            } else {
                work->field_0 = 0x12;
            }
        } else {
            work->field_F8C = 1;
            work->field_0   = 0x14;
        }
    }
}

void func_actor_403000_8013D72C(Actor403000* arg0)
{
    GpEnemy*         enemy;
    Actor403000Work* work;
    TmdObject*       obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        enemy           = arg0->field_20;
        work->field_FCA = 1;
        Gp_ClearNodeSlots(&enemy->node);
        obj->field_C = 0;
        Tmd_AllocBuffers(obj);
        work->field_ACA         = 0x10;
        work->field_AC0         = 2;
        work->field_AD6         = 0;
        work->field_AD8         = 0;
        work->objD18.obj.flags |= 0x4000;
        func_actor_403000_80133AF8(arg0);
        return;
    }
    func_actor_403000_80133AF8(arg0);
    if (work->field_FA6 == 0xA) {
        work->field_F30.t[2]    = 0;
        work->field_F30.t[1]    = 0;
        work->field_F30.t[0]    = 0;
        work->field_F30.m[2][2] = 0;
        work->field_F30.m[2][1] = 0;
        work->field_F30.m[2][0] = 0;
        work->field_F30.m[1][2] = 0;
        work->field_F30.m[1][1] = 0;
        work->field_F30.m[1][0] = 0;
        work->field_F30.m[0][2] = 0;
        work->field_F30.m[0][1] = 0;
        work->field_F30.m[0][0] = 0;
    }
    if (work->field_FA6 == 4 && work->field_AC6 == 0x1B && (work->field_60.half & 0x100)) {
        work->field_AC6 = 0x1D;
        work->field_AC0 = 2;
    }
}

void func_actor_403000_8013D850(Actor403000* arg0)
{
    Actor403000Work* work;
    TmdObject*       obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        work->field_FCA = 0;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_ACA           = 0x10;
        work->field_AC2           = 0;
        work->field_AC6           = 0x10;
        work->objD18.obj.flags   |= 0x4000;
        func_actor_403000_80133AF8(arg0);
        work->field_6 = 0;
    }
    work->field_6++;
    func_actor_403000_80133AF8(arg0);
    if (work->field_60.half & 0x100) {
        work->field_FD3 = 1;
        work->field_FD2 = 1;
        work->field_0   = 2;
    }
}

void func_actor_403000_8013D910(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;
    u32              rng;
    s16              timer;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        rng           = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng;
        work->field_6 = ((rng >> 16) & 0xF) + 0xA;
    }
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if (timer < 0 && enemy->field_40 > 0) {
        work->field_0 = 0x13;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_3", func_actor_403000_8013D98C);
