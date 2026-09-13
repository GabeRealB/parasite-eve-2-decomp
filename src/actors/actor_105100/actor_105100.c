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

void func_8017FC40(GsCOORDINATE2* arg0, s32 arg1, u16 arg2);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8018294C(Actor105100* arg0);
void func_actor_105100_80132C2C(Actor105100* arg0);
void func_actor_105100_80133134(Actor105100* arg0);
void func_actor_105100_80133CE4(Actor105100* arg0);
void func_actor_105100_80134130(Actor105100* arg0);
void func_actor_105100_80135674(Actor105100* arg0);
void func_actor_105100_801359B4(Actor105100* arg0);
void func_actor_105100_80135B40(Actor105100* arg0);
void func_actor_105100_80135E54(Actor105100* arg0);
void func_actor_105100_80135F50(Actor105100* arg0);
void func_actor_105100_80136408(Actor105100* arg0);
void func_actor_105100_80136524(Actor105100* arg0);

void func_800B4114(Actor105100Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

/// Main-executable global with no module header yet: the remaining-enemy count
/// `func_actor_105100_80136318` tests to decide whether the fight is over.
extern s16 D_80073BA0;

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

/// The spawn's pair tables. `Gp_PackPair` packs the `GpU16Pair` at 0x80141380
/// into the work's third list node (`Actor105100Work::field_4FC`), and the
/// `GpPairSrcE` at 0x80141398 is the pair source the context points at with
/// `Actor105100Ctx::field_50` -- its `field_4` seeds the enemy's HP.
extern GpU16Pair  D_actor_105100_80141380;
extern GpPairSrcE D_actor_105100_80141398;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80131EBC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80132414);

/// Spawn/setup handler. It allocates the 0x5C4-byte work block and hangs it off
/// the task, points the model object at the block's two `MATRIX`es (0x45C the
/// light matrix, 0x43C the colour one) and fills the context's coordinate,
/// pair source and HP (`field_40`, seeded from the pair source's `field_4`).
///
/// The block's 0x14-prefix then becomes the `GpAnimCtx`: `func_800B3F84` loads
/// the animation data into it over the nineteen `GpAnimSlot`s, and slots 1..18
/// are reset. The three list nodes at 0x47C / 0x4E4 / 0x51C are linked into the
/// global object lists with their collision tables (`Gp_InitRec18Table`), which
/// also sets each node's 0x8000 "last element" flag -- then the second node's is
/// cleared again. `&coord[3]` -- the actor's fourth coordinate -- is what the
/// first node, `field_554` and the context's `field_18` all hang off.
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler; otherwise the task moves to the tick handler (`state` 1).
void func_actor_105100_801327B4(Actor105100Ctx* arg0, Actor105100* arg1)
{
    Actor105100Work*  work;
    Actor105100Obj2C* obj;
    GsCOORDINATE2*    coord;
    GpRec18*          records1;
    GpRec18*          records2;
    GpRec18*          records3;
    s32               i;

    obj   = arg1->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x5C4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->field_1C = work;
    obj->field_C   = 0;
    coord->flg     = 0;
    obj->field_1C  = &work->field_45C;
    obj->field_20  = &work->field_43C;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode((GpLinkNode*)&arg0->node);
    arg0->field_18  = &arg1->field_2C->field_8[3];
    arg0->field_1C  = 0;
    arg0->field_20  = 0x64;
    arg0->field_24  = 0;
    arg0->field_50  = &D_actor_105100_80141398;
    arg0->field_54  = (s32)work->field_49C;
    arg0->field_40  = D_actor_105100_80141398.field_4;
    work->field_554 = &arg1->field_2C->field_8[3];
    work->field_558 = 0x500;
    work->field_55A = 3;
    func_800B3F84((GpAnimCtx*)work, D_actor_105100_80141488, (GpAnimObj*)obj, work->field_30C,
                  ((Actor105100Anim*)work)->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_560 = coord->coord;
    work->field_594 = 0x2800;
    work->field_5A8 = 1;
    work->field_59E = 0xF;
    work->field_59A = 0x96;
    work->field_484 = &arg1->field_2C->field_8[3];
    records1        = work->field_49C;
    work->field_488 = records1;
    work->field_48C = 0;
    work->field_48E = 0x1F4;
    work->field_490 = 0;
    work->field_494 = 0x30033;
    work->field_498 = 0x320;
    work->field_49A = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_47C);
    Gp_InitRec18Table(records1, 3, 0);
    work->field_49A = (u16)(work->field_49A | 0x8000);
    work->field_524 = arg1->field_2C->field_8;
    records2        = work->field_53C;
    work->field_528 = records2;
    work->field_52C = 0;
    work->field_52E = 0;
    work->field_530 = -0x12C;
    work->field_534 = 0;
    work->field_538 = 0x4B0;
    work->field_53A = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_51C);
    Gp_InitRec18Table(records2, 1, 0);
    work->field_53A = (u16)(work->field_53A | 0x8000);
    work->field_4EC = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    records3        = work->field_504;
    work->field_4F0 = records3;
    work->field_4F4 = 0;
    work->field_4F6 = 0;
    work->field_4F8 = 0;
    work->field_4FC = Gp_PackPair(&D_actor_105100_80141380, 5);
    work->field_500 = 0x1F4;
    work->field_502 = 1U;
    Gp_LinkObj(3, (GpObj*)work->field_4E4);
    Gp_InitRec18Table(records3, 1, 0);
    work->field_502 = work->field_502 & 0x7FFF;
    arg1->state     = 1;
}

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

/// The per-frame handler the `state == 1` dispatch runs: it hands the reaction
/// `field_40` selects to one of the `80135674` / `801359B4` / `80135B40`
/// sub-handlers, retimes the pose every 6/0xB/0x10 frames of the countdown in
/// `field_48`, and ends the fight (`state = 2`) once that countdown, the work's
/// `field_24` and the parent's `field_5AC` all say so.
void func_actor_105100_801354E8(Actor105100Ctx* arg0, Actor105100* arg1)
{
    Actor105100Rec*  rec;
    Actor105100Work* parentWork;
    GsCOORDINATE2*   coord;
    s32              state;
    s32              one;
    s16              timer;
    u16              count;

    rec        = (Actor105100Rec*)arg1->field_1C;
    parentWork = ((Actor105100*)arg1->parent)->field_1C;
    state      = D_801153F4;
    coord      = arg1->field_2C->field_8;
    one        = 1;

    if (state == one) {
        func_8017FC40(coord, 0x80, rec->field_4E);
        return;
    }
    if (state < 2) {
        goto default_body;
    }
    if (state == 2) {
        goto done;
    }
default_body:
    if (rec->field_40 == one) {
        goto rec1;
    }
    if (rec->field_40 >= 2) {
        goto ge2;
    }
    if (rec->field_40 == 0) {
        goto rec0;
    }
    goto join;
ge2:
    if (rec->field_40 == 2) {
        goto rec2;
    }
    goto join;
rec0:
    func_actor_105100_80135674(arg1);
    goto join;
rec1:
    func_actor_105100_801359B4(arg1);
    goto join;
rec2:
    func_actor_105100_80135B40(arg1);
join:
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    timer = rec->field_48;
    if (timer < 6) {
        rec->field_4E = 0;
    } else if (timer < 0xB) {
        rec->field_4E = 1;
    } else if (timer < 0x10) {
        rec->field_4E = 2;
    }
    func_8017FC40(coord, 0x80, rec->field_4E);
    count         = (u16)rec->field_48 - 1;
    rec->field_48 = count;
    if ((count << 16) <= 0 || ((Actor105100Work*)rec)->field_24 != 0 ||
        parentWork->field_5AC == 0) {
        parentWork->field_5AE = parentWork->field_5AE - 1;
        arg1->state           = 2;
    }
done:
    return;
}

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

/// Second step of the `field_598` schedule: arms pose 2 with the `field_59A`
/// timer at 0x3C frames, then, when the timer runs out, hands the pose back to
/// the schedule entry step and returns it to 0.
void func_actor_105100_80135F50(Actor105100* arg0)
{
    Actor105100Work* work;
    s32              state;
    u16              timer;

    work  = arg0->field_1C;
    state = work->field_598;
    switch (state) {
        case 0:
            work->field_58E = 2;
            work->field_59A = 0x3C;
            work->field_598 = 1;
            break;
        case 1:
            timer           = work->field_59A - 1;
            work->field_59A = timer;
            if ((timer << 16) <= 0) {
                work->field_5A8 = state;
                work->field_58E = state;
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_59A = 0;
            }
            break;
    }
}

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

/// Opening stage of the `field_598` schedule: arms pose 8, releases the held
/// effect slot and drops the `field_502` pose bit, then waits on
/// `Gp_TickObjFlag2` before clearing the enemy's flag-2 bit. On the last stage
/// it waits out the `field_592` timer and returns the schedule to step 0.
void func_actor_105100_801360AC(Actor105100* arg0)
{
    Actor105100Work* work;
    Actor105100Ctx*  enemy;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->field_1C;
    state = work->field_598;
    enemy = arg0->field_20;
    switch (state) {
        case 0:
            work->field_58E = 8;
            work->field_598 = 1;
            if (work->field_5B4 != 0) {
                work->field_5B4 = 0;
                work->field_5B6 = 1;
                work->field_5AA = 0;
            }
            work->field_5AC = 0;
            func_actor_105100_801362A0(arg0);
            eff             = work->field_55C;
            work->field_502 = work->field_502 & 0x7FFF;
            if (eff != NULL) {
                eff->field_0->state = 4;
                work->field_55C     = NULL;
            }
            if (work->field_5B6 == 0) {
                work->field_5AA = 0x1E;
            }
            break;
        case 1:
            if (Gp_TickObjFlag2((GpObj5D*)enemy) != 0) {
                work->field_58E  = 9;
                work->field_598  = 2;
                work->field_5C2  = 0;
                enemy->field_4C &= 0xFD;
            }
            break;
        case 2:
            if ((s16)work->field_592 >= 0xB) {
                work->field_596 = 0;
                work->field_598 = 0;
                work->field_58E = 1;
            }
            break;
    }
}

/// First stage of the `field_598` schedule: arms the pose and the effect slot,
/// and on the next stage waits out the `field_592` timer before handing the
/// state back on, either aborting (0) or resuming (8) depending on `field_5C2`.
void func_actor_105100_801361C4(Actor105100* arg0)
{
    Actor105100Work* work;
    GpEffWork*       eff;
    s32              state;

    work  = arg0->field_1C;
    state = work->field_598;
    switch (state) {
        case 0:
            work->field_58E = 6;
            work->field_598 = 1;
            work->field_5B4 = 0;
            work->field_5AC = 0;
            func_actor_105100_801362A0(arg0);
            eff             = work->field_55C;
            work->field_502 = work->field_502 & 0x7FFF;
            if (eff != NULL) {
                eff->field_0->state = 4;
                work->field_55C     = NULL;
            }
            if (work->field_5B6 == 0) {
                work->field_5AA = 0x1E;
            }
            break;
        case 1:
            if ((s16)work->field_592 >= 0x1D) {
                if (work->field_5C2 == 0) {
                    work->field_596 = 0;
                    work->field_58E = state;
                } else {
                    work->field_596 = 5;
                    work->field_58E = 8;
                }
                work->field_598 = 0;
            }
            break;
    }
}

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

/// Last-enemy handler. While the remaining-enemy count is still positive it
/// retires the queued sound events, unlinks the running effect, drops the
/// 0x8000 bit of `field_502` and pins the task to the tick handler (`state` 2);
/// once the count is spent it puts the enemy's HP (`Actor105100Ctx::field_40`)
/// at 1 and arms pose 6, leaving `state` alone.
///
/// The work block is read twice on purpose. The two loads do not CSE (the
/// `field_5B4` / `field_5AC` stores sit between them), and the first pointer is
/// still live at the tail for `field_502` and `field_55C`, so the second one
/// needs a register of its own.
void func_actor_105100_80136318(Actor105100* arg0)
{
    Actor105100Work* work;
    Actor105100Work* sndWork;
    GpEffWork*       eff;
    s32              snd;

    work = arg0->field_1C;
    if (D_80073BA0 <= 0) {
        arg0->field_20->field_40 = 1;
        work->field_596          = 6;
        work->field_598          = 0;
        return;
    }

    work->field_5B4 = 0;
    work->field_5AC = 0;

    sndWork = arg0->field_1C;

    snd = sndWork->field_580;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_580 = 0;
    }
    snd = sndWork->field_584;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_584 = 0;
    }
    snd = sndWork->field_588;
    if (snd != 0) {
        SndEvt_EnqueueType7(snd, 1);
        sndWork->field_588 = 0;
    }

    eff             = work->field_55C;
    work->field_502 = work->field_502 & 0x7FFF;
    if (eff != NULL) {
        eff->field_0->state = 4;
        work->field_55C     = NULL;
    }

    arg0->state = 2;
}

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
