#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_100300.h"
#include "actors/actor_105100.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"

#include "main/task.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105100_80131E24;

void func_8017FC40(GsCOORDINATE2* arg0, s32 arg1, u16 arg2);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8018294C(Actor105100* arg0);
void func_actor_105100_80132C2C(Actor105100* arg0);
void func_actor_105100_80133134(Actor105100* arg0);
void func_actor_105100_8013329C(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_8013345C(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_801336B8(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_80133A14(Actor105100* arg0, Actor105100Ctx* arg1);
void func_actor_105100_80133CE4(Actor105100* arg0);
void func_actor_105100_80134130(Actor105100* arg0);
void func_actor_105100_80134284(Actor105100Ctx* arg0, Actor105100* arg1);
void func_actor_105100_80135674(Actor105100* arg0);
void func_actor_105100_801359B4(Actor105100* arg0);
void func_actor_105100_80135B40(Actor105100* arg0);
void func_actor_105100_80135E54(Actor105100* arg0);
void func_actor_105100_80135F50(Actor105100* arg0);
void func_actor_105100_80135FCC(Actor105100* arg0);
void func_actor_105100_801360AC(Actor105100* arg0);
void func_actor_105100_801361C4(Actor105100* arg0);
void func_actor_105100_80136318(Actor105100* arg0);
void func_actor_105100_80136408(Actor105100* arg0);
void func_actor_105100_80136524(Actor105100* arg0);

void func_800B4114(Actor105100Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

extern u8 D_801153F4;

/// Main-executable globals with no module header yet: a `D_80114C12` of 1 or a
/// live `D_80071075` means a cutscene is already up, so the death handler skips
/// message 0x13F4.
extern u8 D_80071075;
extern s8 D_80114C12;

/// Main-executable global with no module header yet: bit 2 asks the per-frame
/// handler for the post-hit reaction, which is why `func_actor_105100_80133134`
/// runs `func_actor_105100_80135FCC` off it on every frame it is set.
extern u8 D_8011540D;

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
/// into the work's third list node (`Actor105100Work::obj4E4.key`), and the
/// `GpPairSrcE` at 0x80141398 is the pair source the context points at with
/// `Actor105100Ctx::field_50` -- its `hpMax` seeds the enemy's HP.
extern GpU16Pair  D_actor_105100_80141380;
extern GpPairSrcE D_actor_105100_80141398;

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_105100_80141488[];

/// Animation block the attack body hands the player with message 0x3F4.
extern void* D_actor_105100_801414B4;

/// The approach points the `field_40 == 1` reaction walks the model through,
/// indexed by `Actor105100Rec::field_44`. Only the x and z halves are read: the
/// reaction subtracts the model's current position and walks the resulting
/// planar delta.
extern SVECTOR D_actor_105100_80141418[6];

extern SVECTOR D_actor_105100_801413E8[];
extern s16     D_actor_105100_80141448[];
extern s16     D_actor_105100_80141450[];

void func_actor_105100_80135DF8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_80135E54(Actor105100* arg0)
{
    Actor105100Ctx*  enemy;
    Actor105100Work* work;
    s32              state;
    s32              damage;
    s32              tick;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
    }
    if ((enemy->field_4C & 2) && (work->field_596 != 5)) {
        work->field_596 = 5;
        work->field_598 = 0;
        work->field_5C2 = 1;
    }
    if (enemy->field_4C & 0xC) {
        tick = Gp_TickObjFlag4((GpObj5C*)enemy) << 0x10;
        if (tick != 0) {
            damage = tick >> 0x12;
            func_800DA6E8(&enemy->node, damage, 0);
            state           = enemy->field_40 - damage;
            enemy->field_40 = state;
            state         <<= 0x10;
            if (state <= 0) {
                state = 7;
            } else {
                state = 6;
            }
            work->field_596 = state;
            work->field_598 = 0;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }
}

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
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
}

/// Opening stage of the `field_598` schedule: arms pose 8, releases the held
/// effect slot and drops the `obj4E4.flags` pose bit, then waits on
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
            eff                = work->field_55C;
            work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
            if (eff != NULL) {
                eff->task->state = 4;
                work->field_55C  = NULL;
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
            eff                = work->field_55C;
            work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
            if (eff != NULL) {
                eff->task->state = 4;
                work->field_55C  = NULL;
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
/// 0x8000 bit of `obj4E4.flags` and pins the task to the tick handler (`state` 2);
/// once the count is spent it puts the enemy's HP (`Actor105100Ctx::field_40`)
/// at 1 and arms pose 6, leaving `state` alone.
///
/// The work block is read twice on purpose. The two loads do not CSE (the
/// `field_5B4` / `field_5AC` stores sit between them), and the first pointer is
/// still live at the tail for `obj4E4.flags` and `field_55C`, so the second one
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

    eff                = work->field_55C;
    work->obj4E4.flags = work->obj4E4.flags & 0x7FFF;
    if (eff != NULL) {
        eff->task->state = 4;
        work->field_55C  = NULL;
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
