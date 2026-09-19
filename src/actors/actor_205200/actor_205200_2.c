#include "common.h"

#include "actors/actor_205200.h"
#include "actors/actors_shared_80134ff0.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/mem.h"
#include "gameplay/gameplay.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_205200_80149E24;
extern GpEnemyTaskFuncTable3 D_actor_205200_80149E30;
/// Animation block the attack body hands the player with message 0x3F4.
extern void* D_actor_205200_80156800;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
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
extern s16 D_80073BA0;
extern u32 Gp_LcgState;

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
    parentWork = (Actor205200Work*)arg1->field_8->work;
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
    arg0->node.flags = 8;
    goto default_body;
case2:
    arg0->node.flags = one;
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

extern u32     D_actor_205200_801567E8;
extern s16     D_actor_205200_801567B0[];
extern SVECTOR D_actor_205200_801567B4[];
extern u32     D_actor_205200_801567D0;

/// Spawn handler: allocates the work block, binds the model's matrices to it,
/// starts animation slots 1..18 and links the two render objects, whose
/// second one takes its offset and range from the spawn place's `field_2`.
void func_actor_205200_8014BAE8(GpEnemy* enemy, Task* task)
{
    TmdObject*       tmd;
    GsCOORDINATE2*   coords;
    Actor205200Work* work;
    s32              i;

    tmd    = task->extra;
    coords = tmd->coords;
    work   = memCalloc(sizeof(Actor205200Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    tmd->flags      = 0;
    coords->flg     = 0;
    tmd->lightMtx   = &work->field_45C;
    tmd->colorMtx   = &work->field_43C;
    enemy->field_4  = &coords->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->field_18    = &((TmdObject*)task->extra)->coords[3];
    enemy->node.flags  = 5;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_54    = (s32)work->field_49C;
    enemy->field_50    = NULL;
    enemy->field_40    = 0;
    work->field_554    = &((TmdObject*)task->extra)->coords[3];
    work->field_558    = 0x200;
    work->field_55A    = 1;
    func_800B3F84((GpAnimCtx*)work, &D_actor_205200_801567E8, tmd, work->field_30C,
                  (GpAnimSlot*)&work->pad_14);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i++;
    } while (i < 0x13);
    work->field_596          = ((GpAreaPlace*)enemy->field_3C)->field_2;
    work->field_47C.pos.vy   = -300;
    work->field_47C.coord    = coords;
    work->field_47C.ctx.recs = work->field_49C;
    work->field_47C.pos.vx   = 0;
    work->field_47C.pos.vz   = 0;
    work->field_47C.key      = 0x3003C;
    work->field_47C.radius   = 300;
    work->field_47C.flags    = 1;
    Gp_LinkObj(2, &work->field_47C);
    Gp_InitRec18Table(work->field_49C, 3, 0);
    work->field_47C.flags   |= 0x8000;
    work->field_4E4.coord    = coords;
    work->field_4E4.ctx.recs = &work->field_504;
    work->field_4E4.pos.vx   = D_actor_205200_801567B4[work->field_596].vx;
    work->field_4E4.pos.vy   = D_actor_205200_801567B4[work->field_596].vy;
    work->field_4E4.pos.vz   = D_actor_205200_801567B4[work->field_596].vz;
    work->field_4E4.key      = 0;
    work->field_4E4.radius   = D_actor_205200_801567B0[work->field_596];
    work->field_4E4.flags    = 1;
    Gp_LinkObj(2, &work->field_4E4);
    Gp_InitRec18Table(&work->field_504, 1, 0);
    work->field_4E4.flags |= 0x8000;
    task->msgTable         = &D_actor_205200_801567D0;
    task->state            = 1;
}

void func_actor_205200_8014BD4C(Actor205200* arg0)
{
    Actor205200Work* work;
    s32              i;
    s32              found;
    s32              last;
    s32              n;

    found                  = 0;
    work                   = arg0->field_1C;
    last                   = 0;
    *(u32*)G_SCRATCH_HEAD -= 0x10;
    if (work->field_57C != 0) {
        if (--work->field_57C <= 0) {
            work->field_57C = 0;
        }
        if (work->field_57C != 0) {
            goto end;
        }
    }
    for (i = 0; i < 3; i++) {
        if ((work->field_49C[i].key & 0xFFFF0000) == 0x20000) {
            func_800DA6E8(&arg0->field_20->node, 0, 0);
            switch (Gp_GetIdParam0(work->field_49C[i].key) & 0xFFFF) {
                case 1:
                    found = 1;
                    break;
                case 2:
                    break;
            }
            if (found == 0) {
                break;
            }
            work->field_584 = 1;
            work->field_586 = 0;
            if (last != work->field_49C[i].key) {
                last = work->field_49C[i].key;
                func_800FDB18(Gp_GetIdParam1(last) & 0xFFFF, &arg0->field_2C->field_8[3], NULL,
                              (GpEffArg*)&work->field_554);
            }
            if ((n = Gp_GetIdParam2(work->field_49C[i].key)) > 0) {
                work->field_57C = n;
            }
        }
    }
end:
    Gp_ClearRec18Occupied(work->field_49C);
    if (work->field_504.flags & 1) {
        if ((work->field_504.key & 0xFFFF0000) == 0x10000 && D_80073BA0 > 0) {
            work->field_588      = 1;
            Gp_StateC08.field_6 |= 1;
        }
        Gp_ClearRec18Occupied(&work->field_504);
    }
    *(u32*)G_SCRATCH_HEAD += 0x10;
}

/// Charge-handler sub-state machine. States 0 and 3 share a random roll: every
/// 15-46 frames a 1-in-8 draw switches to state 4 with animation 5. State 2
/// waits out the animation, raising bit 2 of `Gp_StateF0.field_1D` on frame 60,
/// and state 4 returns to 3 while the `field_590` cooldown is still running.
void func_actor_205200_8014BF28(Actor205200* arg0)
{
    Actor205200Work* work;
    s16              next;

    work = arg0->field_1C;
    switch (work->field_586) {
        case 0:
            if (Gp_StateF0.field_1D & 2) {
                Gp_StateF0.field_1D &= 0xFD;
                work->field_586      = 1;
            }
        tick:
            if (--work->field_592 <= 0) {
                work->field_592 = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1F) + 0xF;
                if (!(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 7)) {
                    work->field_586 = 4;
                    work->field_57E = 5;
                }
            }
            break;
        case 1:
            work->field_57E = 2;
            work->field_586 = 2;
            break;
        case 2:
            if ((s16)work->field_582 == 0x3C) {
                Gp_StateF0.field_1D |= 4;
            }
            if ((s16)work->field_582 >= 0x54) {
                work->field_57E = 1;
                work->field_586 = 3;
                work->field_590 = 0x258;
            }
            break;
        case 3:
            if (--work->field_590 <= 0) {
                work->field_586 = 0;
            }
            goto tick;
        case 4:
            if ((s16)work->field_582 >= 0x40) {
                next = 0;
                if (work->field_590 > 0) {
                    next = 3;
                }
                work->field_586 = next;
                work->field_57E = 1;
            }
            break;
    }
}

/// The attack body, run while `field_588` is set. It carves an
/// `Actor205200AttackScratch` from `G_SCRATCH_HEAD` and steps `field_58A`:
/// state 0 records which side of the player it is on (`field_58E`), plays its grab
/// animation and spawns the effect; state 1 drags the player towards the actor
/// for 0x10 frames and hands over after 0x1E/0x20; state 2 waits for the
/// animation to finish and clears `field_588`. The duplicated calls in the
/// `field_596` arms are what the target's shared tails need: jump2's
/// cross-jumping merges them, where a variable or ternary is hoisted instead.
void func_actor_205200_8014C0C0(Actor205200* arg0)
{
    Actor205200Work*          work;
    GsCOORDINATE2*            coord;
    Task*                     player;
    GsCOORDINATE2*            target;
    Actor205200AttackScratch* scratch;
    void*                     head;
    s32                       sound;
    s32                       count;

    work                    = arg0->field_1C;
    player                  = Game_GetPtrSlot(3);
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - sizeof(Actor205200AttackScratch);
    scratch                 = *(Actor205200AttackScratch**)G_SCRATCH_HEAD;
    coord                   = arg0->field_2C->field_8;
    target                  = ((TmdObject*)player->extra)->coords;

    switch (work->field_58A) {
        case 0:
            if (((GameActor*)player->work)->field_954 != 2) {
                scratch->delta.vx      = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy      = 0;
                scratch->delta.vz      = target->coord.t[2] - coord->coord.t[2];
                work->field_58E        = (scratch->delta.vx * target->coord.m[0][2] + scratch->delta.vz * target->coord.m[2][2]) > 0;
                scratch->anim.field_0  = &D_actor_205200_80156800;
                scratch->anim.field_4  = work->field_58E + 1;
                scratch->anim.field_8  = 0;
                scratch->anim.field_C  = 0;
                scratch->anim.field_10 = 1;
                Gp_DispatchMsg(player, 0x3F4, (s32)scratch, 0);
                work->field_58A = 1;
                work->field_58C = 0;
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
                sound = ((arg0->field_20->field_8 >> 12) << 8) | 7;
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                scratch->dir.vx = 0;
                scratch->dir.vy = -1000;
                scratch->dir.vz = 0;
                if (work->field_596 == 0) {
                    Gp_SpawnEff(0x60299, ((TmdObject*)player->extra)->coords, 0, &scratch->dir);
                } else {
                    Gp_SpawnEff(0x601AC, ((TmdObject*)player->extra)->coords, 0, &scratch->dir);
                }
            } else {
                work->field_588 = 0;
            }
            break;
        case 1:
            if ((s16)work->field_58C < 0x10) {
                scratch->delta.vx = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy = target->coord.t[1] - coord->coord.t[1];
                scratch->delta.vz = target->coord.t[2] - coord->coord.t[2];
                VectorNormalS(&scratch->delta, &scratch->dir);
                scratch->pos.vx = target->coord.t[0] + ((scratch->dir.vx * 25) >> 10);
                scratch->pos.vy = 0;
                scratch->pos.vz = target->coord.t[2] + ((scratch->dir.vz * 25) >> 10);
                scratch->rot.vx = 0;
                if (work->field_58E == 0) {
                    scratch->rot.vy = (ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) + 0x800) & 0xFFF;
                } else {
                    scratch->rot.vy = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & 0xFFF;
                }
                scratch->rot.vz = 0;
                Gp_DispatchMsg(player, 0x3E9, (s32)&scratch->pos, 0);
            }
            if ((s16)work->field_58C == 0x10) {
                if (work->field_596 == 0) {
                    sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x55180002;
                    SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                } else {
                    sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x55190003;
                    SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
            }
            count = (s16)++work->field_58C;
            if ((work->field_58E != 0 && count >= 0x1E) || (work->field_58E == 0 && count >= 0x20)) {
                scratch->anim.field_0  = &D_actor_205200_80156800;
                scratch->anim.field_4  = work->field_58E + 3;
                scratch->anim.field_8  = 0;
                scratch->anim.field_C  = 0;
                scratch->anim.field_10 = 1;
                Gp_DispatchMsg(player, 0x3F4, (s32)scratch, 0);
                work->field_58A = 2;
                work->field_58C = 0;
            }
            break;
        case 2:
            if ((s16)++work->field_58C >= 0x25) {
                if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                    Gp_DispatchMsg(player, 0x3F1, 0, 0);
                    work->field_58A = 0;
                    work->field_58C = 0;
                    work->field_588 = 0;
                }
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(Actor205200AttackScratch);
}

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
    if (gGameSession->eventState != 0) {
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

/// Charge handler, sub-state 1 of `func_actor_205200_8014C67C`. On entry it
/// switches the animation to id 3 and raises bit 3 of `Gp_StateF0.field_1D`;
/// once the frame counter reaches 35 it plays id 1, parks the charge sub-state
/// at 3, drops back to the idle handler and loads 600 into `field_590`.
void func_actor_205200_8014C748(Actor205200* arg0)
{
    Actor205200Work* work;
    s16              state;

    work  = arg0->field_1C;
    state = work->field_586;
    switch (state) {
        case 0:
            work->field_57E      = 3;
            work->field_586      = 1;
            Gp_StateF0.field_1D |= 8;
            return;
        case 1:
            if ((s16)work->field_582 >= 0x23) {
                work->field_586 = 3;
                work->field_57E = 1;
                work->field_584 = 0;
                work->field_590 = 0x258;
            }
            return;
    }
}

/// Keeps the work's animation id bound to its helper slots. When the id has
/// changed since the last tick the remembered id follows it, the frame counter
/// at 0x582 restarts and every slot 1..18 is pointed at the new id at weight 8;
/// otherwise the counter ticks and the slots are simply advanced. The same body
/// is matched once for the actors that share it as `ActorsShared8014af2c`,
/// which differs only in the slot count and the id offsets.
void func_actor_205200_8014C7CC(Actor205200* arg0)
{
    Actor205200Work* work;
    s32              i;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_57E != (s16)work->field_580) {
        work->field_580 = work->field_57E;
        work->field_582 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_57E, 0, 8);
            i++;
        } while (i < 0x13);
        return;
    }
    TOUCH_REG(i);
    work->field_582 = (u16)(work->field_582 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 0x13);
}
