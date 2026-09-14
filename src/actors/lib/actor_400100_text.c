#include "common.h"

#include "actors/actor_400100.h"
#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/wipsys.h"
#include <psyq/inline_c.h>

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn01EEC);

void      Actor00100_Fn001FC(GsCOORDINATE2*, s16);
void      Actor00100_Fn01D74(Actor00100*);
s32       Actor00100_Fn01EEC(Actor00100*, Actor00100Work*);
void      Gp_UpdateCoord(GsCOORDINATE2*);
void      func_800B4114(s8*, s32, s16, s32, s32);
extern s8 Actor00100_D1B6D0;

void Actor00100_Fn02788(Actor00100* arg0)
{
    s32             index;
    u32             table;
    Actor00100Work* seekWork;
    Actor00100Work* resetWork;
    Actor00100Work* turnWork;
    Actor00100Work* secondaryWork;
    Actor00100Work* tickWork;
    Actor00100Work* work;
    s32             targetAngle;
    s32             animation;
    s32             updatedTurn;
    s16             currentTurn;
    s16             thirdAngle;
    s16             state;
    s32             currentAngle;
    s16             angle;
    s32             seekSlotIndex;
    s32             resetSlotIndex;
    s32             secondarySlotIndex;
    s32             tickSlotIndex;
    s32             signedTurn;
    s32             soundId;
    s32             sound;
    s32             resetIndex;
    s32             secondaryIndex;
    s32             tickIndex;
    s32             seekIndex;
    s32             delta;
    s8*             tickSlot;
    s8*             seekSlot;
    s8*             resetSlot;
    s8*             secondarySlot;
    s32             pan;
    s32             currentAngleBits;
    u16             originalTurn;
    s32             targetAngleBits;
    u16             updatedTurnBits;
    s32             clampedAngle;
    s32             targetTurn;

    work  = arg0->field_1C;
    state = (s16)work->field_828;
    if (state == 1) {
        if (work->field_82C != (s16)work->field_82E) {
            seekWork = work;
            TOUCH_REG(seekWork);
            seekIndex = 1;
            table     = (u32)&Actor00100_D1B6D0;
            seekSlot  = &work->pad_8[0x20];
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_832;
                animation      = (s16)seekWork->field_82E;
                seekSlot      += 0x28;
                index          = seekWork->field_82C * 0x19;
                func_800B4114(&seekWork->pad_8[0x14], seekSlotIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = (s16)seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(&work->pad_846[2], 0U, 0x48U);
    } else if (state == 2) {
        resetWork = work;
        TOUCH_REG(resetWork);
        resetIndex = 1;
        resetSlot  = &work->pad_8[0x20];
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_832;
            resetSlot      += 0x28;
            Gp_AnimResetSlot((GpAnimCtx*)&resetWork->pad_8[0x14], resetSlotIndex, (s32)(s16)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = (s16)resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(&work->pad_846[2], 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork  = arg0->field_1C;
        secondaryIndex = 1;
        secondarySlot  = &secondaryWork->pad_8[0x20];
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_83A;
            secondarySlot      += 0x28;
            Gp_AnimResetSlot((GpAnimCtx*)&secondaryWork->pad_6A[0x3B6], secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if ((s16)work->field_82A == 0) {
        tickWork  = arg0->field_1C;
        tickIndex = 1;
        tickSlot  = &tickWork->pad_8[0x20];
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_832;
            Gp_AnimTickIndex((GpAnimCtx*)&tickWork->pad_8[0x14], tickSlotIndex);
            tickSlot  += 0x28;
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        Actor00100_Fn01D74(arg0);
        if (work->field_46C & 0x100) {
            work->field_82A = 0;
        }
    }
    targetAngle      = (s16)work->field_840;
    currentAngle     = (s16)work->field_844;
    targetAngleBits  = work->field_840;
    currentAngleBits = work->field_844;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_844 = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_844 = currentAngleBits - 0x71;
    } else {
    block_26:
        work->field_844 = targetAngleBits;
    }
    angle        = (s16)work->field_844;
    clampedAngle = work->field_844;
    if (angle != 0) {
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        Actor00100_Fn001FC(&arg0->field_2C->field_8[2], thirdAngle);
        arg0->field_2C->field_8[2].flg = 0;
        Actor00100_Fn001FC(&arg0->field_2C->field_8[3], thirdAngle);
        arg0->field_2C->field_8[3].flg = 0;
        Actor00100_Fn001FC(&arg0->field_2C->field_8[4], (s16)clampedAngle / 2);
        arg0->field_2C->field_8[4].flg = 0;
    }
    if (((s16)work->field_82E == 0) && (work->field_0 == 0x26)) {
        Gfx_RotMatrixX(&arg0->field_2C->field_8[4].coord, 0x280, 0);
        arg0->field_2C->field_8[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->field_8[4]);
    }
    turnWork     = arg0->field_1C;
    targetTurn   = turnWork->field_83E;
    originalTurn = targetTurn;
    if ((s16)targetTurn >= 0x201) {
        targetTurn = 0x200;
    }
    if ((s16)originalTurn < -0x200) {
        targetTurn = -0x200;
    }
    signedTurn  = (s16)targetTurn;
    currentTurn = turnWork->field_842;
    if (currentTurn < signedTurn) {
        if ((signedTurn - currentTurn) >= 0xD) {
            turnWork->field_842 = (s16)((u16)turnWork->field_842 + 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    updatedTurn     = turnWork->field_842;
    updatedTurnBits = (u16)turnWork->field_842;
    if ((s16)targetTurn < updatedTurn) {
        delta = updatedTurn - (s16)targetTurn;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 0xD) {
            turnWork->field_842 = (s16)(updatedTurnBits - 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    Actor00100_Fn001FC(&arg0->field_2C->field_8[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    arg0->field_2C->field_8[10].flg = 0;
    sound                           = Actor00100_Fn01EEC(arg0, work);
    if (sound != 0) {
        soundId = sound | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
        pan     = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(soundId, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn02C54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn03340);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0375C);

#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

extern GsCOORDINATE2 Gfx_ViewCoord;
extern s8            D_80114C12;
extern u8            D_80071075;

struct _GpObj20E;
void Gp_ReleaseStateF0Add(struct _GpObj20E* arg0, s32 arg1);
s32  Gp_DispatchMsg(void* arg0, s32 arg1, s32 arg2, s32 arg3);

void Actor00100_Fn04270(Actor00100* argx)
{
    register Actor00100*    arg0 asm("s2");
    Actor00100Work*         work;
    TmdObject*              obj;
    Actor00100Ctx*          ctx;
    Actor00100MtxScratch*   blk;
    GsCOORDINATE2*          coords;
    GsCOORDINATE2*          p;
    GsCOORDINATE2*          view0;
    register GsCOORDINATE2* view1 asm("s0");
    SVECTOR*                out;
    SVECTOR*                svp;
    VECTOR*                 vecp;
    s32*                    fp;
    SVECTOR                 sv;
    VECTOR                  vec;
    s32                     flag0;
    s32                     flag1;
    s32                     state;
    s32                     st;
    s32                     d;
    s32                     eff;
    s32                     v;
    s16                     ang;
    u16                     next;

    arg0 = argx;
    work = arg0->field_1C;
    obj  = arg0->field_2C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C         = 0;
        work->objs[2].flags &= 0xBFFF;
        ctx->field_14        = 1;
        work->field_6        = 0;
    }
    if (work->field_6 == 0x3C) {
        Gp_UnlinkObj((GpObj*)&work->objs[0]);
        Gp_UnlinkObj((GpObj*)&work->objs[1]);
        Gp_UnlinkObj((GpObj*)&work->objs[3]);
        Gp_UnlinkObj((GpObj*)&work->objs[2]);
        ctx->field_54 = 0;
    }
    if (work->field_6 >= 0x3D && work->field_C18 == 0 && D_80114C12 != 1 && D_80071075 == 0) {
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x4010000) {
            Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, ctx->field_8 >> 12, 0);
        }
        arg0->field_30++;
        return;
    }
    next          = (u16)work->field_6 + 1;
    work->field_6 = next;
    state         = (s16)next;
    switch (state) {
        case 1:
            Gp_SetLightMode((GpObj4C*)ctx, 0);
            Gp_SetLightMode((GpObj4C*)ctx, 1);
            /* fallthrough */
        case 0xA:
            arg0->field_2C->field_C = 2;
            Gp_SetLightMode((GpObj4C*)ctx, 2);
            break;
        case 0xF:
            work->field_8A8.vx = 0;
            work->field_8A8.vy = 0;
            work->field_8A8.vz = 0;
            work->field_8B0.vx = 0;
            work->field_8B0.vy = 0;
            work->field_8B0.vz = 0;
            view0              = &Gfx_ViewCoord;
            svp                = &sv;
            vecp               = &vec;
            fp                 = &flag0;
            out                = &work->field_8A8;
            p                  = &arg0->field_2C->field_8[2];
            sv.vx              = work->field_8A8.vx;
            sv.vy              = out->vy;
            sv.vz              = out->vz;
        loop0:
            if (p->sub == NULL) {
                goto done0;
            }
            {
                if (p == view0) {
                    out->vx = sv.vx;
                    out->vy = sv.vy;
                    out->vz = sv.vz;
                    goto done0;
                }
                gte_SetTransMatrix(&p->coord);
                gte_SetRotMatrix(&p->coord);
                gte_ldv0(svp);
                gte_rtv0tr_real();
                gte_stlvnl(vecp);
                gte_stflg(fp);
                sv.vx = vec.vx;
                sv.vy = vec.vy;
                sv.vz = vec.vz;
                p     = p->sub;
                goto loop0;
            }
        done0:
            view1 = &Gfx_ViewCoord;
            Gp_SpawnEff(0x600A5, view1, 2, &work->field_8A8);
            work->field_8A8.vy = *(u16*)&arg0->field_2C->field_8[0].coord.t[1];
            svp                = &sv;
            vecp               = &vec;
            fp                 = &flag1;
            out                = &work->field_8B0;
            p                  = &arg0->field_2C->field_8[9];
            sv.vx              = work->field_8B0.vx;
            sv.vy              = out->vy;
            sv.vz              = out->vz;
        loop1:
            if (p->sub == NULL) {
                goto done1;
            }
            {
                if (p == view1) {
                    out->vx = sv.vx;
                    out->vy = sv.vy;
                    out->vz = sv.vz;
                    goto done1;
                }
                gte_SetTransMatrix(&p->coord);
                gte_SetRotMatrix(&p->coord);
                gte_ldv0(svp);
                gte_rtv0tr_real();
                gte_stlvnl(vecp);
                gte_stflg(fp);
                sv.vx = vec.vx;
                sv.vy = vec.vy;
                sv.vz = vec.vz;
                p     = p->sub;
                goto loop1;
            }
        done1:
            work->field_8B0.vy = *(u16*)&arg0->field_2C->field_8[0].coord.t[1];
            Gp_SpawnEff(0x600A5, &Gfx_ViewCoord, 2, &work->field_8B0);
            break;
        case 0x3C:
            arg0->field_2C->field_C = 0x80;
            break;
    }

    st = work->field_6;
    if (st < 0xB) {
        return;
    }
    d = st - 0xA;
    v = d * 107;
    if (v < 0x1000) {
        register u8* h asm("s4");
        register s32 sy asm("s2");
        register s32 k1000 asm("s3");
        TmdObject*   o;

        o      = arg0->field_2C;
        k1000  = 0x1000;
        sy     = k1000 - v;
        h      = PSX_SCRATCH;
        h      = *(u8**)(h + 0x3FC);
        coords = o->field_8;
        blk    = (Actor00100MtxScratch*)(h - 0x34);

        *(Actor00100MtxScratch**)G_SCRATCH_HEAD = blk;

        ang        = ratan2(-coords[0].coord.m[2][0], coords[0].coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = k1000;
        blk->scale.vy = (s16)sy;
        blk->scale.vz = k1000;
        ScaleMatrix(&blk->m, &blk->scale);
        coords[0].coord.m[0][0] = *(u16*)&((Actor00100MtxScratch*)(h - 0x34))->m.m[0][0];
    } else {
        register u8* h2 asm("s2");
        TmdObject*   o2;

        o2     = arg0->field_2C;
        h2     = PSX_SCRATCH;
        h2     = *(u8**)(h2 + 0x3FC);
        coords = o2->field_8;
        blk    = (Actor00100MtxScratch*)(h2 - 0x34);

        *(Actor00100MtxScratch**)G_SCRATCH_HEAD = blk;

        ang        = ratan2(-coords[0].coord.m[2][0], coords[0].coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = 0x1000;
        blk->scale.vy = 0;
        blk->scale.vz = 0x1000;
        ScaleMatrix(&blk->m, &blk->scale);
        coords[0].coord.m[0][0] = *(u16*)&((Actor00100MtxScratch*)(h2 - 0x34))->m.m[0][0];
    }
    coords[0].coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coords[0].coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coords[0].coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coords[0].coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coords[0].coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coords[0].coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coords[0].coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    {
        register u8* h3;
        u8*          top;
        u16          m22;

        h3                      = PSX_SCRATCH;
        top                     = *(u8**)(h3 + 0x3FC);
        m22                     = *(u16*)&blk->m.m[2][2];
        coords[0].flg           = 0;
        *(u8**)G_SCRATCH_HEAD   = top + 0x34;
        coords[0].coord.m[2][2] = m22;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn04864);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0503C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn061FC);

void Actor00100_Fn06398(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    SVECTOR*        vec;
    SVECTOR*        head;
    TmdObject*      obj;
    s32             x;
    s32             z;
    SVECTOR*        gteVec;
    s32             sound;
    s32             pan;
    s32             eventPan;
    u16             tick;
    Task*           player;

    work                       = arg0->field_1C;
    player                     = Game_GetPtrSlot(3);
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 2;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    ctx                        = arg0->field_20;
    gteVec                     = vec;
    if (work->field_4 != 0) {
        TOUCH_REG(gteVec);
        obj             = arg0->field_2C;
        ctx->field_14   = 0;
        work->field_BE4 = 0;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_82E        = 5;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, vec);
        work->field_C28 = 0;
        work->field_6   = 0;
        VectorNormalSS(vec, vec);
        if (work->field_C1A >= 0xFA1) {
            work->field_C1A = 0xFA0;
        }
        gte_lddp(0x85);
        gte_ldsv(gteVec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(gteVec);
        x               = head[-2].vx;
        work->field_8DC = 0;
        work->field_8D8 = x;
        z               = vec->vz;
        work->field_8E8 = 7;
        work->field_8EA = 1;
        work->field_8E0 = z;
        pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(7, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        Gp_SpawnPadLerp(8, 0xFFU, 8U);
    }
    tick          = work->field_6 + 1;
    work->field_6 = tick;
    if (((s16)tick == 0xF) && (work->field_8E8 == 7)) {
        sound    = (((u16)ctx->field_8 >> 0xC) << 8) | 0x4001000A;
        eventPan = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, (s32)eventPan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        if (Gp_State1C->field_A == 2) {
            Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->field_8 + 1, 0x80003A00, NULL);
        }
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1F;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn06654);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn06C10);

void Actor00100_Fn070DC(Actor00100* arg0)
{
    Actor00100Work*         work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          coord2;
    GsCOORDINATE2*          facing;
    GsCOORDINATE2*          facing2;
    TmdObject*              obj;
    s16                     delta;
    s32                     playerX;
    s16                     delta2;
    s16                     z;
    s16                     targetYaw;
    s16                     wrapped;
    s16                     wrappedYaw;
    s16                     wrapped2;
    s32                     angle;
    s32                     angle2;
    s32                     finalDelta;
    s32                     firstDelta;
    s32                     magnitude;
    Actor00100AngleScratch* head;
    Actor00100AngleScratch* scratch;

    head    = *(Actor00100AngleScratch**)G_SCRATCH_HEAD;
    work    = arg0->field_1C;
    scratch = (*(Actor00100AngleScratch**)G_SCRATCH_HEAD = head - 1);
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 0;
        obj->field_C             = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 2;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        work->field_6 = 0;
    }
    Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5);
    arg0->field_2C->field_8->flg = 0;
    coord                        = arg0->field_2C->field_8;
    head[-1].x                   = (s16)(Wip_SysConfig.field_4->t[0] - coord->coord.t[0]);
    scratch->y                   = (s16)(Wip_SysConfig.field_4->t[1] - coord->coord.t[1]);
    scratch->z                   = (s16)(Wip_SysConfig.field_4->t[2] - coord->coord.t[2]);
    arg0->field_2C->field_8->flg = 0;
    Actor00100_Fn02788(arg0);
    facing  = arg0->field_2C->field_8;
    angle   = ratan2((s32)head[-1].x, (s32)scratch->z);
    delta   = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped = delta;

    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    firstDelta         = wrapped;
    scratch->delta     = firstDelta;
    work->field_840    = firstDelta;
    playerX            = -((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][0];
    scratch->yaw       = ratan2((s32)playerX, (s32)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][2]);
    coord2             = arg0->field_2C->field_8;
    scratch->x         = (s16)(Wip_SysConfig.field_4->t[0] - coord2->coord.t[0]);
    scratch->y         = (s16)(Wip_SysConfig.field_4->t[1] - coord2->coord.t[1]);
    z                  = Wip_SysConfig.field_4->t[2] - coord2->coord.t[2];
    scratch->z         = z;
    targetYaw          = ratan2((s32)scratch->x, (s32)z) + 0x800;
    wrappedYaw         = targetYaw;
    scratch->targetYaw = targetYaw;

    if (targetYaw < 0) {
    wrapYawNegative:
        if (wrappedYaw < -0x800) {
            wrappedYaw += 0x1000;
            goto wrapYawNegative;
        }
    } else {
    wrapYawPositive:
        if (wrappedYaw >= 0x801) {
            wrappedYaw -= 0x1000;
            goto wrapYawPositive;
        }
    }
    scratch->targetYaw = wrappedYaw;
    facing2            = arg0->field_2C->field_8;
    angle2             = ratan2((s32)scratch->x, (s32)scratch->z);
    delta2             = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    wrapped2           = delta2;

    if (delta2 < 0) {
    wrapFinalNegative:
        if (wrapped2 < -0x800) {
            wrapped2 += 0x1000;
            goto wrapFinalNegative;
        }
    } else {
    wrapFinalPositive:
        if (wrapped2 >= 0x801) {
            wrapped2 -= 0x1000;
            goto wrapFinalPositive;
        }
    }
    finalDelta      = wrapped2;
    scratch->delta  = (s16)finalDelta;
    work->field_840 = (s16)finalDelta;
    magnitude       = abs(scratch->targetYaw - scratch->yaw);
    if (magnitude >= 0x601) {
        Gp_ArmStateF0(1);
        work->field_0 = 0x1C;
    }
    *(Actor00100AngleScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0747C);

void Actor00100_Fn07650(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;
    s32             sound;
    s32             sound2;
    s32             pan;
    s32             pan2;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj           = arg0->field_2C;
        ctx->field_14 = 0;
        obj->field_C  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_82E        = 0xA;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        sound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40010009;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        ctx->field_40 = (u16)(ctx->field_40 - 0xF);
        func_800DA6E8(ctx->field_10, 0xF, 0);
        if ((s16)ctx->field_40 <= 0) {
            ctx->field_40 = 1U;
        } else {
            sound2 = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40010007;
            pan2   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
            SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        }
    }
    Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5);
    arg0->field_2C->field_8->flg = 0;
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        if ((s16)ctx->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (ctx->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0782C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn08588);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn08A14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn08E7C);

void Actor00100_Fn09310(Actor00100* arg0)
{
    SVECTOR         vector;
    GpEffWork*      effect;
    GpEffWork*      effect2;
    GpEffWork*      effect3;
    GpEffWork*      effect4;
    Task*           task;
    Task*           task2;
    Task*           task3;
    Task*           task4;
    TmdObject*      obj;
    u16             next;
    TmdObject*      effectObj;
    TmdObject*      effectObj2;
    TmdObject*      effectObj3;
    TmdObject*      effectObj4;
    Actor00100Work* work;
    Actor00100Ctx*  ctx;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    obj  = arg0->field_2C;
    if (work->field_4 != 0) {
        work->field_BE4        = 0;
        obj->field_C           = 0x80;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags    = (u16)(work->objs[2].flags & 0xBFFF);
        ctx->field_14          = 1;
        work->field_844        = 0;
        work->field_840        = 0;
        work->field_83E        = 0;
        work->field_6          = 0U;
        vector.vx              = 0x64;
        vector.vz              = 0;
        vector.vy              = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 2) {
        obj->field_C |= 4;
        Tmd_FreeBuffers(obj);
        D_80114B78[0] = &Actor00100_D10D60;
        vector.vz     = 0x64;
        vector.vy     = 0;
        vector.vx     = 0;
        effect        = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[9], 0x200, &vector);
        if (effect != NULL) {
            task                                = effect->field_0;
            ((TmdObject*)task->extra)->field_24 = (u8)arg0->field_2C->field_24;
            ((TmdObject*)task->extra)->field_25 = (u8)arg0->field_2C->field_25;
            effectObj                           = task->extra;
            if (effectObj->field_18 != 0) {
                Tmd_ProcessStream((TmdObject*)effectObj);
                Tmd_ProcessStream((TmdObject*)task->extra);
            }
        }
        if ((s16)work->field_6 == 2) {
            D_80114B78[0] = &Actor00100_D11234;
            vector.vy     = 0;
            vector.vx     = 0;
            effect2       = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[12], 0x200, &vector);
            if (effect2 != NULL) {
                task2                                = effect2->field_0;
                ((TmdObject*)task2->extra)->field_24 = (u8)arg0->field_2C->field_24;
                ((TmdObject*)task2->extra)->field_25 = (u8)arg0->field_2C->field_25;
                effectObj2                           = task2->extra;
                if (effectObj2->field_18 != 0) {
                    Tmd_ProcessStream((TmdObject*)effectObj2);
                    Tmd_ProcessStream((TmdObject*)task2->extra);
                }
            }
        }
    }
    if ((s16)work->field_6 == 4) {
        D_80114B78[0] = &Actor00100_D12470;
        effect3       = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[1], 0x200, NULL);
        if (effect3 != NULL) {
            task3                                = effect3->field_0;
            ((TmdObject*)task3->extra)->field_24 = (u8)arg0->field_2C->field_24;
            ((TmdObject*)task3->extra)->field_25 = (u8)arg0->field_2C->field_25;
            effectObj3                           = task3->extra;
            if (effectObj3->field_18 != 0) {
                Tmd_ProcessStream((TmdObject*)effectObj3);
                Tmd_ProcessStream((TmdObject*)task3->extra);
            }
        }
    }
    if ((s16)work->field_6 == 5) {
        D_80114B78[0] = &Actor00100_D11F90;
        effect4       = Gp_SpawnEff(0xA0000 | 5, &arg0->field_2C->field_8[3], 0x200, NULL);
        if (effect4 != NULL) {
            task4                                = effect4->field_0;
            ((TmdObject*)task4->extra)->field_24 = (u8)arg0->field_2C->field_24;
            ((TmdObject*)task4->extra)->field_25 = (u8)arg0->field_2C->field_25;
            effectObj4                           = task4->extra;
            if (effectObj4->field_18 != 0) {
                Tmd_ProcessStream((TmdObject*)effectObj4);
                Tmd_ProcessStream((TmdObject*)task4->extra);
            }
        }
    }
    if ((s16)work->field_6 == 0x1E) {
        ctx->field_40 = 0;
        Gp_UnlinkObj((GpObj*)&work->objs[0]);
        Gp_UnlinkObj((GpObj*)&work->objs[1]);
        Gp_UnlinkObj((GpObj*)&work->objs[3]);
        Gp_UnlinkObj((GpObj*)&work->objs[2]);
        ctx->field_54 = 0;
    }
    if (((s16)work->field_6 >= 0x1F) && (work->field_C18 == 0) && (D_80114C12 != 1) && (D_80071075 == 0)) {
        if ((*(u32*)&Game_Session->field_4 & 0xFFFF0000) == 0x04010000) {
            Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, (s32)((u16)ctx->field_8 >> 0xC), 0);
        }
        arg0->field_30++;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn09724);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn09CCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0A288);

void Actor00100_Fn0B134(void)
{
}

s16 Actor00100_Fn0B13C(Actor00100* arg0)
{
    Actor00100RecordWork* work  = (Actor00100RecordWork*)arg0->field_1C;
    s16                   found = 0;
    s16                   i;
    s32                   value;

    for (i = 0; i < 5; i++) {
        value = work->records[i].field_0;
        if (value == 0) {
            break;
        }
        if ((value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

s32 Actor00100_Fn0B1A4(Actor00100* arg0, s32 arg1, s32 arg2)
{
    TmdObject*      obj  = arg0->field_2C;
    Actor00100Work* work = arg0->field_1C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->field_C  = 0;
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

s32 Actor00100_Fn0B264(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->field_40 > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->field_C;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

s32 Actor00100_Fn0B2B4(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*            coord;
    s32                       mx;
    s32                       mz;
    ActorsShared80169f74Work* work;

    work                                           = (ActorsShared80169f74Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    coord                                   = ((TmdObject*)task->extra)->field_8;
    mx                                      = coord->coord.m[2][0];
    mz                                      = coord->coord.m[2][2];
    work->yaw                               = ratan2(-mx, mz);
    return 1;
}

void Actor00100_Fn0B3B4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

void Actor00100_Fn0B3DC(Actor00100* arg0, s16 arg1, s16 arg2)
{
    Actor00100Work* work = arg0->field_1C;
    s32             spawn;

    switch (arg1) {
        case 0:
        case 1:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0;
            break;
        case 9:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x2BC;
            break;
        case 7:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (Gp_State1C->field_A == 2 && spawn == 1) {
        Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[arg1], arg2 | 0x80000000, &work->field_898);
    }
}

void Actor00100_Fn0B4D8(Actor00100* arg0)
{
    TmdObject*      obj;
    Actor00100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        arg0->field_20->field_14 = 1;
        obj->field_C            |= 0x80;
        work->objs[2].flags     &= 0xBFFF;
    }
}

void Actor00100_Fn0B52C(Actor00100* arg0)
{
    Actor00100Work* work;
    Actor00100Ctx*  ctx;
    TmdObject*      obj;
    s32             value;
    u32             magnitude;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj           = arg0->field_2C;
        ctx->field_14 = 0;
        obj->field_C  = 0;
        Tmd_AllocBuffers(obj);
        work->field_82E      = 0x15;
        work->field_828      = 2;
        work->field_832      = 0x10;
        work->objs[2].flags |= 0x4000;
        do {
            Actor00100_Fn02788(arg0);
        } while ((work->field_5A & 0x3FF) != 0xC);
        work->field_832 = 0x20;
        return;
    }
    arg0->field_2C->field_8->flg = 0;
    value                        = (s16)work->field_832 / 2;
    work->field_832              = (u16)value;
    magnitude                    = 0x10U;
    if (value == 1) {
        work->field_832 = -magnitude;
    }
    if ((s16)work->field_832 == -1) {
        work->field_832 = 0x10;
    }
    Actor00100_Fn02788(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)ctx) == 1) {
        ctx->field_4C &= 0xFD;
        work->field_0  = 0x24;
    }
}

void Actor00100_Fn0B658(Actor00100* arg0)
{
    TmdObject*      obj;
    Actor00100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 0;
        obj->field_C             = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 8;
        work->field_82A        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        Gp_ArmStateF0(1);
    }
    Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5);
    arg0->field_2C->field_8->flg = 0;
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
}

void Actor00100_Fn0B730(Actor00100* arg0)
{
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 0;
        obj->field_C             = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 0xC;
        work->field_82A        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x26;
    }
}

void Actor00100_Fn0B7DC(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->field_C           = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 1;
        work->field_82E        = 0xA;
        work->field_844        = 0;
        work->field_840        = 0;
        work->field_83E        = 0;
        work->field_832        = work->field_834;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if ((work->field_68 & 0x100) && ((s16)work->field_82E == 0xA)) {
        if ((s16)ctx->field_40 > 0) {
            if (ctx->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0B8D8(Actor00100* arg0)
{
    Actor00100Work* work;
    u16             timer;
    u32             random;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        random        = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState   = random;
        work->field_6 = work->field_C20 + ((random >> 0x10) & 0xF);
    }
    Actor00100_Fn02788(arg0);
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if ((s16)timer < 0) {
        if ((s16)arg0->field_20->field_40 > 0) {
            work->field_0 = 0x24;
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0B98C(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->field_C           = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 1;
        work->field_82E        = 0x10;
        work->field_832        = 0x10;
        work->field_840        = 0;
        work->field_83E        = 0;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        if ((s16)ctx->field_40 > 0) {
            if (ctx->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0BA70(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    TmdObject*      obj;
    Actor00100Work* work;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->field_C           = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 1;
        work->field_828        = 1;
        work->field_82E        = 0x13;
        work->field_832        = 0x10;
        work->field_840        = 0;
        work->field_83E        = 0;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x15;
    }
}

void Actor00100_Fn0BB2C(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->field_C           = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 2;
        work->field_82E        = 0x14;
        work->field_832        = 0x10;
        work->field_840        = 0;
        work->field_83E        = 0;
        if ((s16)ctx->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        if ((s16)ctx->field_40 > 0) {
            if (ctx->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

void Actor00100_Fn0BC14(void)
{
}

void Actor00100_Fn0BC1C(Actor00100* arg0)
{
    GsCOORDINATE2*  coord;
    s32             y;
    s32             distance;
    register s32    slotY asm("v0");
    Actor00100Work* work;
    Task*           task;

    work = arg0->field_1C;
    task = Game_GetPtrSlot(3);
    if ((task != NULL) && (work->field_8E8 == 7)) {
        coord    = ((TmdObject*)task->extra)->field_8;
        slotY    = coord->coord.t[1];
        y        = arg0->field_2C->field_8->coord.t[1];
        distance = slotY - y;
        if (distance < 0) {
            distance = -distance;
        }
        if (distance >= 0x321) {
            coord->coord.t[1]                       = y;
            ((TmdObject*)task->extra)->field_8->flg = 0;
        }
    }
}

void Actor00100_Fn0BCBC(s32 arg0, Task* task)
{
    Actor00100Work* work;

    work = (Actor00100Work*)task->idMap;
    if (work->field_C2A == 1) {
        work->field_C2A = 0;
        Gp_ReleaseStateF0Add((struct _GpObj20E*)task, 1);
    }
    if (work->field_C2A == 0) {
        task->state++;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0BD28);
