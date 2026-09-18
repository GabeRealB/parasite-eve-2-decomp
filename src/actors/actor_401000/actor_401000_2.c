#include "common.h"

#include "actors/actor_401000.h"
#include "main/gfx.h"

s32 func_actor_401000_8013D958(Actor401000* arg0, s32 arg1, u16* arg2)
{
    u16              room;
    u16              state;
    u16              state2;
    Actor401000Work* work;

    work               = arg0->field_1C;
    work->field_C18[0] = ((u8*)arg2)[0];
    work->field_C18[1] = ((u8*)arg2)[1];
    work->field_C18[2] = ((u8*)arg2)[2];
    room               = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        if (state == 1) {
            work->field_0 = 0x17;
            return 1;
        }
        return 0;
    }
    if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                       = 0x1C;
                arg0->field_2C->field_8->coord.t[0] = -0x595;
                arg0->field_2C->field_8->coord.t[1] = 0;
                arg0->field_2C->field_8->coord.t[2] = -0x5B1;
                Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, -0x400, 1);
                arg0->field_2C->field_8->flg = 0;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

void func_actor_401000_8013DA78(Task* task)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = (Actor401000Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C1C != NULL) {
            Task_Kill(work->field_C1C);
        }
        if (work->field_C20 != NULL) {
            Task_Kill(work->field_C20);
        }
        Gp_UnlinkObj(&work->field_B50);
        Gp_UnlinkObj(&work->field_8D0);
        Gp_UnlinkObj(&work->field_A10);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_401000_8013DB10(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = (u16)(obj->field_C | 0x80);
        work->field_B50.flags        = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags        = (u16)(work->field_A10.flags | 0x4000);
    }
}

void func_actor_401000_8013DB6C(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DC14(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DCC0(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DD6C(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x12;
        work->field_89E       = 0xD;
        work->field_89A       = 0;
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
    }
    arg0->field_2C->field_8->flg = 0;
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401000_8013DE24(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8D0.field_1C = 0x1AE;
        work->field_B50.flags   &= 0x7FFF;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 8;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        work->field_8A2          = work->field_8A4;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401000_8013DEC8(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8D0.field_1C = 0x1AE;
        work->field_B50.flags   &= 0x7FFF;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0x16;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        work->field_8A2          = work->field_8A4;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->flags_68.half & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401000_8013DF6C(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = work->field_C10 + ((Gp_LcgState >> 16) & 0xF);
    }
    if (--work->field_6 < 0) {
        switch (work->field_89E) {
            case 0xB:
            case 0x17:
                work->field_0 = 0xF;
                break;
            case 0xC:
            case 0x18:
            case 0x19:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x15;
    }
}
