#include "common.h"

#include "actors/actor_401800.h"
#include "main/gfx.h"

/// Room request handler: copies the request's three leading bytes into the work
/// block, then dispatches on the request's room id and state pair.
///
/// Room `0x301` only accepts state `1` (mount); room `0x1002` accepts `0`
/// (reset) or `2`, which parks the actor's root coordinate at
/// (-0x595, 0, -0x5B1) and rebuilds its Y rotation from -0x400, clearing `flg`
/// so the coordinate tree recomputes. Anything else returns 0, leaving the work
/// block's `field_0` state alone. Same body as `Actor01900_Fn0A5A4`, which
/// takes the same record and dispatches on the same two room ids.
s32 func_actor_401800_8013DF80(Actor401800* arg0, s32 arg1, u16* arg2)
{
    Actor401800Work* work;
    u16              room;
    u16              state;
    u16              state2;

    work = arg0->field_1C;

    work->field_C10[0] = ((u8*)arg2)[0];
    work->field_C10[1] = ((u8*)arg2)[1];
    work->field_C10[2] = ((u8*)arg2)[2];

    room = arg2[0];
    if (room == 0x301) {
        state = arg2[1];
        switch (state) {
            case 1:
                work->field_0 = 0x17;
                return 1;
            default:
                return 0;
        }
    } else if (room == 0x1002) {
        state2 = arg2[1];
        switch (state2) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 2:
                work->field_0                      = 0x1C;
                arg0->field_2C->coords->coord.t[0] = -0x595;
                arg0->field_2C->coords->coord.t[1] = 0;
                arg0->field_2C->coords->coord.t[2] = -0x5B1;
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x400, 1);
                arg0->field_2C->coords->flg = 0;
                return 1;
            default:
                return 0;
        }
    } else {
        return 0;
    }
}

void func_actor_401800_8013E0A0(Task* task)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = (Actor401800Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C14 != NULL) {
            Task_Kill(work->field_C14);
        }
        if (work->field_C18 != NULL) {
            Task_Kill(work->field_C18);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_401800_8013E138(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->flags                   = (u16)(obj->flags | 0x80);
        work->field_B48.flags        = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags        = (u16)(work->field_A08.flags & 0xBFFF);
    }
}

void func_actor_401800_8013E194(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        arg0->field_2C->coords->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E23C(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        arg0->field_2C->coords->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E2E8(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        arg0->field_2C->coords->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E394(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->flags                   = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x12;
        work->field_89E       = 0xD;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
    }
    arg0->field_2C->coords->flg = 0;
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401800_8013E44C(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags    = 0;
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 8;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        work->field_8A2          = work->field_8A4;
    }
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

void func_actor_401800_8013E4F0(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags    = 0;
        work->field_8C8.field_1C = 0x12C;
        work->field_B48.flags   &= 0x7FFF;
        work->field_A08.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0x16;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        work->field_6            = 0;
        work->field_8A2          = work->field_8A4;
    }
    work->field_6 = (u16)(work->field_6 + 1);
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

/// Idle-step handler: with the live flag set a fresh `Gp_LcgState` draw is
/// spread over the step countdown as 0..7 extra steps, and once the countdown
/// underflows the animation state at `field_89E` picks the actor's next
/// `field_0` (0xF for states 11/23, 0x10 for 12/24/25); a target with no HP
/// left forces 0x15 over that. Same body as `func_actor_401300_80141DF4`,
/// whose counterpart masks the LCG draw with 0xF instead of 7.
void func_actor_401800_8013E5A4(Actor401800* arg0)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = work->field_C08 + ((Gp_LcgState >> 16) & 7);
    }
    if (--work->field_6 < 0) {
        switch (work->field_89E) {
            case 11:
            case 23:
                work->field_0 = 0xF;
                break;
            case 12:
            case 24:
            case 25:
                work->field_0 = 0x10;
                break;
        }
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x15;
    }
    func_actor_401800_80133EB8(arg0);
}
