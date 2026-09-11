#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_801651E0(Task* arg0);
void func_actor_341700_80168A14(Task* arg0);
void func_actor_341700_80168A48(Task* arg0);
void func_actor_341700_801697B8(Task* arg0);
void func_actor_341700_801697D4(Task* arg0);
void func_actor_341700_80169888(Task* arg0);
void func_actor_341700_8016999C(Task* arg0);
void func_actor_341700_80169AB0(Task* arg0);

/* Both are called with no argument: the caller's own `Task*` is already in
 * `$a0` at the `jal` and the callee reads it as its own `arg0`, so the target
 * has no register copy.  A real prototype would make GCC emit one, so these
 * stay unprototyped. */
s32 ActorsShared8016945c();
s32 ActorsShared8016974c();

/* This one, by contrast, is a normal prototyped call: the redundant
 * `move $a0, $s0` is deleted again after reload because `$a0` still holds the
 * caller's own `arg0`. */
s32 func_actor_341700_80168234(Task* arg0);

void func_actor_341700_80168684(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_341700_80168698(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_341700_801686AC(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// The three sub-state handlers guarded by `func_actor_341700_80168234`.
extern TaskFuncTable3 D_actor_341700_80161E9C;

void func_actor_341700_801686C0(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable3   sp;

    work = (Actor341700Work*)arg0->idMap;
    sp   = D_actor_341700_80161E9C;
    if ((func_actor_341700_80168234(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// The five sub-state handlers this branch of the actor dispatches through.
extern TaskFuncTable5 D_actor_341700_80161EA8;

void func_actor_341700_80168748(Task* arg0)
{
    Actor341700Work* work;
    TaskFuncTable5   sp;

    work = (Actor341700Work*)arg0->idMap;
    sp   = D_actor_341700_80161EA8;
    sp.funcs[(s16)work->field_422](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_16", func_actor_341700_801687B4);

void func_actor_341700_80168820(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_341700_80169888,
        func_actor_341700_801651E0,
    };

    states[(s16)work->field_422](arg0);
}

void func_actor_341700_80168874(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_341700_8016999C,
        func_actor_341700_80169AB0,
    };

    states[(s16)work->field_422](arg0);
}

void func_actor_341700_801688C8(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_341700_80168A14,
        func_actor_341700_80168A48,
    };

    states[(s16)work->field_422](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_16", func_actor_341700_8016891C);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_16", func_actor_341700_801689A0);

void func_actor_341700_80168A14(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}

void func_actor_341700_80168A48(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;

    if ((ActorsShared8016974c() << 0x10) != 0) {
        work            = (Actor341700Work*)arg0->idMap;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor341700Work*)arg0->idMap;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_16", func_actor_341700_80168AC0);

void func_actor_341700_80168B40(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* slow;
    Actor341700Work* fast;

    work = (Actor341700Work*)arg0->idMap;
    if ((ActorsShared8016974c() << 0x10) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor341700Work*)arg0->idMap;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor341700Work*)arg0->idMap;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_16", func_actor_341700_80168BE0);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_16", func_actor_341700_80168C4C);
