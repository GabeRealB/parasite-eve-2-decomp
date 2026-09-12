#include "common.h"

#include "actors/actor_102000.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_ArmStateF0(s32 arg0);
void Actor02000_Fn00CD0(Actor02000* arg0);
s32  Gp_TickObjFlag2(void* arg0);
s32  Gp_GetObjPan(void* arg0);
s32  Gp_GetObjDepth(void* arg0);
s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s32 Gp_LcgState;
extern s8  D_80115419;
extern s16 Actor02000_D03784[];
extern s32 Actor02000_D15DEC[];

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn00078);

/// Per-frame tick for the actor's approach cycle, sharing the `field_6A8`
/// state with `Actor02000_Fn03268`. State 0 drains the `field_6DA` budget by
/// `field_69C` (0 while `field_698` is still under the per-animation entry of
/// `Actor02000_D03784`, 0x14 once it is past it) and runs
/// `Actor02000_Fn00CD0` every frame; when the budget runs out it switches to
/// animation 4 and state 1. State 1 waits for `field_698` to reach 0x60, then
/// either falls back to animation 2 (budget left) or starts the lunge:
/// animation 3, state 2, a fresh budget of 1000 per unit of the spawn record's
/// byte 1, and `field_6A2` / `field_6A4` set to the actor's current yaw and its
/// opposite. State 2 holds `field_69E` at 0x3B until `field_698` reaches 0x23,
/// then returns to animation 2 and state 0. As in `Actor02000_Fn03268`, a set
/// `field_6B2` or `D_80115419` overrides everything with animation 2 and the
/// shared state-F0 slot.
void Actor02000_Fn00AEC(Actor02000* arg0)
{
    Actor02000Work* work;
    Actor02000Ctx*  spawn;
    GsCOORDINATE2*  self;
    u8*             head;
    s16             state;
    s16             delta;
    s32             ang;
    s32             param;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;

    self  = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    spawn = arg0->field_20;
    state = work->field_6A8;

    switch (state) {
        case 0:
            delta = 0;
            if (work->field_698 >= Actor02000_D03784[work->field_694]) {
                delta = 0x14;
            }
            work->field_69C  = delta;
            work->field_69E  = 0;
            work->field_6DA -= work->field_69C;
            if (work->field_6DA <= 0) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
                work->field_69C = 0;
            }
            Actor02000_Fn00CD0(arg0);
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x60) {
                if (work->field_6DA <= 0) {
                    param           = spawn->field_3C->field_1;
                    work->field_694 = 3;
                    work->field_6A8 = 2;
                    work->field_6DA = param * 1000;
                    ang             = ratan2(self->coord.m[0][2], self->coord.m[2][2]) & 0xFFF;
                    work->field_6A2 = ang;
                    work->field_6A4 = (ang + 0x800) & 0xFFF;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x10;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn00CD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn00E0C);

/// Per-frame tick. State 0 picks the animation from `field_6B8`: 1 selects
/// animation 0x17 and hands over to state 1, anything else selects 0x1B and
/// hands over to state 2. State 1 waits for `field_698` to reach 0x10 and
/// state 2 waits for it to reach 0x16; both then play the matching idle
/// (0x19 / 0x1D), park `field_6A6` at 0xB, move to state 3 and roll a fresh
/// 6-bit dwell into `field_6AE`.
void Actor02000_Fn011E8(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;
    s32             next;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6B8;
            if (next == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x1B;
                work->field_6A8 = 2;
            }
            break;
        case 1:
            if (work->field_698 >= 0x10) {
                work->field_694 = 0x19;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                work->field_694 = 0x1D;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
    }
}

void Actor02000_Fn012E0(Actor02000* arg0)
{
    Actor02000Work* work;
    GsCOORDINATE2*  self;
    s32             snd;
    s16             state;

    work  = arg0->field_1C;
    self  = arg0->field_2C->field_8;
    state = work->field_6A8;

    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694          = 0x16;
                work->field_6A8          = 1;
                work->field_6B8          = 1;
                work->field_6AE          = 0x42;
                work->field_4CC.field_14 = -0xA7;
            } else {
                work->field_694          = 0x1A;
                work->field_6A8          = 1;
                work->field_6B8          = 2;
                work->field_6AE          = 0x31;
                work->field_4CC.field_14 = 0x109;
            }
            work->field_4CC.field_1C = 0x15E;
            work->field_69C          = 0;
            work->field_69E          = 0;
            work->field_6DE          = 1;
            work->field_4CC.flags   |= 0x4000;
            work->field_564.flags   &= 0xBFFF;
            arg0->field_20->field_4C = 0;
            work->field_6D4          = 1;
            break;
        case 1:
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            if (work->field_6B8 == 1) {
                if (work->field_698 == 0x14) {
                    s32 pan;

                    snd = Actor02000_D15DEC[work->field_6D6 + 0xC] | ((arg0->field_20->field_8 >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = Actor02000_D15DEC[work->field_6D6 + 8] | ((arg0->field_20->field_8 >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = Actor02000_D15DEC[work->field_6D6 + 8] | ((arg0->field_20->field_8 >> 12) << 8);
                pan = (s8)Gp_GetObjPan(self);

                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
            }
            work->field_6AE--;
            if (work->field_6AE <= 0) {
                arg0->field_30  = 2;
                work->field_6A8 = 0;
                work->field_6D4 = 0;
            }
            break;
    }
}

void Actor02000_Fn0150C(Actor02000* arg0)
{
    Actor02000Work* work;
    GsCOORDINATE2*  coord;
    SVECTOR*        rot;
    s32             ang;
    u16             want;
    s16             diff;
    s32             adiff;
    s32             step;
    s32             ustep;
    s32             wstep;
    s32             cur;
    s32             next;
    s32             wrapStep;

    rot   = (SVECTOR*)(SCRATCH_SP -= 8);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_6A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_6A2 = ang;
    if (adiff < 0x800) {
        step  = work->field_69E;
        ustep = (u16)work->field_69E;
        if (step >= adiff) {
            work->field_6A2 = want;
        } else {
            if (work->field_694 == 3) {
                next = ang - ustep;
            } else {
                next = work->field_6A2;
                if (diff <= 0) {
                    next -= step;
                } else {
                    next += step;
                }
            }
            work->field_6A2 = next;
        }
    } else {
        wstep = work->field_69E;
        if (diff > 0) {
            if (wstep >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (wstep >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_6A2 = work->field_6A4;
        goto done;
    turn:
        if (work->field_694 == 3) {
            work->field_6A2 = (u16)work->field_6A2 - (u16)work->field_69E;
        } else {
            wrapStep = work->field_69E;
            cur      = work->field_6A2;
            if (diff > 0) {
                work->field_6A2 = cur - wrapStep;
            } else {
                work->field_6A2 = cur + wrapStep;
            }
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_6A2;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_SP += 8;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn01698);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn018A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn01A20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn01DF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn02294);

void                 Gp_DestroyEnemy(Actor02000Ctx* ctx, Actor02000* actor);
void                 Gp_LinkNode(Actor02000Node* node);
void                 func_800B3F84(Actor02000Work* arg0, void* arg1, TmdObject* arg2, void* arg3,
                                   Actor02000AnimSlots* arg4);
void                 Gp_AnimResetSlot(Actor02000Work* arg0, s32 arg1, s32 arg2);
void                 Gp_IncStateF0Ref(s32 arg0);
void                 Gp_LinkObj(s32 arg0, Actor02000Obj* arg1);
void                 Gp_InitRec18Table(Actor02000Rec18* arg0, s32 arg1, s32 arg2);
Actor02000Eff*       Gp_SpawnEnemyFromTable(void* table, s32 idx, s32 arg2, void* parent);
void                 Gp_SyncAreaKeyIndex(Actor02000AreaKey* arg0);
Actor02000AreaTable* Gp_GetNestedAreaRec(Actor02000AreaKey* arg0);

extern void           Actor02000_D15FE8;
extern s16            Actor02000_D15FD0[];
extern u16*           Actor02000_D15FB8[];
extern Actor02000Desc Actor02000_D15D10[];

/// Enemy init. Allocates the 0x6E4-byte work block, points the model object at
/// the light / color matrices inside it, runs the animation context over its
/// nineteen slots, and spawns the companion enemy from `Actor02000_D15FD0`,
/// copying that model's texture page and CLUT row out of the current area
/// record. `Actor02000Ctx.field_4B` then selects the variant: 0 builds the
/// full object set (list node, the four `Gp_LinkObj` nodes and their
/// `Actor02000Rec18` tables, and the optional CD prefetch of `field_6D6`),
/// while 1 and 2 only prime the animation state and hand the task to state 2.
void Actor02000_Fn0251C(Actor02000Ctx* ctx, Actor02000* actor)
{
    Actor02000Work*      work;
    TmdObject*           obj;
    TmdObject*           model;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       parts;
    GsCOORDINATE2*       partsA;
    GsCOORDINATE2*       partsB;
    GsCOORDINATE2*       partsC;
    GsCOORDINATE2*       effParts;
    Actor02000AreaKey*   sessionKey;
    Actor02000AreaKey*   keyPtr;
    u8                   areaByte0;
    Actor02000AreaTable* rec;
    Actor02000AreaRec*   entry;
    Actor02000Eff*       eff;
    u16*                 tbl;
    u8                   param1[8];
    u8                   param2[8];
    Actor02000AreaKey    key;
    s32                  i;
    s32                  one;
    s32                  kind;
    s32                  idx;
    s32                  param;

    obj   = actor->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->field_1C = work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = &work->field_45C;
    obj->field_20   = &work->field_43C;
    work->field_6CA = 0x14;
    work->field_66C = Actor02000_D15FD0;
    work->field_670 = &actor->field_2C->field_8[3];
    work->field_674 = 0x500;
    work->field_676 = 2;
    func_800B3F84(work, &Actor02000_D15FE8, obj, work->field_30C, &work->field_14);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(work, i, 1);
    }
    eff         = Gp_SpawnEnemyFromTable(Actor02000_D15FD0, 1, 0, ctx);
    sessionKey  = (Actor02000AreaKey*)&Game_Session->field_4;
    model       = eff->task->field_2C;
    idx         = ctx->field_8 >> 12;
    key.field_3 = sessionKey->field_3;
    key.field_2 = sessionKey->field_2;
    key.field_1 = sessionKey->field_1;
    areaByte0   = sessionKey->field_0;
    /* Both calls take `&key`. Left alone, GCC 2.8.1 CSEs that address into one
       pseudo that is live across the first call, costing a callee-saved
       register; the ROM rematerializes `addiu a0, sp, key` for each call. The
       barrier keeps the address materialization next to the call and the
       `+r` touch makes the second one a fresh computation. */
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry           = (Actor02000AreaRec*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = entry->field_D;
    model->field_25 = entry->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }

    one  = 1;
    kind = ctx->field_4B;
    if (kind == one) {
        goto case1;
    }
    if (kind >= 2) {
        goto ge2;
    }
    if (kind == 0) {
        goto case0;
    }
    return;
ge2:
    if (kind == 2) {
        goto case2;
    }
    return;

case0:
    ctx->field_4  = &coord->coord;
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    parts         = actor->field_2C->field_8;
    ctx->field_1C = 0;
    ctx->field_20 = 0;
    ctx->field_24 = 0;
    ctx->field_50 = Actor02000_D15D10;
    ctx->field_54 = work->field_4EC;
    ctx->field_18 = &parts[3];
    ctx->field_40 = Actor02000_D15D10->field_4;
    Gp_IncStateF0Ref(0);
    work->field_6AC = ctx->field_3C->field_2 & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = ctx->field_3C->field_1;
        work->field_6DA = param * 1000;
    }

    tbl = Actor02000_D15FB8[Game_Session->field_7];
    if (tbl != NULL) {
        work->field_6D6 = tbl[Game_Session->field_6];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x14;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }

    work->field_49C.field_4  = 0x1F40;
    work->field_49C.field_10 = 0x3E8;
    work->field_49C.field_0  = 0;
    work->field_49C.field_2  = 0;
    work->field_49C.field_8  = 0;
    work->field_49C.field_A  = 0;
    work->field_49C.field_C  = 0;
    work->field_49C.field_12 = 0x5DC;
    work->field_49C.field_14 = work->field_4B4;
    partsA                   = actor->field_2C->field_8;
    work->field_47C.field_C  = &work->field_49C;
    work->field_47C.field_10 = 0;
    work->field_47C.field_12 = 0;
    work->field_47C.field_14 = 0;
    work->field_47C.field_18 = 0;
    work->field_47C.field_1C = 0;
    work->field_47C.flags    = 3;
    work->field_47C.field_8  = &partsA[4];
    Gp_LinkObj(3, &work->field_47C);
    Gp_InitRec18Table(work->field_4B4, 1, 0);
    work->field_47C.flags |= 0xCC00;

    partsB                   = actor->field_2C->field_8;
    work->field_4CC.field_C  = work->field_4EC;
    work->field_4CC.field_10 = 0;
    work->field_4CC.field_12 = 0;
    work->field_4CC.field_14 = 0;
    work->field_4CC.field_18 = 0x30014;
    work->field_4CC.field_1C = 0x190;
    work->field_4CC.flags    = 1;
    work->field_4CC.field_8  = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= 0x8000;

    partsC                   = actor->field_2C->field_8;
    work->field_564.field_12 = -0x226;
    work->field_564.field_C  = work->field_584;
    work->field_564.field_10 = 0;
    work->field_564.field_14 = 0;
    work->field_564.field_18 = 0;
    work->field_564.field_1C = 0x226;
    work->field_564.flags    = 1;
    work->field_564.field_8  = partsC;
    Gp_LinkObj(2, &work->field_564);
    Gp_InitRec18Table(work->field_584, 4, 0);
    work->field_564.flags |= 0x4200;

    effParts                 = eff->task->field_2C->field_8;
    work->field_5E4.field_C  = work->field_604;
    work->field_5E4.field_10 = 0;
    work->field_5E4.field_12 = 0x1F4;
    work->field_5E4.field_14 = 0;
    work->field_5E4.field_18 = 0;
    work->field_5E4.field_1C = 0x1F4;
    work->field_5E4.flags    = 1;
    work->field_5E4.field_8  = effParts;
    Gp_LinkObj(3, &work->field_5E4);
    Gp_InitRec18Table(work->field_604, 1, 0);
    work->field_5E4.flags &= 0x7FFF;
    actor->field_30        = 1;
    return;

case1:
    work->field_694 = 0x19;
    work->field_6A8 = 2;
    actor->field_30 = 2;
    return;

case2:
    work->field_694 = 0x1D;
    work->field_6A8 = kind;
    actor->field_30 = kind;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn02A34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn02D5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn0315C);

/// Per-frame tick, entry 0 of `Actor02000_D16064`. State 0 counts `field_6AE`
/// up to 0x5B frames and then hands over to state 1 with animation 4, running
/// `Actor02000_Fn00CD0` every frame meanwhile; state 1 waits for `field_698`
/// to reach 0x5E and drops back to state 0 with animation 1. Either way, once
/// `field_6B2` or the global `D_80115419` is set the actor switches to
/// animation 2 and arms the shared state-F0 slot.
void Actor02000_Fn03268(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_6AE++;
            if (work->field_6AE >= 0x5B) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
            }
            Actor02000_Fn00CD0(arg0);
            break;
        case 1:
            if (work->field_698 >= 0x5E) {
                work->field_694 = 1;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }
}

/// Per-frame tick. State 0 selects animation 0x11, hands over to state 1 and
/// clears the pair of counters at `field_69C`. State 1 waits for `field_698`
/// to reach 0x37, then parks at animation 2 / `field_6A6` 2 when `field_6E0`
/// is clear, or animation 0x14 / `field_6A6` 0xA otherwise, and drops back
/// to state 0.
void Actor02000_Fn03348(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_694 = 0x11;
            work->field_6A8 = 1;
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x37) {
                if (work->field_6E0 == 0) {
                    work->field_694 = 2;
                    work->field_6A6 = 2;
                    work->field_6A8 = 0;
                } else {
                    work->field_694 = 0x14;
                    work->field_6A6 = 0xA;
                    work->field_6A8 = 0;
                }
            }
            break;
    }
}

/// Per-frame tick. State 0 picks the animation from `field_6AA`: 1 selects
/// animation 0x12 and hands over to state 1, anything else selects 0x13 and
/// hands over to state 2; either way the pair of counters at `field_69C` is
/// cleared. State 1 waits for `field_698` to reach 0x50 and state 2 waits for
/// it to reach 0x3B, both dropping back to state 0 with animation 2.
void Actor02000_Fn033D4(Actor02000* arg0)
{
    Actor02000Work* work;
    s32             state;
    s32             next;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6AA;
            if (next == 1) {
                work->field_694 = 0x12;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x13;
                work->field_6A8 = 2;
            }
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x50) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
        case 2:
            if (work->field_698 >= 0x3B) {
                work->field_694 = state;
                work->field_6A6 = state;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Per-frame tick. State 0 waits for `Gp_TickObjFlag2` on the spawn block to
/// fire, then selects animation 0x13, clears `field_6E0` and advances to state
/// 1. State 1 waits for `field_698` to reach 0x3B and drops back to state 0
/// with animation 2.
void Actor02000_Fn0349C(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                work->field_694 = 0x13;
                work->field_6A8 = 1;
                work->field_6E0 = 0;
            }
            break;
        case 1:
            if (work->field_698 >= 0x3B) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Per-frame tick. State 0 picks the animation from `field_6B8`: 1 selects
/// animation 0x17 and hands over to state 1, anything else selects 0x1B and
/// hands over to state 2. State 1 waits for `field_698` to reach 0x10 and
/// state 2 waits for it to reach 0x16; both park the actor by writing its
/// dwell code to `field_30` and drop back to state 0.
void Actor02000_Fn03528(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;
    s32             next;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6B8;
            if (next == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x1B;
                work->field_6A8 = 2;
            }
            break;
        case 1:
            if (work->field_698 >= 0x10) {
                arg0->field_30  = 2;
                work->field_6A8 = 0;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                arg0->field_30  = state;
                work->field_6A8 = 0;
            }
            break;
    }
}

void Actor02000_Fn035E0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn035E8);

/// Parents this actor's model to part 7 of its spawner's model, points the
/// model at the spawner's light and colour matrices and seeds the spawner's
/// dwell counter, then advances the task to state 1. `arg0` is the enemy
/// context every state handler takes and is unused here.
void Actor02000_Fn03644(void* arg0, Task* task)
{
    Task*           parent;
    TmdObject*      obj;
    Actor02000Work* work;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->field_8;
    coord        = obj->field_8;
    work         = (Actor02000Work*)parent->idMap;

    coord->flg      = 0;
    coord->sub      = &parentCoords[7];
    obj->field_1C   = &work->field_45C;
    obj->field_20   = &work->field_43C;
    obj->field_C    = 0;
    task->state     = 1;
    work->field_6D8 = 0xA;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn03690);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn03728);
