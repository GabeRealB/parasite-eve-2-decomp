#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "actors/actor_105100.h"
#include "actors/actors_shared_80134ff0.h"
#include "actors/actors_shared_80136574.h"
#include "main/sound.h"

#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105100_80131E24;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8018294C(Actor105100* arg0);
void func_actor_105100_80132C2C(Actor105100* arg0);
void func_actor_105100_80133134(Actor105100* arg0);
void func_actor_105100_80133CE4(Actor105100* arg0);
void func_actor_105100_80134130(Actor105100* arg0);
void func_actor_105100_80135E54(Actor105100* arg0);
void func_actor_105100_80136408(Actor105100* arg0);
void func_actor_105100_80136524(Actor105100* arg0);

void func_800B4114(Actor105100Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

/// The run of HP caps at 0x8014139C; `func_actor_105100_80135FCC` reads the
/// first entry. Declared as an aggregate on purpose: a bare `extern u16` makes
/// `true_dependence` (`sched.c:846`) drop the dependence between the store to
/// `Actor105100Ctx::field_40` and this load -- the store is in-struct with a
/// varying address, this load a scalar MEM at a fixed one -- and sched2 then
/// hoists this load above the store, ahead of the `sll`.
extern u16 D_actor_105100_8014139C[1];

/// The s16 animation-id run at 0x801414C8, one entry per work state at
/// `Actor105100Work::field_58E`; `func_actor_105100_80136408` reads the entry
/// the new state selects before it re-queues every slot.
extern s16 D_actor_105100_801414C8[];

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80131EBC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80132414);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801327B4);

void func_actor_105100_80132AA0(Actor105100Ctx* arg0, Actor105100* arg1)
{
    GsCOORDINATE2*    coord;
    Actor105100Obj2C* obj;
    Actor105100Work*  work;
    s32               state;
    s32               one;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    one   = 1;
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
    obj->field_C       = 0;
    arg0->node.field_4 = 8;
    if (work->field_5BC != 0) {
        SndEvt_EnqueueType9(0x40000000);
        work->field_5BC = 0;
    }
    goto default_body;
case1:
    ActorsShared80134ff0((ActorShared80134ff0*)arg1);
    func_actor_105100_80136524(arg1);
    goto join_12;
case2:
    obj->field_C       = 0x80;
    arg0->node.field_4 = one;
join_12:
    SOFT_USE_REG(work);
    if (work->field_5BC == 0) {
        SndEvt_EnqueueType8(0x40000000);
    }
    work->field_5BC = state;
    return;
default_body:
    if (arg0->field_4C != 0) {
        func_actor_105100_80135E54(arg1);
    }
    func_actor_105100_80132C2C(arg1);
    func_actor_105100_80133134(arg1);
    if (work->field_5A2 != 0) {
        func_actor_105100_80133CE4(arg1);
    }
    func_actor_105100_80136408(arg1);
    func_actor_105100_80134130(arg1);
    ActorsShared80136574((ActorShared80136574*)arg1, &work->field_560, work->field_594, 1);
    if (work->field_5A8 != 0) {
        func_8018294C(arg1);
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    ActorsShared80134ff0((ActorShared80134ff0*)arg1);
    func_actor_105100_80136524(arg1);
}

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100", D_actor_105100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100", D_actor_105100_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80132C2C);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80133134);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_8013329C);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_8013345C);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801336B8);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80133A14);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80133CE4);

void func_actor_105100_80134130(Actor105100* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor105100Work* work;
    GsCOORDINATE2*   self;
    GpAnimRec*       rec;

    work = arg0->field_1C;
    self = arg0->field_2C->field_8;
    rec  = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->obj38.prev);
    if (rec != NULL) {
        if (!(rec->field_3 & 0x20) && (work->field_5B8 & 0x20)) {
            snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x40330001;
            pan = (s8)Gp_GetObjPan((GpObj38*)self);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)self));
        }
        if (!(rec->field_3 & 0x10) && (work->field_5B8 & 0x10)) {
            snd  = ((arg0->field_20->field_8 >> 12) << 8) | 0x40330002;
            pan2 = (s8)Gp_GetObjPan((GpObj38*)self);
            SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)self));
        }
        work->field_5B8 = (u16)(rec->field_3 & 0x30);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80134284);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801347D4);

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100", D_actor_105100_80131E90);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80134B00);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135278);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801354E8);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135674);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801359B4);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135B40);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135CEC);

void func_actor_105100_80135DF8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135E54);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135F50);

void func_actor_105100_80135FCC(Actor105100* arg0)
{
    Actor105100Ctx* enemy;
    GsCOORDINATE2*  coord;
    s32             snd;
    s32             pan;
    u16             hp;

    enemy                = arg0->field_20;
    coord                = arg0->field_2C->field_8;
    Gp_StateF0.field_1D &= 0xFB;
    hp                   = enemy->field_40 + 0x50;
    enemy->field_40      = hp;
    if (D_actor_105100_8014139C[0] < (s16)hp) {
        enemy->field_40 = D_actor_105100_8014139C[0];
    }
    func_800DA6E8(&enemy->node, -0x50, 0);
    Gp_SpawnEff(0x601AF, NULL, 0, NULL);
    snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x4033000C;
    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801360AC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801361C4);

void func_actor_105100_801362A0(Actor105100* arg0)
{
    Actor105100Work* work;
    s32              snd;

    work = arg0->field_1C;

    snd = work->field_580;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_580 = 0;
    }
    snd = work->field_584;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_584 = 0;
    }
    snd = work->field_588;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        work->field_588 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80136318);

void func_actor_105100_80136408(Actor105100* arg0)
{
    Actor105100Work* work;
    s32              i;
    s32              val;

    work = arg0->field_1C;
    i    = 1;
    if ((s16)work->field_58E != work->field_590) {
        work->field_590 = work->field_58E;
        work->field_592 = 0;
        val             = D_actor_105100_801414C8[(s16)work->field_58E];
        do {
            func_800B4114(work, i, (s16)work->field_58E, 0, val);
            i++;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        work->field_592 += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}
