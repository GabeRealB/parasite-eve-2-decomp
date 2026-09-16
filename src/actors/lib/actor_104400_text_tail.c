#include "common.h"
#include "actors/actor_104400.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_8016945c.h"
#include "actors/actors_shared_801692e8.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

void Actor04400_Fn006A8(Task* arg0);
/* Reads the caller's Task* from $a0; the call passes no argument. */
void Actor04400_Fn02B8C();
s16  Actor04400_Fn06328(Task* arg0);
void Actor04400_Fn06374(Task* arg0, s32 arg1);
s16  Actor04400_Fn065F4(Task* arg0, s32 arg1);
s16  Actor04400_Fn06618(Task* arg0);
void Actor04400_Fn067A0(Task* arg0, s32 step);
void Actor04400_Fn06BC4(Task* arg0);
void Actor04400_Fn06BF8(Task* arg0);
void Actor04400_Fn03390(Task* arg0);
void Actor04400_Fn07968(Task* arg0);
void Actor04400_Fn07984(Task* arg0);
void Actor04400_Fn07A38(Task* arg0);
void Actor04400_Fn07B4C(Task* arg0);
void Actor04400_Fn07C60(Task* arg0);
void Actor04400_Fn08208(Task* arg0);
void Actor04400_Fn0823C(Task* arg0);
void Actor04400_Fn08718(Task* arg0);
void Actor04400_Fn087E0(Task* arg0);
s32  Actor04400_Fn08DBC(Task* arg0);
void Actor04400_Fn00220(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4, u8 arg5);

extern TaskFuncTable5 Actor04400_D001C4;
extern u32            Gp_LcgState;

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn03538);

/// Same body as `ActorsShared80166b20`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
///
/// Counts `field_412` up against the `field_446` hold and enters state 2 once
/// it runs out; over its last 0x30 frames, eases the yaw `field_424` back to
/// zero. Before that, while `field_43A` (the distance to the nearer player
/// actor) is under 0xDAC and the heading `field_444` is outside 0x3C0..0xC40,
/// eases the yaw toward it and enters state 3 (arming `Gp_ArmStateF0`) after
/// 16 such frames; otherwise swings it toward +-0x380 on bit 6 of
/// `field_442`.
void Actor04400_Fn039EC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    Actor104400Work* state;
    Actor104400Work* state2;
    s32              angle;
    s32              cur;
    s32              aim;

    if ((s16)++work->field_412 > work->field_446) {
        state            = (Actor104400Work*)arg0->idMap;
        state->field_420 = 2;
        state->field_422 = 0;
        return;
    }
    if (work->field_446 - 0x30 < (s16)work->field_412) {
        cur             = (u16)work->field_424;
        work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->field_43A < 0xDAC && (aim = (u16)work->field_444, (aim < 0x3C0 || aim > 0xC40))) {
        angle           = (u16)work->field_424;
        work->field_424 = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->field_42C >= 0x10) {
            Gp_ArmStateF0(1);
            state2            = (Actor104400Work*)arg0->idMap;
            state2->field_420 = 3;
            state2->field_422 = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `field_424`, which a single update after an if/else lacks.
        if (!(((u16)work->field_442 >> 6) & 1)) {
            work->field_424 = (u16)work->field_424 + ((s16)(0x3800 - (u16)work->field_424 * 16) >> 9);
        } else {
            work->field_424 = (u16)work->field_424 + ((s16)(-0x3800 - (u16)work->field_424 * 16) >> 9);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn03B34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn03CA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn03E20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn03F8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn042C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn045A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn04718);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn048A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn04A3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn04BA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn04D44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn04EDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn05040);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn05260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn053FC);

/// Same body as `ActorsShared80168a28`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn058F4(Task* arg0)
{
    Actor104400Work* objs;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    Actor104400Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor104400Work*)arg0->idMap;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    tmd             = (TmdObject*)arg0->extra;
    work->field_438 = 1;
    if (enemy->field_40 >= 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    objs            = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&objs->obj_2AC);
    Gp_UnlinkObj(&objs->obj_2CC);
    Gp_UnlinkObj(&objs->obj_3AC);
    ActorsShared_SetTaskState(arg0, 5);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 0, 0);
    tmd->field_C |= 0x80;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn05A40);

/// `ActorsShared8013a2c0`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
/// This overlay's copy lives in `actor_104400_text.c`; the tail unit needs its
/// own because the two are separate translation units.
static __inline__ void Actor04400_UpdateColor(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Same body as `func_actor_342400_80168F14`. This overlay's whole `.text` is
/// already one shared span, so it cannot join that unit.
///
/// The per-frame callback the actor's AI states are dispatched from: state 0
/// counts `field_442` up, runs the handler `field_420` selects from
/// `Actor04400_D001C4` and spawns effect 3 on the model's second coordinate
/// part every 32 frames, then falls into state 1, which re-pushes that
/// coordinate's world position for `Gp_UpdateActorColor` and rebuilds the
/// part-pair colour quads while `field_451` is clear. `D_801153F4` short-
/// circuits both: nonzero runs state 1 only, 2 hides the model instead.
void Actor04400_Fn05DE0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable5   sp    = Actor04400_D001C4;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->field_C &= ~0x80;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn05FC8);

/// Same body as `ActorsShared801692e8`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
///
/// Queues CD command 0x21 once, guarded by `D_80115415`: the first parameter
/// block selects 2 or 3 when session `field_7` is 4, `field_6` is 0x27 or 0x28
/// and `field_9` is 1 or 2 respectively, and 1 otherwise.
void Actor04400_Fn061B4(void)
{
    u8 param1[8];
    u8 param2[8];

    if (D_80115415 == 0) {
        /* Each branch makes its own call; jump2's cross-jumping merges the
         * identical tails after sched2, which is why the argument setup is
         * duplicated per branch in the target. */
        if (Game_Session->field_7 == 4 && (u32)(Game_Session->field_6 - 0x27) < 2 && Game_Session->field_9 == 1) {
            param1[2] = 0xA;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else if (Game_Session->field_7 == 4 && (u32)(Game_Session->field_6 - 0x27) < 2 && Game_Session->field_9 == 2) {
            param1[2] = 0xA;
            param1[0] = 3;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else {
            param1[2] = 0xA;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        }
        D_80115415 = 1;
    }
}

/// Walks the death sequence's two-state handler table on the work block's
/// state index, the same shape as `func_actor_341700_80168124`.
void Actor04400_Fn062D4(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        Actor04400_Fn08718,
        Actor04400_Fn087E0,
    };

    states[(s16)work->field_420](arg0);
}

/// Same body as `ActorsShared8016945c`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
s16 Actor04400_Fn06328(Task* arg0)
{
    if ((s8)Gp_StateF0.field_1F & 0x80) {
        ActorsShared_SetTaskState(arg0, 3);
        ActorsShared_SetWorkState(arg0, 5);
        return 1;
    }
    return 0;
}

/// Claims or releases this actor's spawn place in `Gp_StateF0.field_1F`:
/// `arg1` non-zero sets bit 7 from the place id in bits 12+ of the spawn
/// descriptor (unless the place is already claimed), and `arg1` zero clears the
/// byte when its low nibble still matches that place. Spawn paths pass 1,
/// despawn paths pass 0; `Actor04400_Fn06328` reads bit 7 back.
///
/// Same body as `func_actor_341700_801681C4` in `actor_341700` and
/// `func_actor_342400_801694A8` in `actor_342400` — all three are byte-identical
/// — but those are separate packages whose copies sit at their own link
/// offsets, so this one cannot join them.
void Actor04400_Fn06374(Task* arg0, s32 arg1)
{
    if ((arg1 << 0x10) != 0) {
        if (!((s8)Gp_StateF0.field_1F & 0x80)) {
            Gp_StateF0.field_1F = (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) | 0x80;
        }
    } else if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
}

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 Actor04400_Fn063E4(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 6;
                w->field_422       = 0;
                break;
            }
            case 1: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 2: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 8;
                w->field_422       = 0;
                break;
            }
            case 3: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 4: {
                Actor104400Work* w = (Actor104400Work*)arg0->idMap;
                w->field_420       = 9;
                w->field_422       = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `field_44C`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test. `arg1` is the dispatch's
/// handler index and is unused here.
void Actor04400_Fn0648C(Task* arg0, s32 arg1, Actor104400Msg* arg2)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if (arg2->field_0 == 0x2C00) {
        switch (arg2->field_2 & 0xF) {
            case 1:
                work->field_44C = arg2->field_2;
                break;
            case 2:
                work->field_44C = arg2->field_2;
                break;
            case 3:
                work->field_44C = arg2->field_2;
                break;
            case 4:
                work->field_44C = arg2->field_2;
                break;
            case 5:
                work->field_44C = arg2->field_2;
                break;
        }
    }
}

void Actor04400_Fn064EC(Task* task, s16 part, VECTOR3* pos)
{
    GsCOORDINATE2* coord;

    coord             = ((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    coord->flg        = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06520);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn065F4);

/// Same body as `ActorsShared8016974c`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
s16 Actor04400_Fn06618(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06658);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn066DC);

/// Runs the intro's two-state handler table on the work block's state index,
/// the same shape as `Actor04400_Fn062D4`.
void Actor04400_Fn0674C(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07968,
        Actor04400_Fn07984,
    };

    states[(s16)work->field_420](arg0);
}

/// Same body as `ActorsShared801698d4`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn067A0(Task* arg0, s32 step)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    SVECTOR          vec;
    s32              diff;
    u16              angle;
    s32              yaw;

    vec.vx = work->field_88;
    vec.vy = 0;
    vec.vz = work->field_8C;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->field_7A;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_7A = angle - step;
    } else if (diff < -0x100) {
        work->field_7A = angle + step;
    }
}

void Actor04400_Fn06834(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn06848(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn0685C(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// Copies this overlay's `Actor04400_D0007C` dispatcher table onto the stack and
/// lets the pending-request handler `Actor04400_Fn063E4` consume the request
/// first: the table entry `field_422` selects runs only when nothing was
/// consumed.
void Actor04400_Fn06870(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->idMap;
    sp   = Actor04400_D0007C;
    if ((s16)Actor04400_Fn063E4(arg0) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn068F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06964);

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// encounter's animation (`Actor04400_Fn07A38`, which then advances `field_422`
/// itself), entry 1 runs the handler that answers a pending request or a hit
/// (`Actor04400_Fn03390`), chosen by the sub-state index `field_422`.
void Actor04400_Fn069D0(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07A38,
        Actor04400_Fn03390,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// animation the encounter asked for (`Actor04400_Fn07B4C`), entry 1 finishes
/// the encounter (`Actor04400_Fn07C60`), chosen by the sub-state index
/// `field_422`.
void Actor04400_Fn06A24(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07B4C,
        Actor04400_Fn07C60,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 advances the
/// animation sub-state (`Actor04400_Fn06BC4`), entry 1 runs the pending-request
/// handler (`Actor04400_Fn06BF8`), chosen by the sub-state index `field_422`.
void Actor04400_Fn06A78(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        Actor04400_Fn06BC4,
        Actor04400_Fn06BF8,
    };

    states[(s16)work->field_422](arg0);
}

void Actor04400_Fn06ACC(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->idMap;
    sp   = Actor04400_D00070;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        Actor04400_Fn08DBC(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06B50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06BC4);

void Actor04400_Fn06BF8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work            = (Actor104400Work*)arg0->idMap;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

void Actor04400_Fn06C70(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    work->field_44F = Actor04400_D10814[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor104400Work* w = (Actor104400Work*)arg0->idMap;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor104400Work* w = (Actor104400Work*)arg0->idMap;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}

void Actor04400_Fn06CF0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* slow;
    Actor104400Work* fast;

    work = (Actor104400Work*)arg0->idMap;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor104400Work*)arg0->idMap;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor104400Work*)arg0->idMap;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

void Actor04400_Fn06D90(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if (Actor04400_Fn06618(arg0)) {
        if (work->field_44F == 1) {
            Actor104400Work* w = (Actor104400Work*)arg0->idMap;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor104400Work* w = (Actor104400Work*)arg0->idMap;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}

void Actor04400_Fn06DFC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    s16              angle;
    s16              speed;

    Actor04400_Fn067A0(arg0, 0x10);
    speed                                           = Actor04400_Fn065F4(arg0, -0x10);
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        Actor104400Work* next = (Actor104400Work*)arg0->idMap;

        next->field_420 = 4;
        next->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06EEC);

/// Counts `field_412` up, clearing the death flag `field_438` on the way. On
/// frame 1 it plays the enemy's hit sound at the model's pan and depth, with
/// the id's high half taken from `GpEnemy::field_8`. Then, when
/// `Actor04400_Fn06618` accepts the frame, draws `field_44A` as 0x5A..0xD9 from
/// `Gp_LcgState` and enters state 3.
void Actor04400_Fn06F50(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              soundId;
    s32              pan;
    u32              rand;

    work            = (Actor104400Work*)arg0->idMap;
    work->field_438 = 0;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        rand             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = rand;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

/// Same body as `ActorsShared8016a184`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn07050(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;
    s32              soundId;
    s32              pan;

    work            = (Actor104400Work*)arg0->idMap;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 9;
    work->field_414 = 1;
    work->field_412 = 0;
    work->field_422++;
    if (enemy->field_40 > 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0002;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
}

void Actor04400_Fn0710C(Task* arg0)
{
    u16              ticks;
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

void Actor04400_Fn0714C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->idMap;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        Actor04400_Fn06374(arg0, 0);
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

void Actor04400_Fn071C8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->idMap;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Frames 0x1D..0x29 of the state: each frame `Actor04400_Fn065F4` gives the
/// step speed, which pushes the model's root coordinate along `field_7A` +
/// 0x400 with `rsin`/`rcos` and clears the root flag. On the frame
/// `Actor04400_Fn06618` accepts, clears the death flag `field_438` and enters
/// state 3.
void Actor04400_Fn0723C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s16              angle;
    s16              speed;
    u16              ticks;

    work            = (Actor104400Work*)arg0->idMap;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((u32)((ticks - 0x1D) & 0xFFFF) < 0xDU) {
        speed                                           = Actor04400_Fn065F4(arg0, 0x1E);
        angle                                           = work->field_7A + 0x400;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 0;
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn07360);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn073C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn07404);

void Actor04400_Fn07530(Task* arg0)
{
    GpEnemy*         enemy;
    Actor104400Work* work;
    TmdObject*       model;
    Actor104400Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2;
    model = (TmdObject*)arg0->extra;
    work  = (Actor104400Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    Actor04400_Fn06374(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->field_C   = model->field_C | 0x80;
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}

/// Same body as `func_actor_342400_8016A724`.
void Actor04400_Fn075F0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    Actor104400Work* work4;
    s16              anim;
    s16              next;

    work = (Actor104400Work*)arg0->idMap;
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work2            = (Actor104400Work*)arg0->idMap;
            work2->field_426 = 4;
            work2->field_41C = 0x10;
            work2->field_418 = 5;
            work2->field_414 = 1;
        } else {
            work3            = (Actor104400Work*)arg0->idMap;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 6;
            work3->field_414 = 1;
        }
    } else {
        next             = Actor04400_D10828[anim - 1];
        work4            = (Actor104400Work*)arg0->idMap;
        work4->field_426 = 4;
        work4->field_41C = 0x10;
        work4->field_418 = next;
        work4->field_414 = 1;
    }
    Actor04400_Fn02B8C(arg0);
    work->field_420++;
}

void Actor04400_Fn076D0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    Actor04400_Fn02B8C();
    work2 = (Actor104400Work*)arg0->idMap;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn07750(Task* arg0)
{
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    Actor104400Work* objWork;

    enemy->field_54 = 0;

    objWork = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// Waits 0x18 frames on `field_412`, then hides the model by setting bit 1 of
/// `TmdObject.field_C` and returns the actor to the state that follows this one.
void Actor04400_Fn0781C(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->field_C |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn07878(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

void Actor04400_Fn07890(Task* arg0)
{
    Actor104400Work* work;
    u16              ticks;

    work            = (Actor104400Work*)arg0->idMap;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn078D4(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    Actor04400_Fn006A8(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    work            = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor104400Work*)arg0->idMap;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

void Actor04400_Fn07968(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn07984(Task* arg0)
{
    Actor104400Work* work;
    u16              ticks;

    work            = (Actor104400Work*)arg0->idMap;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((Game_Session->field_7 == 4) && ((u32)(Game_Session->field_6 - 0x27) < 2U) && (Game_Session->field_9 == 1)) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

/// Same body as `Actor04400_Fn07B4C` up to the two animation requests it
/// writes: this one asks for animation 0xB at speed 8 when the value overlapped
/// onto `field_44F` is 1, and animation 0x11 otherwise. Plays the same
/// encounter sound at the enemy's pan and depth and counts the sub-state up.
void Actor04400_Fn07A38(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u8               kind;

    work            = (Actor104400Work*)arg0->idMap;
    kind            = Actor04400_D10814[work->field_418 - 1];
    work->field_44F = kind;
    if (kind == 1) {
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work3            = (Actor104400Work*)arg0->idMap;
        work3->field_426 = 8;
        work3->field_41C = 0x10;
        work3->field_418 = 0x11;
        work3->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_422 = (u16)(work->field_422 + 1);
}

/// Applies the animation `field_418` asks for: overlaps the values at
/// `Actor04400_D10814[field_418 - 1]` onto `field_44F`, then requests either
/// animation 0xC (kind 1, with sound 0x402C0002) or animation 0x11 for the
/// enemy encountered, and plays the encounter sound at its pan and depth.
/// Counts the sub-state up.
void Actor04400_Fn07B4C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u8               kind;

    work            = (Actor104400Work*)arg0->idMap;
    kind            = Actor04400_D10814[work->field_418 - 1];
    work->field_44F = kind;
    if (kind == 1) {
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work3            = (Actor104400Work*)arg0->idMap;
        work3->field_426 = 8;
        work3->field_41C = 0x10;
        work3->field_418 = 0x11;
        work3->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_422 = (u16)(work->field_422 + 1);
}

void Actor04400_Fn07C60(Task* arg0)
{
    Actor104400Work* work;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor104400Work*)arg0->idMap;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            Actor04400_Fn06374(arg0, 1);
            work            = (Actor104400Work*)arg0->idMap;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}

void Actor04400_Fn07CF0(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->idMap;
    sp   = Actor04400_D00150;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void Actor04400_Fn07D78(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->idMap;
    sp   = Actor04400_D0015C;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void Actor04400_Fn07E00(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        Actor04400_Fn08208,
        Actor04400_Fn0823C,
    };

    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        states[(s16)work->field_422](arg0);
    }
}

extern TaskFuncTable3 Actor04400_D00168;

void Actor04400_Fn07E74(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->idMap;
    sp   = Actor04400_D00168;
    if ((Actor04400_Fn06328(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn07F04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn07F6C);

/// Same body as `ActorsShared8016b104`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn07FD0(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;
    s16              dist;

    if (work->field_446 < (s16)work->field_412++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            Actor104400Work* w = (Actor104400Work*)arg0->idMap;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            Actor104400Work* w = (Actor104400Work*)arg0->idMap;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        Actor104400Work* next = (Actor104400Work*)arg0->idMap;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}

void Actor04400_Fn08094(Task* arg0)
{
    Actor104400Work* work;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor104400Work*)arg0->idMap;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}

void Actor04400_Fn080E8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 1;
        work->field_422++;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn08160);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn08208);

void Actor04400_Fn0823C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_420 = 0;
        work2->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn08290);

void Actor04400_Fn082E0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_412  = 0;
        work->field_438  = 1;
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422++;
    }
}

void Actor04400_Fn08358(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->idMap;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        work2            = (Actor104400Work*)arg0->idMap;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Same body as `ActorsShared8016b500`, which tests the same `flags_EC` pair
/// through `ActorsShared8016945c`; this overlay's copy calls `Actor04400_Fn06328`.
/// Re-requests animation 4 whenever the enemy still carries status flag 1 or
/// 0x102, and counts `field_422` up.
void Actor04400_Fn083CC(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        work2 = (Actor104400Work*)arg0->idMap;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (Actor104400Work*)arg0->idMap;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn0847C);

/// Same body as `func_actor_342400_8016B744`, up to the animation ids it
/// requests. When `Actor04400_D10814[field_418 - 1]` is 0, requests animation
/// 9 with `field_426` 4 and plays the encounter sound 0x402C0002 at the
/// enemy's pan and depth, then sets the sub-state to 4. Otherwise requests
/// animation 7 with `field_426` 8, folds `field_41C * 4` onto `field_44F` and
/// counts the sub-state up.
void Actor04400_Fn08610(Task* arg0)
{
    Actor104400Work* work;
    s32              soundId;
    s32              pan;

    work = (Actor104400Work*)arg0->idMap;
    if (Actor04400_D10814[work->field_418 - 1] == 0) {
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 9;
        work->field_414 = 1;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0002;
        pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_422 = 4;
        return;
    }
    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 7;
    work->field_414 = 1;
    work->field_44F = (u8)work->field_41C * 4;
    work->field_422++;
}

void Actor04400_Fn08718(Task* arg0)
{
    Actor104400Work* work2;
    Actor104400Work* work;
    GpEnemy*         enemy;
    TmdObject*       model;

    work            = (Actor104400Work*)arg0->idMap;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    model           = (TmdObject*)arg0->extra;
    work->field_412 = 0;
    SndEvt_EnqueueType7(0x402C0002, 1);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    enemy->field_54 = 0;
    work2           = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    model->field_C  = model->field_C | 0x80;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn087E0(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks == 3) {
        Tmd_FreeBuffers(model);
        model->field_C |= 4;
    }
    if ((s16)work->field_412 >= 0x24) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

/// Same body as `func_actor_341700_8016A6C0`.
void Actor04400_Fn08870(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor104400Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

/// Same body as `func_actor_342400_8016BA3C`.
void Actor04400_Fn08908(Task* arg0)
{
    Actor104400Work* work;
    s16              anim;
    s16              next;

    work = (Actor104400Work*)arg0->idMap;
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 5;
            work->field_414 = 1;
        } else {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 6;
            work->field_414 = 1;
        }
    } else {
        next            = Actor04400_D10828[anim - 1];
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = next;
        work->field_414 = 1;
    }
    Actor04400_Fn02B8C();
    work->field_420++;
}

void Actor04400_Fn089C0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->idMap;
    Actor04400_Fn02B8C();
    work2 = (Actor104400Work*)arg0->idMap;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn08A40(Task* arg0)
{
    Actor104400Work* work2;
    Actor104400Work* work;

    work                                  = (Actor104400Work*)arg0->idMap;
    ((GpEnemy*)arg0->spawnArg2)->field_54 = 0;
    work2                                 = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn08A9C(void)
{
}

/// Same body as `Actor04400_Fn08870`.
void Actor04400_Fn08AA4(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor104400Work*)arg0->idMap;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

/// Same body as `ActorsShared8016a98c`, which serves `actor_341700` and
/// `actor_342400`; this unit serves `actor_104400` and `actor_342200`.
///
/// `work` is declared before `coord` on purpose: sched1 promotes all four
/// loads to `LAUNCH_PRIORITY` and breaks that tie by descending `INSN_LUID`,
/// so the order the initialisers are emitted in is the order the loads land.
void Actor04400_Fn08B3C(Task* arg0)
{
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    Actor104400Work* objWork;

    enemy->field_54 = 0;

    objWork = (Actor104400Work*)arg0->idMap;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// Waits 0x18 frames on `field_412`, then hides the model by setting bit 1 of
/// `TmdObject.field_C`. Body is identical to `Actor04400_Fn0781C`'s.
void Actor04400_Fn08C08(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->idMap;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->field_C |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn08C64);

void Actor04400_Fn08DA4(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

s32 Actor04400_Fn08DBC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->idMap;

    if (work->field_41E == 1) {
        switch (work->field_448) {
            case 3:
                work->field_420 = 8;
                work->field_422 = 0;
                break;
            case 5:
                work->field_420 = 9;
                work->field_422 = 0;
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
