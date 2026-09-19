#include "common.h"

#include "actors/actor_102500.h"
#include "actors/actors_shared_80135b58.h"
#include "main/tmd.h"
#include "main/session.h"
#include "main/wipsys.h"
#include "main/mem.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s32 Gp_LcgState;
extern s16 Actor02500_D05BD0[];
extern s16 Actor02500_D05B48[];
extern s16 Actor02500_D05B58[];

void Gp_AnimTickIndex(Actor02500Work* arg0, s32 arg1);
void func_800B4114(Actor02500Work* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void  Gp_DestroyEnemy(Actor02500Ctx* ctx, Actor02500* actor);
void  func_800B3F84(Actor02500Work* arg0, void* arg1, Actor02500Obj2C* arg2, void* arg3,
                    Actor02500AnimSlots* arg4);
void  Gp_AnimResetSlot(Actor02500Work* arg0, s32 arg1, s32 arg2);
void  Gp_IncStateF0Ref(s32 arg0);
void  Gp_SetLightMode(Actor02500Ctx* arg0, s32 arg1);
void  Gp_LinkObj(s32 arg0, GpObj* arg1);
void  Gp_InitRec18Table(GpRec18* arg0, s32 arg1, s32 arg2);
s32   Gp_PackPair(void* pairs, s32 index);
void  Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void  Gp_ClearRec18Occupied(GpRec18* arg0);
s32   Gp_FindRec18(GpRec18* arg0, s32 arg1);
s32   Gp_CountRec18Hi(GpRec18* arg0, s32 arg1);
s32   func_800E0C10(GpRec18* arg0, Actor02500MoveScratch* arg1, s32 arg2, s32 arg3);
s32   Gp_ComputeDamage(u32 arg0, s32 arg1, s32 arg2, s32 arg3);
s32   Gp_GetIdParam0(u32 arg0);
s32   Gp_GetIdParam1(u32 arg0);
s32   Gp_GetIdParam2(u32 arg0);
s32   Gp_RollEnemyChance(Actor02500Ctx* arg0, u32 arg1, s32 arg2);
void  Gp_SetObjFlag1(Actor02500Ctx* arg0);
void  Gp_SetObjFlag2(Actor02500Ctx* arg0, u32 arg1, s32 arg2);
void  func_800E2C78(Actor02500Ctx* arg0, u32 arg1, s32 arg2, s32 arg3);
void  func_800DA6E8(GpLinkNode* arg0, s32 arg1, s32 arg2);
void* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3);
s32   Gp_GetObjPan(GsCOORDINATE2* arg0);
s32   Gp_GetObjDepth(GsCOORDINATE2* arg0);
s32   SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void  Gp_ArmStateF0(s32 arg0);

extern Actor02500GridParams* Gp_GridParams;

void Actor02500_Fn00078(Actor02500Ctx* ctx, Actor02500* actor)
{
    Actor02500Work*  work;
    Actor02500Obj2C* obj;
    GsCOORDINATE2*   coord;
    s32              i;

    obj   = actor->field_2C;
    coord = obj->field_8;
    work  = memCalloc(0x348, 0);
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
    ctx->field_20   = -0x96;
    ctx->field_18   = coord;
    ctx->node.flags = 0;
    ctx->field_1C   = 0;
    ctx->field_24   = 0;
    ctx->field_50   = &Actor02500_D05B38;
    ctx->field_40   = Actor02500_D05B38.hpMax;
    work->field_2E0 = 0x200;
    work->field_2DC = coord;
    work->field_2E2 = 1;
    func_800B3F84(work, &Actor02500_D05BA0, obj, work->field_DC, &work->field_14);
    work->field_31C = 1;
    work->field_31E = 1;
    for (i = 1; i < 5; i++) {
        Gp_AnimResetSlot(work, i, work->field_31C);
    }
    Gp_IncStateF0Ref(0);
    switch (ctx->field_3C->mode) {
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

    work->obj16C.coord    = coord;
    work->obj16C.ctx.recs = work->field_18C;
    work->obj16C.pos.vx   = 0;
    work->obj16C.pos.vy   = -0x190;
    work->obj16C.pos.vz   = 0x258;
    work->obj16C.key      = 0;
    work->obj16C.radius   = 0x258;
    work->obj16C.flags    = 1;
    Gp_LinkObj(3, &work->obj16C);
    Gp_InitRec18Table(work->field_18C, 1, 0);

    work->obj1A4.coord    = coord;
    work->obj1A4.ctx.recs = work->field_1C4;
    work->obj1A4.pos.vx   = 0;
    work->obj1A4.pos.vy   = -0x12C;
    work->obj1A4.pos.vz   = 0;
    work->obj1A4.key      = 0x30019;
    work->obj1A4.radius   = 0x12C;
    work->obj1A4.flags    = 1;
    work->obj16C.flags   |= 0x8000;
    Gp_LinkObj(2, &work->obj1A4);
    Gp_InitRec18Table(work->field_1C4, 3, 0);

    if (ctx->field_3C->mode == 0) {
        work->obj1A4.flags |= 0x8000;
    } else {
        work->obj1A4.flags &= 0x7FFF;
    }

    work->obj20C.coord    = coord;
    work->obj20C.ctx.recs = work->field_22C;
    work->obj20C.pos.vx   = 0;
    work->obj20C.pos.vy   = -0x12C;
    work->obj20C.pos.vz   = 0;
    work->obj20C.key      = 0x30019;
    work->obj20C.radius   = 0x12C;
    work->obj20C.flags    = 1;
    Gp_LinkObj(2, &work->obj20C);
    Gp_InitRec18Table(work->field_22C, 5, 0);
    work->obj20C.flags |= 0x4200;

    work->obj2A4.coord    = actor->field_2C->field_8 + 4;
    work->obj2A4.ctx.recs = work->field_2C4;
    work->obj2A4.pos.vx   = 0;
    work->obj2A4.pos.vy   = -0x3B6;
    work->obj2A4.pos.vz   = 0x1CC;
    work->obj2A4.key      = Gp_PackPair(&Actor02500_D05B30, 0);
    work->obj2A4.radius   = 0x12C;
    work->obj2A4.flags    = 1;
    Gp_LinkObj(3, &work->obj2A4);
    Gp_InitRec18Table(work->field_2C4, 1, 0);
    work->obj2A4.flags &= 0x7FFF;
    actor->field_30     = 1;
}

/// Per-frame collision and damage pass. Carves a `Actor02500MoveScratch` off
/// the scratchpad stack, lets `func_800E0C10` resolve this frame's movement
/// into it, then walks the three `field_1C4` records: kind 2 is a hit that
/// costs the enemy HP and plays a sound, kinds 1 and 3 push it away from the
/// obstacle, and the strongest push is applied to the coordinate at the end.
void Actor02500_Fn00494(Actor02500* actor)
{
    s32                     one;
    Actor02500**            slots;
    u32                     lastId;
    VECTOR*                 normal;
    Actor02500Ctx*          ctx;
    GpRec18*                rec2C4;
    Actor02500Work*         work;
    Actor02500Work*         walk;
    GsCOORDINATE2*          coord;
    Actor02500MoveScratch*  scratchEnd;
    Actor02500MoveScratch*  frame;
    VECTOR*                 frameNormal;
    Actor02500MoveScratch*  frameAlias;
    Actor02500MoveScratch*  frameBase;
    Actor02500MoveScratch** scratchSp;
    s16                     hitCooldown;
    s16                     hp;
    s32                     cooldown;
    s32                     kind;
    s32                     moveResult;
    s32                     dx;
    s32                     dy;
    s32                     dz;
    s32                     ax;
    s32                     ay;
    s32                     az;
    s32                     param0;
    s32                     push;
    s32                     bestPush;
    s32                     damage;
    s32                     soundId;
    s32                     pushClamped;
    s32                     pan;
    s32                     panOther;
    s32                     panHit;
    u32                     recId;
    u32                     id;
    u32                     recKind;
    u32                     paramKind;
    GsCOORDINATE2*          targetCoord;

    bestPush   = 0;
    lastId     = 0;
    work       = actor->field_1C;
    scratchSp  = (Actor02500MoveScratch**)&SCRATCH_SP;
    scratchEnd = *scratchSp;
    SOFT_TOUCH_REG2_USE(scratchEnd, scratchSp, work->field_22C);
    SOFT_TOUCH_REG(scratchSp);
    frameBase  = scratchEnd - 1;
    frameAlias = frameBase;
    SOFT_TOUCH_REG2(frameAlias, scratchSp);
    frame = frameAlias;
    ctx   = actor->field_20;
    coord = actor->field_2C->field_8;
    SOFT_TOUCH_REG_USE2(coord, frameBase, scratchSp);
    SOFT_TOUCH_REG2_USE(frame, scratchSp, coord);
    SOFT_TOUCH_REG(scratchSp);
    *scratchSp      = frame;
    work->field_340 = 0;
    moveResult      = func_800E0C10(work->field_22C, frame, 5, 0);
    one             = 1;
    SOFT_TOUCH_REG(one);
    if (moveResult == one) {
        goto move_delta;
    }
    if (moveResult < 2) {
        goto move_done;
    }
    if (moveResult == 2) {
        goto move_absolute;
    }
    goto move_done;
move_delta:
    coord->coord.t[0] += scratchEnd[-1].vx.p.hi;
    coord->coord.t[1] += frame->vy.p.hi;
    coord->coord.t[2] += frame->vz.p.hi;
    goto move_tail;
move_absolute:
    coord->coord.t[0] = work->field_304;
    coord->coord.t[1] = work->field_308;
    coord->coord.t[2] = work->field_30C;
move_tail:
    if (scratchEnd[-1].vx.v != 0 || frame->vz.v != 0) {
        work->field_340 = one;
    }
move_done:
    Gp_ClearRec18Occupied(work->field_22C);
    if (work->field_334 != 0) {
        hitCooldown     = (u16)work->field_334 - 1;
        work->field_334 = hitCooldown;
        if (hitCooldown <= 0) {
            work->field_334 = 0;
        }
    }
    frameNormal = &frame->normal;
    normal      = frameNormal;
    walk        = work;
    do {
        SOFT_TOUCH_REG2(walk, frame);
        SOFT_TOUCH_REG(frame);
        recId   = walk->field_1C4[0].key;
        recKind = recId >> 0x10;
        switch (recKind) {
            default:
                break;
            case 2:
                if (work->field_334 == 0) {
                    slots = Gp_ActorSlots;
                    SOFT_TOUCH_REG_USE(slots, recId);
                    targetCoord = slots[(recId >> 7) & 1]->field_2C->field_8;
                    ax          = targetCoord->coord.t[0] - coord->coord.t[0];
                    frame->vx.v = ax;
                    ay          = targetCoord->coord.t[1] - coord->coord.t[1];
                    frame->vy.v = ay;
                    az          = targetCoord->coord.t[2] - coord->coord.t[2];
                    frame->vz.v = az;
                    damage      = Gp_ComputeDamage(walk->field_1C4[0].key,
                                                   SquareRoot0((frame->vx.v * frame->vx.v) + (frame->vy.v * frame->vy.v) +
                                                               (frame->vz.v * frame->vz.v)),
                                                   0, 0);
                    param0      = Gp_GetIdParam0(walk->field_1C4[0].key);
                    kind        = param0 & 0xFFFF;
                    if (kind == 5) {
                        damage *= 2;
                        Gp_SpawnEff(0x6009C, coord, 2, NULL);
                    }
                    if (Gp_RollEnemyChance(ctx, walk->field_1C4[0].key, 0) != 0) {
                        damage *= 4;
                        if (kind != 5) {
                            Gp_SpawnEff(0x6009C, coord, 0, NULL);
                        }
                    }
                    func_800E2C78(ctx, walk->field_1C4[0].key, damage, 0);
                    func_800DA6E8(&ctx->node, damage, 0);
                    hp            = (u16)ctx->field_40 - damage;
                    ctx->field_40 = hp;
                    if ((hp << 0x10) <= 0) {
                        work->field_322     = 6;
                        work->field_324     = 0;
                        work->obj2A4.flags &= 0x7FFF;
                        soundId             = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x4019000A;
                        pan                 = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth(coord));
                    } else {
                        if (work->field_342 == 0) {
                            work->field_322 = 2;
                            work->field_324 = 0;
                        }
                        work->field_342 = 0;
                        soundId         = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40190009;
                        panOther        = (s8)Gp_GetObjPan(coord);
                        SndEvt_EnqueueType6(soundId, panOther, (s8)Gp_GetObjDepth(coord));
                    }
                    paramKind = param0 & 0xFFFF;
                    switch (paramKind) {
                        case 0:
                        case 3:
                        case 5:
                        case 7:
                        case 8:
                        case 9:
                            break;
                        case 1:
                            if (work->field_33E == 0) {
                                Gp_SetObjFlag1(ctx);
                            }
                            work->obj2A4.flags &= 0x7FFF;
                            break;
                        case 2:
                            Gp_SetObjFlag2(ctx, walk->field_1C4[0].key, 0);
                            work->obj2A4.flags &= 0x7FFF;
                            break;
                        case 4:
                        case 6:
                            if (ctx->field_40 <= 0) {
                                work->field_33C = 1;
                            }
                            break;
                    }
                    id = walk->field_1C4[0].key;
                    if (lastId != id) {
                        lastId = id;
                        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, 0, &work->field_2DC);
                    }
                    cooldown = Gp_GetIdParam2(walk->field_1C4[0].key);
                    if (cooldown > 0) {
                        work->field_334 = cooldown;
                    }
                }
            case 0:
            rec_done:
                break;
            case 1:
            case 3:
                dx          = coord->workm.t[0] - walk->field_1C4[0].point.vx;
                frame->vx.v = dx;
                dy          = coord->workm.t[1] - walk->field_1C4[0].point.vy;
                frame->vy.v = dy;
                dz          = coord->workm.t[2] - walk->field_1C4[0].point.vz;
                frame->vz.v = dz;
                push        = walk->field_1C4[0].depth -
                       SquareRoot0((frame->vx.v * frame->vx.v) + (frame->vy.v * frame->vy.v) +
                                   (frame->vz.v * frame->vz.v));
                pushClamped = push;
                if (push <= 0) {
                    pushClamped = 0;
                }
                push = pushClamped;
                SOFT_TOUCH_REG(push);
                USE_REG(pushClamped);
                if (bestPush < push) {
                    bestPush = push;
                    VectorNormal((VECTOR*)frame, normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, &frame->dir);
                }
                goto rec_done;
        }
        walk = (Actor02500Work*)((GpRec18*)walk + 1);
    } while ((s32)walk < (s32)((GpRec18*)work + 3));
    if (bestPush > 0) {
        coord->coord.t[0] += (bestPush * frame->dir.vx) >> 0xC;
        coord->coord.t[2] += (bestPush * frame->dir.vz) >> 0xC;
    }
    Gp_ClearRec18Occupied(work->field_1C4);
    rec2C4 = work->field_2C4;
    if (Gp_FindRec18(rec2C4, 0) != 0) {
        work->obj2A4.flags &= 0x7FFF;
        soundId             = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40190006;
        panHit              = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(soundId, panHit, (s8)Gp_GetObjDepth(coord));
    }
    Gp_ClearRec18Occupied(rec2C4);
    if (Gp_CountRec18Hi(work->field_18C, 0x10000) != 0 && work->field_322 == 0) {
        work->field_322 = 1;
        work->field_324 = 0;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_18C);
    SCRATCH_SP += 0x30;
}

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
            work->field_326   = (s16)Actor02500_D05B58[actor->field_20->field_3C->rowIndex];
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
    work->field_328 = (s16)Actor02500_D05B48[actor->field_20->field_3C->rowIndex];
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
            work->field_326   = (s16)Actor02500_D05B78[actor->field_20->field_3C->rowIndex];
            scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vector->vy        = 0;
            vector->vz        = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
                    scratchEnd[-1].vx = Player_Status.coordMtx->t[0] - work->field_314;
                    vector->vy        = 0;
                    homeDz            = Player_Status.coordMtx->t[2] - work->field_318;
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
                work->obj2A4.flags |= 0x8000;
            } else if (frame == 42) {
                sound = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40190005;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(coord));
            } else if (frame == 44) {
                work->field_342     = 0;
                work->obj2A4.flags &= 0x7FFF;
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
    work->field_328 = (s16)Actor02500_D05B68[actor->field_20->field_3C->rowIndex];
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
