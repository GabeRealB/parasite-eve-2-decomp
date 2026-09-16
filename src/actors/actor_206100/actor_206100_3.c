#include "common.h"

#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"

/// Task table `func_actor_206100_8014FDE8` spawns the shockwave from.
extern TaskDesc D_801818BC;

void func_actor_206100_8014DEAC(Task* task);
void func_actor_206100_8014F284(Task* task);
void func_actor_206100_8014F2F0(Task* task);
s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
void func_actor_206100_8014F8BC(Task* task);
void func_actor_206100_8014F970(Task* task);
void func_actor_206100_8014F9C4(Task* task);
void func_actor_206100_8014FA08(Task* task);

/// Runs the actor's sub-state handler for the current `field_522`, after
/// marking the enemy's list node so the exit path tears the actor down.
void func_actor_206100_8014F524(Task* task)
{
    Actor206100Work* work;
    GpEnemy*         enemy;
    TaskFuncTable5   sp;

    work                = (Actor206100Work*)task->idMap;
    enemy               = (GpEnemy*)task->spawnArg2;
    sp                  = D_actor_206100_80149E94;
    enemy->node.field_4 = 1;
    sp.funcs[(s16)work->field_522](task);
}

void func_actor_206100_8014F59C(void)
{
}

void func_actor_206100_8014F5A4(void)
{
}

void func_actor_206100_8014F5AC(void)
{
}

void func_actor_206100_8014F5B4(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->idMap;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F8BC,
        func_actor_206100_8014F970,
    };

    states[(s16)work->field_522](task);
}

void func_actor_206100_8014F608(Task* task)
{
    Actor206100Work* work                = (Actor206100Work*)task->idMap;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F9C4,
        func_actor_206100_8014FA08,
    };

    states[(s16)work->field_522](task);
}

void func_actor_206100_8014F65C(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    func_actor_206100_8014DEAC(task);
    Gp_MsgPlayerWeapon(0);
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F69C(Task* task)
{
    u16              timer;
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    func_actor_206100_8014DEAC(task);
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x5A) {
        work->field_51E = 0;
        work->field_522 = work->field_522 + 1;
    }
}

void func_actor_206100_8014F6F8(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* anim;

    work            = (Actor206100Work*)task->idMap;
    work->field_51E = 0;
    anim            = (Actor206100Work*)task->idMap;
    anim->field_524 = 8;
    anim->field_51A = 8;
    anim->field_510 = 7;
    anim->field_50C = 1;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F738(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    work->field_524 = 0xA;
    work->field_51A = 0x10;
    work->field_510 = 1;
    work->field_50C = 1;
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F770(Task* task)
{
    u16              timer;
    Actor206100Work* work;
    Actor206100Work* next;

    work            = (Actor206100Work*)task->idMap;
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x5B) {
        next            = (Actor206100Work*)task->idMap;
        next->field_520 = 3;
        next->field_522 = 0;
    }
}

void func_actor_206100_8014F7B4(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    s32              soundId;
    s32              pan;

    work    = (Actor206100Work*)task->idMap;
    soundId = ((((GpEnemy*)task->spawnArg2)->field_8 >> 0xC) << 8) | 0x551E0005;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
    next            = (Actor206100Work*)task->idMap;
    next->field_524 = 0xA;
    next->field_51A = 0x10;
    next->field_510 = 1;
    next->field_50C = 1;
    work->field_526 = 0x1388;
    work->field_51E = 0;
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F878(Task* task)
{
    u16              timer;
    Actor206100Work* work;
    Actor206100Work* next;

    work            = (Actor206100Work*)task->idMap;
    timer           = work->field_51E + 1;
    work->field_51E = timer;
    if ((s16)timer >= 0x3D) {
        next            = (Actor206100Work*)task->idMap;
        next->field_520 = 1;
        next->field_522 = 0;
    }
}

void func_actor_206100_8014F8BC(Task* task)
{
    Actor206100Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor206100Work*)task->idMap;
    work->field_524 = 6;
    work->field_51A = 0x10;
    work->field_510 = 0xA;
    work->field_50C = 1;
    soundId         = ((((GpEnemy*)task->spawnArg2)->field_8 >> 0xC) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
    work->field_522 = work->field_522 + 1;
}

void func_actor_206100_8014F970(Task* task)
{
    Actor206100Work* work;
    s32              cond;

    work = (Actor206100Work*)task->idMap;
    if ((work->flags_514.half & 1) || (work->flags_514.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor206100Work*)task->idMap;
        work->field_520 = 2;
        work->field_522 = 0;
    }
}

void func_actor_206100_8014F9C4(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->idMap;

    work->field_524 = 8;
    work->field_51A = 0x10;
    work->field_510 = 0xE;
    work->field_50C = 1;
    work->field_51E = 0;
    work->field_526 = work->field_536;
    work->field_522 = work->field_522 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_3", func_actor_206100_8014FA08);
/// Ring-spawn state: seeds `field_4F4` and `field_548` from the eight-point ring
/// `D_actor_206100_80158B68`, copies the current vertex into the root part
/// coordinate, advances the index modulo 8, and hands the actor the state-1
/// animation request.  `coord` is the coordinate the effect argument at
/// `eff_4C0` shares, so moving it moves the actor.
///
/// `enemy` is a local rather than the inline
/// `((GpEnemy*)task->spawnArg2)->node.field_4 = 1;` because the fused form
/// transposes the `spawnArg2` and `task->extra` loads; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store, so give the pointer its own local".
void func_actor_206100_8014FAE4(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor206100Work* work;
    Actor206100Work* next;
    Actor206100Work* last;
    GpEnemy*         enemy;

    work                = (Actor206100Work*)task->idMap;
    enemy               = (GpEnemy*)task->spawnArg2;
    coord               = ((TmdObject*)task->extra)->field_8;
    enemy->node.field_4 = 1;
    work->field_54D     = 1;
    work->field_548     = 0;
    work->field_4F4     = D_actor_206100_80158B68;
    next                = (Actor206100Work*)task->idMap;
    next->field_51A     = 0x10;
    next->field_510     = 3;
    next->field_50C     = 2;
    work->field_43E     = 0x400;
    coord->coord.t[0]   = work->field_4F4[work->field_548].field_0;
    coord->coord.t[1]   = work->field_4F4[work->field_548].field_2;
    coord->coord.t[2]   = work->field_4F4[work->field_548].field_4;
    work->field_548     = (work->field_548 + 1) & 7;
    Gp_SetLightMode((GpObj4C*)task->spawnArg2, 2);
    work->field_51E = 0;
    last            = (Actor206100Work*)task->idMap;
    last->field_520 = 1;
    last->field_522 = 0;
}

void func_actor_206100_8014FBE4(Task* task)
{
    Actor206100Work* work;
    GpEnemy*         enemy;
    s32              soundId;
    s32              pan;

    work  = (Actor206100Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    SndEvt_EnqueueType7(0x551E0002, 1);
    Gp_ApplyAreaRecs(&D_8018590C);
    work->field_526 = work->field_536;
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)task, 0);
    GameFlag_SetNibble(0xF3, 1);
    enemy->field_54 = 0;
    Gp_UnlinkObj(&work->obj_364);
    Gp_UnlinkObj(&work->obj_414);
    work->field_51E = 0;
    work->field_520 = work->field_520 + 1;
    soundId         = ((((GpEnemy*)task->spawnArg2)->field_8 >> 0xC) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan,
                        (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
}

/// Idle-state tick: re-arms the animation request, then advances the clip
/// phase `field_512` -- reset when the requested clip is not the one playing,
/// otherwise ramped by `func_actor_206100_8014F3C8` -- ticks every animation
/// slot and bumps the frame counter.
///
/// `next` is the same block as `work` loaded a second time: the first four
/// stores reach it through one local and everything after the request-kind
/// read through the other, which is what the two loads of `Task::idMap` are.
///
/// The loop is written `for (i = 1; i < 0xF; i++)` rather than as the
/// `do`/`while` its test-at-the-bottom shape suggests, and its initialiser sits
/// *after* the sub-state chain rather than before it.  Placed before the chain,
/// the store that materialises `i` is the one the case-3 branch jumps over, and
/// post-reload CSE then rewrites the phase's `+ 1` into `+ $s0`; after the chain
/// that store is the branch's own target, reorg copies it into the delay slot
/// and threads the branch past it.  See `DECOMPILATION_LEARNINGS.md`, "A
/// constant store in a delay slot decides whether post-reload CSE folds it into
/// a later increment".
void func_actor_206100_8014FCD4(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    s32              i;
    s16              state;

    work            = (Actor206100Work*)task->idMap;
    work->field_524 = 4;
    work->field_51A = 0x10;
    work->field_510 = 0xE;
    work->field_50C = 1;
    next            = (Actor206100Work*)task->idMap;
    state           = next->field_50C;
    if (state == 1) {
        if (next->field_50E != next->field_510) {
            next->field_512 = 0;
        } else {
            next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
        }
        func_actor_206100_8014F2F0(task);
        next->field_50C = 3;
    } else if (state == 2) {
        func_actor_206100_8014F284(task);
        next->field_50C = 3;
        next->field_512 = 0;
    } else if (state == 3) {
        next->field_512 = next->field_512 + 1;
    }
    for (i = 1; i < 0xF; i++) {
        Gp_AnimTickIndex(&next->anim, i);
    }
    work->field_520 = work->field_520 + 1;
}
/// Idle-state tick: advances the actor's two frame counters, keeps the root
/// coordinate dirty so `GsGetLw` rebuilds it, spawns the shockwave task once the
/// counter reaches 0x5A and retires the actor four frames later.
///
/// `coord` is a local rather than the inline
/// `((TmdObject*)task->extra)->field_8->flg = 0;` because the fused form loads
/// `task->extra` *after* the two counter stores, and sched1 will not lift a load
/// above an earlier store; its address load stays with the stores and both pick
/// up load-delay nops.  Binding the pointer above the counters frees the two
/// loads to be scheduled first, which is the target's order; see
/// `DECOMPILATION_LEARNINGS.md`, "A dereference-store's address load is ranked
/// with its store".
void func_actor_206100_8014FDE8(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;

    coord           = ((TmdObject*)task->extra)->field_8;
    work            = (Actor206100Work*)task->idMap;
    work->field_51E = work->field_51E + 1;
    work->field_526 = work->field_526 + 0x10;
    coord->flg      = 0;
    if ((s16)work->field_51E == 0x5A) {
        Task_SpawnFromTable(&D_801818BC, 0, 0, 0);
    }
    if ((s16)work->field_51E >= 0x10E) {
        task->state     = 4;
        next            = (Actor206100Work*)task->idMap;
        next->field_520 = 0;
        next->field_522 = 0;
    }
}
