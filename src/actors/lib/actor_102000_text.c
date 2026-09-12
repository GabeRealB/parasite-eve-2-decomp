#include "common.h"

#include "actors/actor_102000.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void               Gp_ArmStateF0(s32 arg0);
void               Actor02000_Fn00CD0(Actor02000* arg0);
s32                Gp_TickObjFlag2(void* arg0);
s32                Gp_GetObjPan(void* arg0);
s32                Gp_GetObjDepth(void* arg0);
Actor02000AnimRec* Gp_AnimGetRec(Actor02000Work*, void*);
s32                SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s32 Gp_LcgState;
extern s32 D_8011572C;
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

void Actor02000_Fn018A4(Actor02000* arg0)
{
    s32                snd;
    s32                pan;
    s32                pan2;
    Actor02000Work*    work;
    GsCOORDINATE2*     self;
    Actor02000AnimRec* rec;

    work = arg0->field_1C;
    self = arg0->field_2C->field_8;
    if (work->field_6D6 != 0) {
        rec = Gp_AnimGetRec(work, &work->field_14.slots[1]);
        if (rec != NULL) {
            if (!(rec->field_3 & 0x20) && (work->field_6A0 & 0x20)) {
                snd = Actor02000_D15DEC[work->field_6D6 * 2 - 1] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
            }
            if (!(rec->field_3 & 0x10) && (work->field_6A0 & 0x10)) {
                snd  = Actor02000_D15DEC[work->field_6D6 * 2] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth(self));
            }
            work->field_6A0 = (u16)(rec->field_3 & 0x30);
        }
    }
}

extern u8 D_801153F4;
void      Gp_AnimTickIndex(Actor02000Work*, s32);
void      Gp_DrawEffGroundQuad(VECTOR3*, s32, s32);
void      Gp_ReleaseStateF0Add(Actor02000*, s32);
void      Gp_SaveEnemyPose(Actor02000Ctx*);
void      Gp_UnlinkNode(Actor02000Node*);
void      Gp_UnlinkObj(Actor02000Obj*);
void      Gp_UpdateActorColor(Actor02000Ctx*, VECTOR3*, s32, s32);
void      Gp_UpdateCoord(GsCOORDINATE2*);
void      func_800B4114(Actor02000Work*, s32, s32, s32, s32);

void Actor02000_Fn01A20(Actor02000Ctx* ctx, Actor02000* actor)
{
    VECTOR3         pos;
    SVECTOR*        scratch;
    s16             duration;
    s16             state;
    s16             anim;
    GsCOORDINATE2*  partA;
    GsCOORDINATE2*  partB;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  rootA;
    GsCOORDINATE2*  rootB;
    GsCOORDINATE2*  rootC;
    GsCOORDINATE2*  rootD;
    s32             i;
    u32             random;
    Actor02000Work* work;
    Actor02000Work* animWork;

    *(u32*)0x1F8003FC -= 8;
    scratch            = (SVECTOR*)*(u32*)0x1F8003FC;
    work               = actor->field_1C;
    coord              = ((void*)((volatile Actor02000*)actor)->field_2C->field_8);
    switch (D_801153F4) {
        case 0:
            actor->field_2C->field_C = 0;
            ctx->node.field_4        = 0;
            break;
        case 1:
            coord->flg                      = 0;
            actor->field_2C->field_8[3].flg = 0;
            Gp_UpdateCoord(coord);
            rootC  = actor->field_2C->field_8;
            pos.vx = rootC->workm.t[0];
            pos.vy = rootC->workm.t[1];
            pos.vz = rootC->workm.t[2];
            Gp_UpdateActorColor(actor->field_20, &pos, 0, 0);
            rootD  = actor->field_2C->field_8;
            partB  = &rootD[3];
            pos.vx = partB->workm.t[0];
            pos.vy = rootD->workm.t[1];
            pos.vz = partB->workm.t[2];
            Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
            return;
        case 2:
            actor->field_2C->field_C = 0x80;
            ctx->node.field_4        = 1;
            return;
    }
    state = work->field_6A8;
    switch (state) {
        case 0:
            ctx->field_54 = 0;
            Gp_UnlinkNode(&ctx->node);
            Gp_UnlinkObj(&work->field_47C);
            Gp_UnlinkObj(&work->field_564);
            Gp_UnlinkObj(&work->field_4CC);
            Gp_UnlinkObj(&work->field_5E4);
            if ((u32)((u16)work->field_6CA - 0x38) < 2U) {
                Gp_UnlinkObj(&work->field_61C);
            }
            Gp_ReleaseStateF0Add(actor, (s16)work->field_6CA);
            anim = 0x1D;
            if (work->field_6B8 == 1) {
                anim = 0x19;
            }
            work->field_694 = anim;
            work->field_6A8 = 1;
            ctx->field_4B   = (u8)work->field_6B8;
            Gp_SaveEnemyPose(ctx);
            D_80115419 = 1;
            break;
        case 1:
            if (!(work->field_698 & 3)) {
                scratch->vx = 0;
                scratch->vz = 0;
                random      = (Gp_LcgState * 5) + 0x71357911;
                scratch->vy = -((random >> 0x10) & 0x1FF);
                Gp_LcgState = (s32)random;
                Gp_SpawnEff(0x600E0, &actor->field_2C->field_8[3], 0x400, scratch);
            }
            break;
    }
    animWork = actor->field_1C;
    i        = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0U;
        duration            = Actor02000_D03784[animWork->field_694];
        do {
            func_800B4114(animWork, i, animWork->field_694, 0, (s32)duration);
            i++;
        } while (i < 0x13);
        coord->flg = 0;
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)(animWork->field_698 + i);
        do {
            Gp_AnimTickIndex(animWork, i);
            i += 1;
        } while (i < 0x13);
        coord->flg = 0;
    }
    actor->field_2C->field_8[3].flg = 0;
    Gp_UpdateCoord(coord);
    rootA  = actor->field_2C->field_8;
    pos.vx = rootA->workm.t[0];
    pos.vy = rootA->workm.t[1];
    pos.vz = rootA->workm.t[2];
    Gp_UpdateActorColor(actor->field_20, &pos, 0, 0);
    rootB  = actor->field_2C->field_8;
    partA  = &rootB[3];
    pos.vx = partA->workm.t[0];
    pos.vy = rootB->workm.t[1];
    pos.vz = partA->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
    *(u32*)0x1F8003FC += 8;
    return;
}

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

void        Actor02000_Fn00078(Actor02000*);
void        Actor02000_Fn01698(Actor02000*);
extern void (*Actor02000_D16064[])(Actor02000*);

void Actor02000_Fn02A34(Actor02000Ctx* ctx, Actor02000* actor)
{
    VECTOR3         pos;
    Actor02000Ctx*  spawn;
    TmdObject*      model;
    Actor02000Work* moveWork;
    Actor02000Work* animWork;
    Actor02000Work* work;
    Actor02000Work* flagWork;
    GsCOORDINATE2*  moveCoord;
    GsCOORDINATE2*  part;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  root;
    s16             duration;
    s32             i;
    u8              flags;

    work  = actor->field_1C;
    model = actor->field_2C;
    coord = model->field_8;
    switch (D_801153F4) {
        case 0:
            model->field_C    = 0;
            ctx->node.field_4 = 0;
            break;
        case 1:
            goto draw;
        case 2:
            model->field_C    = 0x80;
            ctx->node.field_4 = 1;
            return;
    }

    if (ctx->field_4C != 0) {
        spawn    = actor->field_20;
        flags    = spawn->field_4C;
        flagWork = actor->field_1C;
        if ((flags & 2) && (flagWork->field_6B8 == 0)) {
            spawn->field_4C     = flags & 0xFD;
            flagWork->field_6A6 = 0xA;
            flagWork->field_694 = 0x14;
            flagWork->field_6A8 = 0;
            flagWork->field_6E0 = 1;
        }
    }
    Actor02000_Fn00078(actor);
    Actor02000_D16064[work->field_6A6](actor);
    if (work->field_69E != 0) {
        Actor02000_Fn0150C(actor);
    }
    moveCoord              = actor->field_2C->field_8;
    moveWork               = actor->field_1C;
    moveWork->field_678    = (s32)moveCoord->coord.t[0];
    moveWork->field_67C    = (s32)moveCoord->coord.t[1];
    moveWork->field_680    = (s32)moveCoord->coord.t[2];
    moveCoord->coord.t[0] += (s32)(moveCoord->coord.m[0][2] * moveWork->field_69C) >> 0xC;
    if (moveWork->field_6DE < 2) {
        moveCoord->coord.t[1] += 0x80;
    }
    moveCoord->coord.t[2] += (s32)(moveCoord->coord.m[2][2] * moveWork->field_69C) >> 0xC;
    animWork               = actor->field_1C;
    i                      = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0;
        duration            = Actor02000_D03784[animWork->field_694];
        do {
            func_800B4114(animWork, i, (s32)animWork->field_694, 0, (s32)duration);
            i += 1;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)animWork->field_698 + i;
        do {
            Gp_AnimTickIndex(animWork, i);
            i += 1;
        } while (i < 0x13);
    }
    if (work->field_6B4 != 0) {
        Actor02000_Fn01698(actor);
    }
    Actor02000_Fn018A4(actor);
    coord->flg                      = 0;
    actor->field_2C->field_8[3].flg = 0;
    Gp_UpdateCoord(coord);
draw:
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(actor->field_20, &pos, 0, 0);
    root   = actor->field_2C->field_8;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
    return;
}

void Actor02000_Fn02D5C(Actor02000* arg0)
{
    s16             yaw;
    s16             yaw2;
    s16             state;
    s16             deltaYaw;
    s16             deltaYaw2;
    s16             speed;
    s32             magnitude;
    s32             magnitude2;
    s16             wrapped;
    s16             wrapped2;
    s16             angle;
    s32             dx;
    s32             dz;
    u16             flags;
    u16             flags2;
    u8*             head;
    VECTOR*         delta;
    Actor02000Work* work;
    GsCOORDINATE2*  coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = arg0->field_1C;
    state                 = work->field_6A8;
    coord                 = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= Actor02000_D03784[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            delta->vz       = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | 0xC000;
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & 0x3FFF);
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            dz              = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
            } else {
                work->field_6A6 = 3;
                work->field_6A8 = 0;
                work->field_694 = 5;
                work->field_6AE = 0;
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | 0xC000;
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & 0x3FFF);
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
                delta->vz       = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = work->field_6A4 - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
            }
            break;
    }
    *(s32*)G_SCRATCH_HEAD += 0x10;
}

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

void Actor02000_Fn035E8(Actor02000* arg0)
{
    Actor02000StateFuncTable3 sp;

    sp = Actor02000_D00060;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}

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

void Actor02000_Fn03690(void* arg0, Task* task)
{
    GpEffWork*      effect;
    Task*           parent;
    Actor02000Work* work;
    s16             count;

    parent                             = task->parent;
    work                               = (Actor02000Work*)parent->idMap;
    ((TmdObject*)task->extra)->field_C = (u16)((TmdObject*)parent->extra)->field_C;
    if (work->field_6D8 > 0) {
        count           = (u16)work->field_6D8 - 1;
        work->field_6D8 = count;
        if (count == 0) {
            effect = Gp_SpawnEff(D_8011572C | 0x80000000,
                                 &((TmdObject*)task->parent->extra)->field_8[7], 0, NULL);
            if (effect != NULL) {
                Task_Reparent(task, effect->field_0);
            }
        }
    }
}

void Actor02000_Fn03728(Actor02000* arg0)
{
    Actor02000StateFuncTable3 sp;

    sp = Actor02000_D0006C;
    sp.funcs[arg0->field_30](arg0->field_20, arg0);
}
