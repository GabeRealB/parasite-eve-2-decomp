#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_105600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/* `gte_MulMatrix0` from `psyq/gtemac.h`, except with the real `rtv0` / `rtir`
 * encodings this toolchain assembles correctly. */
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105600_80131EA0;

/// Nonzero parks the actor instead of running its state machine: 1 draws the
/// body where it stands and 2 fades it out.
extern u8 D_801153F4;

/// Frame counts of the actor's animations, indexed by `Actor105600Work.field_694`.
extern s16 D_actor_105600_80136B1C[];

/// The approach cycle's per-state handlers, indexed by `Actor105600Work.field_6A6`.
extern void (*D_actor_105600_80148360[])(Task*);

void Gp_AnimTickIndex(GpAnimCtx* arg0, s32 arg1);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_105600_80131EC4(Task* arg0);
void func_actor_105600_80133358(Task* arg0);
void func_actor_105600_80134368(Task* arg0);
void func_actor_105600_801334E4(Task* arg0);
void func_actor_105600_801336F0(Task* arg0);

/// Approach-cycle state machine, entry 6 of `D_actor_105600_80148360` for the
/// second half of the fight. State 0 waits out the opening clip; state 1 backs
/// away while tracking the player and running the aim helper, for 0x1E frames;
/// state 2 measures the distance and yaw error to the companion in slot 3 and
/// either breaks off (too close) or commits to the lunge; state 3 keeps facing
/// the companion, counts the strikes in `field_6BC` / `field_6BE` and picks the
/// follow-up clip from them; state 4 fires the effect burst; states 5 and 6
/// hand back to the other handlers. The delta vector and its normal are carved
/// off `G_SCRATCH_HEAD` and released on the way out.
void func_actor_105600_80133C3C(Task* arg0)
{
    s16              diff;
    s32              mag;
    s16              angle;
    s32              dx;
    s32              dz;
    VECTOR*          delta;
    VECTOR*          normal;
    VECTOR*          normal2;
    GsCOORDINATE2*   target;
    Actor105600Work* work;
    GsCOORDINATE2*   coord;

    delta = (VECTOR*)(*(u8**)G_SCRATCH_HEAD -= 0x20);
    work  = (Actor105600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    switch (work->field_6A8) {
        case 0:
            if (work->field_698 >= 0x14) {
                work->field_6A8 = 1;
                work->field_694 = 0x1E;
                work->field_6AE = 0;
            }
            break;
        case 1:
            work->field_69C = -0x16;
            work->field_69E = 0x1E;
            work->field_6CE = work->field_6D0 > 0;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
            func_actor_105600_80134368(arg0);
            work->field_6AE++;
            if (work->field_6AE >= 0x1E) {
                work->field_6A8        = 2;
                work->field_6AE        = 0;
                work->field_61C.flags &= 0x3FFF;
            }
            break;
        case 2:
            work->field_6CE = work->field_6D0 > 0;
            target          = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[2];
            delta->vx       = target->workm.t[0] - coord->workm.t[0];
            normal          = delta + 1;
            delta->vy       = target->workm.t[1] - coord->workm.t[1];
            delta->vz       = target->workm.t[2] - coord->workm.t[2];
            VectorNormal(delta, normal);
            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, delta);
            dx = delta->vx;
            dz = delta->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->field_6A6 = 7;
                work->field_6A8 = 0;
                work->field_694 = 0x10;
                work->field_6CE = 0;
                break;
            }
            diff = (ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF) - work->field_6A2;
            mag  = __builtin_abs(diff);
            if (mag < 0x800) {
                angle = mag;
            } else if (diff > 0) {
                angle = 0x1000 - diff;
            } else {
                angle = diff + 0x1000;
            }
            if (angle >= 0x101) {
                work->field_6A8 = 6;
                work->field_694 = 0xE;
                work->field_6CE = 0;
            } else {
                work->field_6A8 = 3;
                work->field_694 = 0xD;
                work->field_6CC = 1;
                work->field_6BA = 1;
                work->field_6B6 = 0;
                work->field_6BC++;
                work->field_6BE++;
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_6CE = work->field_6D0 > 0;
            if (work->field_698 < 3) {
                work->field_69E = 0;
            } else {
                target    = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[2];
                delta->vx = target->workm.t[0] - coord->workm.t[0];
                normal2   = delta + 1;
                delta->vy = target->workm.t[1] - coord->workm.t[1];
                delta->vz = target->workm.t[2] - coord->workm.t[2];
                VectorNormal(delta, normal2);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal2, delta);
                work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
                work->field_69E = 7;
            }
            if (work->field_6BE != 0 && work->field_698 == D_actor_105600_80136B1C[13] - 1) {
                work->field_6BA = 1;
                work->field_6BC++;
                work->field_6BE++;
            }
            if (work->field_6B6 >= 0x29) {
                work->field_6A6        = 8;
                work->field_6A8        = 0;
                work->field_69C        = 0;
                work->field_69E        = 0;
                work->field_6CC        = 0;
                work->field_6CE        = 0;
                work->field_5E4.flags &= 0x7FFF;
            } else if (work->field_6BC >= 6) {
                if (work->field_698 >= D_actor_105600_80136B1C[13] + 0x16) {
                    work->field_6A8             = 4;
                    work->field_694             = 0xF;
                    work->field_6BC             = 0;
                    work->field_6BE             = 0;
                    D_actor_105600_80136B1C[13] = 0;
                    work->field_6CC             = 0;
                }
            } else if (work->field_6BE < 6) {
                if (work->field_698 >= D_actor_105600_80136B1C[13] + 3) {
                    D_actor_105600_80136B1C[13] = 3;
                    work->field_694             = 0xD;
                    work->field_696             = 0x1E;
                }
            } else if (work->field_698 >= D_actor_105600_80136B1C[13] + 0x16) {
                work->field_6A8             = 1;
                work->field_6BE             = 0;
                work->field_694             = 0x1E;
                D_actor_105600_80136B1C[13] = 0;
                work->field_6CC             = 0;
            }
            break;
        case 4:
            if (work->field_698 == 0x1A) {
                Gp_SpawnEff(0x6006E, &((TmdObject*)arg0->extra)->field_8[7], 0x6000C, NULL);
            }
            work->field_6CE = 0;
            if (work->field_698 >= 0x87) {
                work->field_6A8 = 5;
            }
            break;
        case 5:
            work->field_6A6 = 2;
            work->field_6A8 = 2;
            work->field_694 = 4;
            break;
        case 6:
            if (work->field_698 >= 0x19) {
                work->field_6A6 = 2;
                work->field_6A8 = 0;
                work->field_694 = 2;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += 0x20;
}

INCLUDE_RODATA("actors/nonmatchings/actor_105600/actor_105600_2", D_actor_105600_80131EA0);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600_2", func_actor_105600_801341C0);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600_2", func_actor_105600_80134368);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600_2", func_actor_105600_80134770);

/// Places a fresh body block for the actor: allocates the 0xF0-byte work
/// block, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `GpRec18` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into
/// `D_actor_105600_80148110`.
void func_actor_105600_80134FD0(GpEnemy* arg0, Task* arg1)
{
    Actor105600FxWork*       work;
    Actor105600PlaceScratch* scratch;
    Actor105600Ctx*          ctx;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           parentCoord;
    TmdObject*               tmd;
    Task*                    parent;
    s32                      sound;
    s32                      pan;

    tmd         = arg1->extra;
    coord       = tmd->field_8;
    parent      = arg1->parent;
    parentCoord = ((TmdObject*)parent->extra)->field_8;
    work        = Mem_Calloc(0xF0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work    = (TaskIdMap*)work;
    tmd->field_C  = 0;
    scratch       = (Actor105600PlaceScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x38);
    tmd->field_1C = &work->lightMtx;
    tmd->field_20 = &work->colorMtx;

    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = 0;
    scratch->rot.vy = 0x1F4;
    scratch->rot.vz = 0x64;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    coord->sub         = &Gfx_ViewCoord;
    coord->coord.t[0] += scratch->pos.vx;
    coord->coord.t[1] += scratch->pos.vy;
    coord->coord.t[2] += scratch->pos.vz;

    scratch->rot.vx = 0x80;
    scratch->rot.vy = 0;
    scratch->rot.vz = 0x10;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir_real();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][2]);

    work->field_EE = (D_actor_105600_80147FDC.field_E != 1);

    work->obj40.field_8  = coord;
    work->obj40.field_C  = work->rec60;
    work->obj40.field_10 = 0;
    work->obj40.field_12 = 0;
    work->obj40.field_14 = 0;
    work->obj40.field_18 = Gp_PackPair(&D_actor_105600_80147FDC.pair, 3);
    work->obj40.field_1C = 0x64;
    work->obj40.flags    = 1;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(work->rec60, 1, 0);
    work->obj40.flags |= 0x8000;

    work->obj78.field_8  = coord;
    work->obj78.field_C  = work->rec60;
    work->obj78.field_10 = 0;
    work->obj78.field_12 = 0;
    work->obj78.field_14 = 0;
    work->obj78.field_18 = 0x22B2B;
    work->obj78.field_1C = 0x64;
    work->obj78.flags    = 1;
    Gp_LinkObj(1, &work->obj78);
    work->obj78.flags |= 0x8000;

    work->d4rec.field_0  = 0;
    work->d4rec.field_2  = 0;
    work->d4rec.field_4  = 0;
    work->d4rec.field_8  = 0;
    work->d4rec.field_A  = -0x1F4;
    work->d4rec.field_C  = 0;
    work->d4rec.field_10 = 1;
    work->d4rec.field_12 = 1;
    work->d4rec.field_14 = work->recD0;
    work->obj98.field_C  = (GpRec18*)&work->d4rec;
    work->obj98.field_8  = coord;
    work->obj98.field_10 = 0;
    work->obj98.field_12 = 0;
    work->obj98.field_14 = 0;
    work->obj98.field_18 = 0;
    work->obj98.field_1C = 0;
    work->obj98.flags    = 3;
    Gp_LinkObj(3, &work->obj98);
    Gp_InitRec18Table(work->recD0, 1, 0);
    work->obj98.flags |= 0x4400;

    arg1->state = 1;
    Task_DetachFromParent(arg1);

    coord->flg = 0;
    Gp_UpdateCoord(coord);

    ctx   = arg1->spawnArg2;
    sound = D_actor_105600_80148110 | (((u16)ctx->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));

    *(u8**)G_SCRATCH_HEAD += 0x38;
}

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600_2", func_actor_105600_80135410);

/// Spawn handler of the approach cycle: allocates the 0x6E4-byte work block,
/// binds the animation set and reseeds the nineteen slots, then starts the
/// companion enemy whose model takes its texture page and CLUT row from the
/// current room's area record. `GpEnemy::field_4B` picks how much of that is
/// kept: 0 also links the list node, the five `Gp_LinkObj` collision nodes with
/// their `GpRec18` tables and the room's streaming cue, while 1 and 2 only
/// prime the animation state. The same body as `Actor02000_Fn0251C` of
/// `actor_102000`, with a fifth collision node and a 0x38 rather than 0x14 cue
/// channel.
void func_actor_105600_80135744(GpEnemy* ctx, Task* actor)
{
    Actor105600Work*    work;
    TmdObject*          obj;
    TmdObject*          model;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      parts;
    GsCOORDINATE2*      partsA;
    GsCOORDINATE2*      partsB;
    GsCOORDINATE2*      partsC;
    GsCOORDINATE2*      partsD;
    GsCOORDINATE2*      effParts;
    GpAreaKey*          sessionKey;
    GpAreaKey*          keyPtr;
    u8                  areaByte0;
    GpAreaRec*          rec;
    Actor105600AreaRec* entry;
    GpEnemy*            eff;
    u16*                tbl;
    u8                  param1[8];
    u8                  param2[8];
    GpAreaKey           key;
    s32                 i;
    s32                 one;
    s32                 kind;
    s32                 idx;
    s32                 param;

    obj   = actor->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work     = (TaskIdMap*)work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = &work->field_45C;
    obj->field_20   = &work->field_43C;
    work->field_6CA = 0x38;
    work->field_66C = D_actor_105600_801482C0;
    work->field_670 = &((TmdObject*)actor->extra)->field_8[3];
    work->field_674 = 0x500;
    work->field_676 = 2;
    func_800B3F84(&work->ctx, &D_actor_105600_801482E4, (GpAnimObj*)obj, work->field_30C, work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->ctx, i, 1);
    }
    eff         = Gp_SpawnEnemyFromTable(D_actor_105600_801482C0, 1, 0, ctx);
    sessionKey  = (GpAreaKey*)&Game_Session->field_4;
    model       = eff->task->extra;
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
    entry           = (Actor105600AreaRec*)((idx << 4) + (s32)rec->field_0);
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
    parts            = ((TmdObject*)actor->extra)->field_8;
    ctx->field_1C.vx = 0;
    ctx->field_1C.vy = 0;
    ctx->field_1C.vz = 0;
    ctx->field_50    = D_actor_105600_80147FF0;
    ctx->field_54    = (s32)work->field_4EC;
    ctx->field_18    = &parts[3];
    ctx->field_40    = D_actor_105600_80147FF0->field_4;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_6AC = ((GpAreaPlace*)ctx->field_3C)->field_2 & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = ((GpAreaPlace*)ctx->field_3C)->field_1;
        work->field_6DA = param * 1000;
    }

    tbl = D_actor_105600_80148298[Game_Session->field_7];
    if (tbl != NULL) {
        work->field_6D6 = tbl[Game_Session->field_6];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x38;
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
    partsA                   = ((TmdObject*)actor->extra)->field_8;
    work->field_47C.field_C  = (GpRec18*)&work->field_49C;
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

    partsB                   = ((TmdObject*)actor->extra)->field_8;
    work->field_4CC.field_C  = work->field_4EC;
    work->field_4CC.field_10 = 0;
    work->field_4CC.field_12 = 0;
    work->field_4CC.field_14 = 0;
    work->field_4CC.field_18 = 0x30038;
    work->field_4CC.field_1C = 0x190;
    work->field_4CC.flags    = 1;
    work->field_4CC.field_8  = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= 0x8000;

    partsC                   = ((TmdObject*)actor->extra)->field_8;
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

    effParts                 = ((TmdObject*)eff->task->extra)->field_8;
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

    work->field_63C.field_0  = 0;
    work->field_63C.field_2  = 0;
    work->field_63C.field_4  = 0;
    work->field_63C.field_8  = 0;
    work->field_63C.field_A  = 0;
    work->field_63C.field_C  = 0;
    work->field_63C.field_10 = 1;
    work->field_63C.field_12 = 1;
    work->field_63C.field_14 = work->field_654;
    partsD                   = ((TmdObject*)actor->extra)->field_8;
    work->field_61C.field_C  = (GpRec18*)&work->field_63C;
    work->field_61C.field_10 = 0;
    work->field_61C.field_12 = 0;
    work->field_61C.field_14 = 0;
    work->field_61C.field_18 = 0;
    work->field_61C.field_1C = 0;
    work->field_61C.flags    = 3;
    work->field_61C.field_8  = partsD;
    Gp_LinkObj(3, &work->field_61C);
    Gp_InitRec18Table(work->field_654, 1, 0);
    work->field_61C.flags = (work->field_61C.flags & 0x3FFF) | 0xC00;
    actor->state          = 1;
    return;

case1:
    work->field_694 = 0x19;
    work->field_6A8 = 2;
    actor->state    = 2;
    return;

case2:
    work->field_694 = 0x1D;
    work->field_6A8 = kind;
    actor->state    = kind;
}

/// Per-frame tick of the approach cycle: runs the collision and state handlers,
/// drifts the root coordinate forward along its Z axis (and upward while
/// `field_6DE` is below 2), reseeds or ticks the nineteen animation slots, then
/// publishes the body's colour and its ground shadow. `D_801153F4` overrides
/// the whole state machine - 1 draws the body without advancing it and 2 parks
/// it faded out. The same body as `Actor02000_Fn02A34` of `actor_102000`.
void func_actor_105600_80135CDC(GpEnemy* ctx, Task* actor)
{
    VECTOR3          pos;
    Actor105600Ctx*  spawn;
    TmdObject*       model;
    Actor105600Work* moveWork;
    Actor105600Work* animWork;
    Actor105600Work* work;
    Actor105600Work* flagWork;
    GsCOORDINATE2*   moveCoord;
    GsCOORDINATE2*   part;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    s16              duration;
    s32              i;
    u8               flags;

    work  = (Actor105600Work*)actor->work;
    model = (TmdObject*)actor->extra;
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
        spawn    = (Actor105600Ctx*)actor->spawnArg2;
        flags    = spawn->field_4C;
        flagWork = (Actor105600Work*)actor->work;
        if ((flags & 2) && (flagWork->field_6B8 == 0)) {
            spawn->field_4C     = flags & 0xFD;
            flagWork->field_6A6 = 0xA;
            flagWork->field_694 = 0x14;
            flagWork->field_6A8 = 0;
            flagWork->field_6E0 = 1;
        }
    }
    func_actor_105600_80131EC4(actor);
    D_actor_105600_80148360[work->field_6A6](actor);
    if (work->field_69E != 0) {
        func_actor_105600_80133358(actor);
    }
    moveCoord              = ((TmdObject*)actor->extra)->field_8;
    moveWork               = (Actor105600Work*)actor->work;
    moveWork->field_678    = moveCoord->coord.t[0];
    moveWork->field_67C    = moveCoord->coord.t[1];
    moveWork->field_680    = moveCoord->coord.t[2];
    moveCoord->coord.t[0] += (s32)(moveCoord->coord.m[0][2] * moveWork->field_69C) >> 0xC;
    if (moveWork->field_6DE < 2) {
        moveCoord->coord.t[1] += 0x80;
    }
    moveCoord->coord.t[2] += (s32)(moveCoord->coord.m[2][2] * moveWork->field_69C) >> 0xC;
    animWork               = (Actor105600Work*)actor->work;
    i                      = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0;
        duration            = D_actor_105600_80136B1C[animWork->field_694];
        do {
            func_800B4114(&animWork->ctx, i, animWork->field_694, 0, duration);
            i += 1;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)animWork->field_698 + i;
        do {
            Gp_AnimTickIndex(&animWork->ctx, i);
            i += 1;
        } while (i < 0x13);
    }
    if (work->field_6B4 != 0) {
        func_actor_105600_801334E4(actor);
    }
    func_actor_105600_801336F0(actor);
    coord->flg                                 = 0;
    ((TmdObject*)actor->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(coord);
draw:
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)actor->spawnArg2, (VECTOR*)&pos, 0, 0);
    root   = ((TmdObject*)actor->extra)->field_8;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
}

/// Approach-cycle state machine, entry 6 of `D_actor_105600_80148360`. State 0
/// turns the actor toward the player, handing over to the charge animation once
/// the clip has run and the yaw error is wide (or to the recovery animation when
/// `field_6DC` is set); state 1 picks the close or far attack from the distance
/// to the player; state 2 waits out its clip before turning again; state 3 arms
/// `field_6DC` and drops back to state 0.
void func_actor_105600_80136004(Task* arg0)
{
    s16              yaw;
    s16              yaw2;
    s16              state;
    s16              deltaYaw;
    s16              deltaYaw2;
    s16              speed;
    s32              magnitude;
    s32              magnitude2;
    s16              wrapped;
    s16              wrapped2;
    s16              angle;
    s32              dx;
    s32              dz;
    u16              flags;
    u16              flags2;
    u8*              head;
    VECTOR*          delta;
    Actor105600Work* work;
    GsCOORDINATE2*   coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = (Actor105600Work*)arg0->work;
    state                 = work->field_6A8;
    coord                 = ((TmdObject*)arg0->extra)->field_8;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= D_actor_105600_80136B1C[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x1E;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            dz              = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->field_6A6 = 7;
                work->field_6A8 = 0;
                work->field_694 = 0x10;
            } else {
                work->field_6A6 = 6;
                work->field_6A8 = 0;
                work->field_694 = 0xC;
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
                delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
