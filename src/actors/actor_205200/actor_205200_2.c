#include "common.h"

#include "actors/actor_205200.h"
#include "actors/actors_shared_80134ff0.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_205200_80149E24;
extern GpEnemyTaskFuncTable3 D_actor_205200_80149E30;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_205200_8014BD4C(Actor205200* arg0);
void func_actor_205200_8014BF28(Actor205200* arg0);
void func_actor_205200_8014C0C0(Actor205200* arg0);
void func_actor_205200_8014C67C(Actor205200* arg0);
void func_actor_205200_8014C748(Actor205200* arg0);
void func_actor_205200_8014C7CC(Actor205200* arg0);
void func_actor_205200_8014C8D4(Actor205200* arg0);
void func_8017EBA4(Actor205200* arg0);
void func_80181930(Actor205200* arg0);

extern u8  D_801153F4;
extern u16 D_80071078;

s32 func_actor_205200_8014B914(s32 arg0)
{
    s32 delta;

    delta = arg0 - D_80071078;
    if (delta >= 0x7FFF) {
        delta = 0x7FFF;
    }
    if (delta < -0x7FFF) {
        delta = -0x7FFF;
    }
    return delta >> 8;
}

s32 func_actor_205200_8014B94C(Actor205200* arg0, s32 arg1, Actor205200Msg7DB* arg2)
{
    Actor205200Work* work;

    work = arg0->field_1C;
    if (arg2->field_2 != 0 && work->field_2E == 0) {
        work->field_2E = 1;
    }
    return 0;
}

void func_actor_205200_8014B978(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_205200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame tick of the live states. `D_801153F4` gates the shared body:
/// mode 1 runs none of it, mode 2 raises the node flag to 1 and returns, mode 0
/// raises it to 8 before falling in, and any other mode enters it directly.
/// The body ticks the effect timer and, once the parent actor's 0x7DB flag is
/// up, pushes this task to state 2 and re-arms `field_72`. The dispatch is
/// written as gotos because that is the shape the switch's binary decision tree
/// leaves behind - mode 0 shares the body with the default path, so its `break`
/// is a jump into it (see `func_actor_207200_8014D2DC`).
void func_actor_205200_8014B9D4(GpEnemy* arg0, Actor205200* arg1)
{
    Actor205200Work* work;
    Actor205200Work* parentWork;
    s32              state;
    s32              one;

    work       = arg1->field_1C;
    parentWork = (Actor205200Work*)arg1->field_8->idMap;
    state      = D_801153F4;
    one        = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg0->node.field_4 = 8;
    goto default_body;
case2:
    arg0->node.field_4 = one;
    return;
default_body:
    func_actor_205200_8014B048(arg1, one);
    if (work->field_74 != 0) {
        func_actor_205200_8014BA94(arg1);
    }
    if (parentWork->field_2E == 1) {
        arg1->field_30 = 2;
        work->field_72 = 2;
    }
case1:
    return;
}

void func_actor_205200_8014BA94(Actor205200* arg0)
{
    Actor205200Work* work;
    u16              timer;

    work           = arg0->field_1C;
    timer          = (u16)work->field_74 - 1;
    work->field_74 = timer;
    if (!(timer & 0x3F)) {
        func_800FDB18(7, arg0->field_2C->field_8, NULL, &work->field_68);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014BAE8);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014BD4C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014BF28);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C0C0);

void func_actor_205200_8014C540(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_205200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_205200_8014C59C(Actor205200Ctx* arg0, Actor205200* arg1)
{
    GsCOORDINATE2*    coord;
    Actor205200Obj2C* obj;
    Actor205200Work*  work;
    s32               state;

    work  = arg1->field_1C;
    obj   = arg1->field_2C;
    coord = obj->field_8;
    if (Game_Session->field_1 != 0) {
        return;
    }
    if (work->field_594 != 0) {
        arg1->field_30 = 2;
        return;
    }
    state = D_801153F4;
    if (state == 1) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->field_C = 0;
    goto default_body;
case2:
    obj->field_C = 0x80;
    return;
default_body:
    func_actor_205200_8014BD4C(arg1);
    func_actor_205200_8014C67C(arg1);
    func_actor_205200_8014C7CC(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    ActorsShared80134ff0((ActorShared80134ff0*)arg1);
    func_actor_205200_8014C8D4(arg1);
}

/// Per-frame tick of the live state, run from `func_actor_205200_8014C59C`'s
/// shared body. Bit 0 of `Gp_StateF0.field_1D` is a one-shot re-arm: it clears
/// itself and drops the actor back to sub-state 1 with the sub-state-0x586
/// counter restarted, which is what `func_actor_205200_8014C748` drives. The
/// sub-state at 0x584 then picks the idle or the charge handler, the halfword at
/// 0x596 which of the two shared ticks follows, and the flag at 0x588 keeps the
/// attack body running until that body clears it itself.
void func_actor_205200_8014C67C(Actor205200* arg0)
{
    Actor205200Work* work;

    work = arg0->field_1C;
    if (Gp_StateF0.field_1D & 1) {
        Gp_StateF0.field_1D &= 0xFE;
        work->field_584      = 1;
        work->field_586      = 0;
    }
    switch (work->field_584) {
        case 0:
            func_actor_205200_8014BF28(arg0);
            break;
        case 1:
            func_actor_205200_8014C748(arg0);
            break;
    }
    if (work->field_596 == 0) {
        func_8017EBA4(arg0);
    } else {
        func_80181930(arg0);
    }
    if (work->field_588 != 0) {
        func_actor_205200_8014C0C0(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C748);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C7CC);
