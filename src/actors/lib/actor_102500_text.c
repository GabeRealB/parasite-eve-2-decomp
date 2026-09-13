#include "common.h"

#include "actors/actor_102500.h"
#include "actors/actors_shared_80135b58.h"
#include "main/tmd.h"
#include "main/session.h"
#include "main/wipsys.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s32 Gp_LcgState;
extern s16 Actor02500_D05BD0[];
extern s16 Actor02500_D05B48[];
extern s16 Actor02500_D05B58[];

void Gp_AnimTickIndex(Actor02500Work* arg0, s32 arg1);
void func_800B4114(Actor02500Work* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void* Mem_Calloc(s32 size, s32 arg1);
void  Gp_DestroyEnemy(Actor02500Ctx* ctx, Actor02500* actor);
void  Gp_LinkNode(Actor02500Node* node);
void  func_800B3F84(Actor02500Work* arg0, void* arg1, Actor02500Obj2C* arg2, void* arg3,
                    Actor02500AnimSlots* arg4);
void  Gp_AnimResetSlot(Actor02500Work* arg0, s32 arg1, s32 arg2);
void  Gp_IncStateF0Ref(s32 arg0);
void  Gp_SetLightMode(Actor02500Ctx* arg0, s32 arg1);
void  Gp_LinkObj(s32 arg0, Actor02500Obj* arg1);
void  Gp_InitRec18Table(Actor02500Rec18* arg0, s32 arg1, s32 arg2);
s32   Gp_PackPair(void* arg0, s32 arg1);
void  Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void Actor02500_Fn00078(Actor02500Ctx* ctx, Actor02500* actor)
{
    Actor02500Work*  work;
    Actor02500Obj2C* obj;
    GsCOORDINATE2*   coord;
    s32              i;

    obj   = actor->field_2C;
    coord = obj->field_8;
    work  = Mem_Calloc(0x348, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->field_1C = work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = work->field_14C;
    obj->field_20   = work->field_12C;
    ctx->field_4    = &coord->coord;
    ctx->field_48   = 0;
    Gp_LinkNode(&ctx->node);
    ctx->field_20     = -0x96;
    ctx->field_18     = coord;
    ctx->node.field_4 = 0;
    ctx->field_1C     = 0;
    ctx->field_24     = 0;
    ctx->field_50     = Actor02500_D05B38;
    ctx->field_40     = Actor02500_D05B38->field_4;
    work->field_2E0   = 0x200;
    work->field_2DC   = coord;
    work->field_2E2   = 1;
    func_800B3F84(work, &Actor02500_D05BA0, obj, work->field_DC, &work->field_14);
    work->field_31C = 1;
    work->field_31E = 1;
    for (i = 1; i < 5; i++) {
        Gp_AnimResetSlot(work, i, work->field_31C);
    }
    Gp_IncStateF0Ref(0);
    switch (ctx->field_3C->field_2) {
        case 0:
            work->field_322 = 0;
            work->field_324 = 0;
            ctx->field_54   = work->field_1C4;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_32E = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x1E;
            break;
        case 1:
            work->field_322 = 5;
            work->field_324 = 0;
            ctx->field_54   = NULL;
            Gp_SetLightMode(ctx, 2);
            break;
        case 2:
            work->field_322 = 5;
            work->field_324 = 1;
            ctx->field_54   = NULL;
            Gp_SetLightMode(ctx, 2);
            break;
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_338 = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x1E;
    work->field_314 = coord->coord.t[0];
    work->field_316 = coord->coord.t[1];
    work->field_318 = coord->coord.t[2];
    work->field_32A = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;

    work->field_16C.field_8  = coord;
    work->field_16C.field_C  = work->field_18C;
    work->field_16C.field_10 = 0;
    work->field_16C.field_12 = -0x190;
    work->field_16C.field_14 = 0x258;
    work->field_16C.field_18 = 0;
    work->field_16C.field_1C = 0x258;
    work->field_16C.flags    = 1;
    Gp_LinkObj(3, &work->field_16C);
    Gp_InitRec18Table(work->field_18C, 1, 0);

    work->field_1A4.field_8  = coord;
    work->field_1A4.field_C  = work->field_1C4;
    work->field_1A4.field_10 = 0;
    work->field_1A4.field_12 = -0x12C;
    work->field_1A4.field_14 = 0;
    work->field_1A4.field_18 = 0x30019;
    work->field_1A4.field_1C = 0x12C;
    work->field_1A4.flags    = 1;
    work->field_16C.flags   |= 0x8000;
    Gp_LinkObj(2, &work->field_1A4);
    Gp_InitRec18Table(work->field_1C4, 3, 0);

    if (ctx->field_3C->field_2 == 0) {
        work->field_1A4.flags |= 0x8000;
    } else {
        work->field_1A4.flags &= 0x7FFF;
    }

    work->field_20C.field_8  = coord;
    work->field_20C.field_C  = work->field_22C;
    work->field_20C.field_10 = 0;
    work->field_20C.field_12 = -0x12C;
    work->field_20C.field_14 = 0;
    work->field_20C.field_18 = 0x30019;
    work->field_20C.field_1C = 0x12C;
    work->field_20C.flags    = 1;
    Gp_LinkObj(2, &work->field_20C);
    Gp_InitRec18Table(work->field_22C, 5, 0);
    work->field_20C.flags |= 0x4200;

    work->field_2A4.field_8  = actor->field_2C->field_8 + 4;
    work->field_2A4.field_C  = work->field_2C4;
    work->field_2A4.field_10 = 0;
    work->field_2A4.field_12 = -0x3B6;
    work->field_2A4.field_14 = 0x1CC;
    work->field_2A4.field_18 = Gp_PackPair(&Actor02500_D05B30, 0);
    work->field_2A4.field_1C = 0x12C;
    work->field_2A4.flags    = 1;
    Gp_LinkObj(3, &work->field_2A4);
    Gp_InitRec18Table(work->field_2C4, 1, 0);
    work->field_2A4.flags &= 0x7FFF;
    actor->field_30        = 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102500_text", Actor02500_Fn00494);

void Actor02500_Fn00B18(Actor02500* actor)
{
    Actor02500Work* work;
    GsCOORDINATE2*  coord;
    s16             timer;
    s16             moveTimer;
    s16             state;
    s32             randomAngle;
    s32             randomMoveTime;
    s32             dx;
    s32             randomIdleTime;
    s32             dz;
    s32             idleTime;
    VECTOR*         vector;
    VECTOR*         scratchEnd;

    scratchEnd = (VECTOR*)SCRATCH_SP;
    vector     = scratchEnd - 1;
    SCRATCH_SP = (u32)vector;
    work       = actor->field_1C;
    state      = work->field_324;
    coord      = actor->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_31C = 1;
            work->field_326 = 0;
            timer           = (u16)work->field_32E - 1;
            work->field_32E = timer;
            if (timer <= 0) {
                work->field_336 = 0;
                work->field_31C = 3;
                work->field_324 = 1;
                randomAngle     = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = randomAngle;
                work->field_32A = ((u32)randomAngle >> 0x10) & 0xFFF;
            }
            break;
        case 1:
            work->field_326 = 0;
            if (work->field_32C == (s16)work->field_32A) {
                work->field_324 = 2;
                randomMoveTime  = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = randomMoveTime;
                work->field_32E = (((u32)randomMoveTime >> 0x10) & 0x7F) + 0x1E;
            }
            break;
        case 2:
            work->field_326   = (s16)Actor02500_D05B58[actor->field_20->field_3C->field_F];
            scratchEnd[-1].vx = (s32)(work->field_314 - coord->coord.t[0]);
            vector->vy        = 0;
            dz                = work->field_318 - coord->coord.t[2];
            vector->vz        = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) >= 0x7D0) && (work->field_336 == 0)) {
                work->field_324 = 3;
            } else {
                if (work->field_340 != 1) {
                    moveTimer       = (u16)work->field_32E - 1;
                    work->field_32E = moveTimer;
                    if (moveTimer > 0) {
                        break;
                    }
                }
                work->field_324 = 0;
                randomIdleTime  = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = randomIdleTime;
                idleTime        = ((u32)randomIdleTime >> 0x10) & 0x3F;
                work->field_32E = idleTime + 0x1E;
            }
            break;
        case 3:
            scratchEnd[-1].vx = (s32)(work->field_314 - coord->coord.t[0]);
            vector->vy        = 0;
            vector->vz        = (s32)(work->field_318 - coord->coord.t[2]);
            work->field_32A   = ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)vector->vz) & 0xFFF;
            work->field_336   = 1;
            work->field_324   = 1;
            break;
    }
    work->field_328 = (s16)Actor02500_D05B48[actor->field_20->field_3C->field_F];
    SCRATCH_SP     += 0x10;
}

void Actor02500_Fn00DD8(Actor02500* actor)
{
    Actor02500Work* work;
    GsCOORDINATE2*  coord;
    s16             timer;
    s32             state;
    s16             diff;
    s32             frame;
    s32             absDiff;
    s16             angle;
    s32             sound;
    s32             dx;
    s32             dz;
    s32             homeDx;
    s32             homeDz;
    s32             pan;
    VECTOR*         vector;
    VECTOR*         scratchEnd;

    scratchEnd = (VECTOR*)SCRATCH_SP;
    vector     = scratchEnd - 1;
    SCRATCH_SP = (u32)vector;
    work       = actor->field_1C;
    state      = work->field_324;
    coord      = actor->field_2C->field_8;
    switch (state) {
        case 0:
            work->field_31C = 4;
            work->field_32E = 0xF0;
            work->field_326 = 0;
            work->field_324 = 1;
            break;
        case 1:
            work->field_326   = (s16)Actor02500_D05B78[actor->field_20->field_3C->field_F];
            scratchEnd[-1].vx = Wip_SysConfig.field_4->t[0] - coord->coord.t[0];
            vector->vy        = 0;
            vector->vz        = Wip_SysConfig.field_4->t[2] - coord->coord.t[2];
            work->field_32A   = ratan2((s16)scratchEnd[-1].vx, (s16)vector->vz) & 0xFFF;
            dx                = scratchEnd[-1].vx;
            dz                = vector->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) {
                diff            = work->field_32A - (u16)work->field_32C;
                absDiff         = diff >= 0 ? diff : -diff;
                work->field_326 = 0;
                if (absDiff < 0x800) {
                    angle = absDiff;
                } else if (diff > 0) {
                    angle = 0x1000 - diff;
                } else {
                    angle = diff + 0x1000;
                }
                if (angle < 0x30) {
                    work->field_324 = 2;
                    work->field_31C = 6;
                }
            } else {
                timer           = (u16)work->field_32E - 1;
                work->field_32E = timer;
                if (timer <= 0) {
                    scratchEnd[-1].vx = Wip_SysConfig.field_4->t[0] - work->field_314;
                    vector->vy        = 0;
                    homeDz            = Wip_SysConfig.field_4->t[2] - work->field_318;
                    vector->vz        = homeDz;
                    homeDx            = scratchEnd[-1].vx;
                    if (SquareRoot0((homeDx * homeDx) + (homeDz * homeDz)) >= 0x7D1) {
                        work->field_324 = 3;
                    }
                }
            }
            break;
        case 2:
            frame           = (s16)work->field_320;
            work->field_326 = 0;
            work->field_342 = 1;
            if (frame == 41) {
                work->field_2A4.flags |= 0x8000;
            } else if (frame == 42) {
                sound = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40190005;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(coord));
            } else if (frame == 44) {
                work->field_342        = 0;
                work->field_2A4.flags &= 0x7FFF;
            } else if (frame >= 76) {
                work->field_324 = 1;
                work->field_31C = 4;
            }
            break;
        case 3:
            work->field_322 = 0;
            work->field_324 = state;
            work->field_31C = state;
            break;
    }
    work->field_328 = (s16)Actor02500_D05B68[actor->field_20->field_3C->field_F];
    SCRATCH_SP     += 0x10;
}

void Actor02500_Fn01144(Actor02500* actor)
{
    Actor02500Work* work;
    GsCOORDINATE2*  coord;
    s32             sound;
    s32             pan;
    s32             pan9;
    s32             pan18;
    u32             random;

    work  = actor->field_1C;
    coord = actor->field_2C->field_8;
    work->field_338--;
    if (work->field_338 <= 0) {
        random          = Gp_LcgState * 5 + 0x71357911;
        work->field_338 = ((random >> 16) & 0x7F) + 0x1E;
        Gp_LcgState     = random;
        sound           = ((actor->field_20->field_8 >> 12) << 8) | 0x40190008;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(coord));
    }
    if (work->field_326 != 0) {
        work->field_33A++;
        if (work->field_33A == 9) {
            sound = ((actor->field_20->field_8 >> 12) << 8) | 0x40190001;
            pan9  = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan9, (s8)Gp_GetObjDepth(coord));
        } else if (work->field_33A == 18) {
            sound = ((actor->field_20->field_8 >> 12) << 8) | 0x40190002;
            pan18 = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(sound, pan18, (s8)Gp_GetObjDepth(coord));
            work->field_33A = 0;
        }
    } else {
        work->field_33A = 0;
    }
}
