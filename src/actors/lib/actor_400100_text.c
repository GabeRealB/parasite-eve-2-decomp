#include "common.h"

#include "actors/actor_400100.h"
#include "actors/actor_400100_damage.h"
#include "actors/actor_400100_motion.h"
#include "actors/actor_400100_update.h"
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

void                         Actor00100_Fn001FC(GsCOORDINATE2*, s16);
void                         Actor00100_Fn01D74(Actor00100*);
s32                          Actor00100_Fn01EEC(Actor00100*, Actor00100Work*);
void                         Gp_UpdateCoord(GsCOORDINATE2*);
void                         func_800B4114(s8*, s32, s16, s32, s32);
extern s8                    Actor00100_D1B6D0;
extern GpEnemyTaskFuncTable4 Actor00100_D001A0;

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

void Actor00100_Fn061FC(Actor00100* arg0)
{
    s32             radius;
    SVECTOR         delta;
    Actor00100Work* work;
    TmdObject*      obj;
    s32             outside;
    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 0;
        obj->field_C             = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 4;
        work->field_82A        = 0;
        work->field_BE4        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        return;
    }
    radius = 1000;
    Actor00100_Fn02788(arg0);
    Actor00100_PositionDelta(arg0->field_2C->field_8, &delta);
    if (work->field_68 & 0x100) {
        outside       = Actor00100_OutsideRadius(&delta, radius);
        work->field_0 = outside == 0 ? 0x1F : 0x26;
    }
}

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

void Actor00100_Fn06C10(Actor00100* arg0)
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
        work->field_82E        = 6;
        work->field_82A        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        work->field_6                  = 0;
        work->state.field_8            = 0;
        work->objs[3].field_20.field_C = -0x2D0;
    }
    work->field_6 += 1;
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x26;
    }
    if (((u32)((work->field_5A & 0x3FF) - 6) < 8U) && (work->state.field_8 < 5)) {
        if (Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5) != 0) {
            work->state.field_8 = (s16)((u16)work->state.field_8 + 1);
        }
        switch (work->field_5A & 0x3FF) {
            case 12:
                Actor00100_MoveForward(arg0->field_2C->field_8, -60);
                break;
            case 13:
                Actor00100_MoveForward(arg0->field_2C->field_8, -30);
                break;
            case 14:
                Actor00100_MoveForward(arg0->field_2C->field_8, -15);
                break;
            default:
                if (Actor00100_HasRecord10(arg0)) {
                    Actor00100_MoveForward(arg0->field_2C->field_8, -85);
                } else {
                    Actor00100_MoveForward(arg0->field_2C->field_8, -120);
                }
                break;
        }
    } else {
        Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5);
    }
    arg0->field_2C->field_8->flg = 0;
}

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

void Actor00100_Fn0747C(Actor00100* arg0)
{
    SVECTOR         delta;
    Actor00100Work* work;
    TmdObject*      obj;
    s32             radius;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg0->field_2C;
        arg0->field_20->field_14 = 0;
        obj->field_C             = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 2;
        work->field_82E        = 7;
        work->field_82A        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
    }
    radius = 2000;
    Actor00100_Fn02788(arg0);
    if (((s16)Actor00100_Fn00508(arg0->field_2C->field_8, &work->objs[0].field_20, 5, &delta) != 0) || ((s16)Actor00100_Fn00508(arg0->field_2C->field_8, &work->objs[1].field_20, 5, &delta) != 0)) {
        work->field_0 = 0x22;
    }
    Actor00100_PositionDelta(arg0->field_2C->field_8, &delta);
    if (Actor00100_OutsideRadius(&delta, radius) == 0) {
        work->field_0 = 0x22;
    }
    arg0->field_2C->field_8->flg = 0;
}

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

void Actor00100_Fn08E7C(Actor00100* arg0)
{
    Actor00100Work*           work;
    GsCOORDINATE2*            coord;
    GsCOORDINATE2*            coord2;
    GsCOORDINATE2*            facing;
    GsCOORDINATE2*            facing2;
    TmdObject*                obj;
    s16                       delta;
    s32                       playerX;
    s16                       delta2;
    s16                       z;
    s16                       targetYaw;
    s16                       wrapped;
    s16                       wrappedYaw;
    s16                       wrapped2;
    s32                       angle;
    s32                       angle2;
    s32                       finalDelta;
    s32                       firstDelta;
    Actor00100ProjectScratch* head;
    Actor00100ProjectScratch* scratch;

    head    = *(Actor00100ProjectScratch**)G_SCRATCH_HEAD;
    work    = arg0->field_1C;
    scratch = (*(Actor00100ProjectScratch**)G_SCRATCH_HEAD = head - 1);
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
    Gp_UpdateCoord(arg0->field_2C->field_8);
    gte_SetTransMatrix(&arg0->field_2C->field_8->workm);
    gte_SetRotMatrix(&arg0->field_2C->field_8->workm);
    head[-1].x = 0;
    scratch->y = 0;
    scratch->z = 0;
    gte_ldv0(scratch);
    __asm__ volatile("nop; nop; .word 0x4A180001");
    gte_stsxy(&head[-1].screenX);
    gte_stdp(&head[-1].dp);
    gte_stflg(&head[-1].flag);
    gte_stszotz(&head[-1].depth);
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
    if (abs(scratch->screenX) < 0x78 && abs(scratch->screenY) < 0x64 && abs(scratch->delta) < 0x200) {
        Gp_ArmStateF0(1);
        work->field_0 = 0x1C;
    }
    *(Actor00100ProjectScratch**)G_SCRATCH_HEAD += 1;
}

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

void Actor00100_Fn09CCC(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    GsCOORDINATE2*  coord;
    TmdObject*      obj;
    TmdObject*      hiddenObj;
    TmdObject*      finishedObj;
    s32             timer;
    s32             y;
    s32             sound;
    s32             sound2;
    s32             depth;
    s32             pan2;
    s32             pan;

    work                   = arg0->field_1C;
    *(s32*)G_SCRATCH_HEAD -= 0x14;
    ctx                    = arg0->field_20;
    if (work->field_4 != 0) {
        obj           = arg0->field_2C;
        ctx->field_14 = 1;
        obj->field_C  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 3;
        work->field_82A        = 0;
        work->field_6          = 0;
        work->field_840        = 0;
        work->field_844        = 0;
        work->field_83E        = 0;
        work->field_842        = 0;
        work->objs[2].flags   &= 0xBFFF;
        work->field_832        = work->field_834;
    }
    work->field_6 += 1;
    Actor00100_Fn02788(arg0);
    switch ((s16)work->field_82E) {
        case 3:
            Actor00100_MoveForwardNonzero(arg0->field_2C->field_8, ((s16)work->field_834 * 1000) / 192);
            arg0->field_2C->field_8->flg = 0;
            if ((s16)work->field_6 >= 0xD) {
                work->field_82E = 0xE;
                work->field_828 = 1;
                work->field_6   = 0;
            }
            break;
        case 14:
            Actor00100_MoveForwardNonzero(arg0->field_2C->field_8, ((s16)work->field_834 * 1300) / 192);
            timer = (s16)work->field_6;
            if (timer == 0xF) {
                if ((Gp_GetViewIndex() & 0xFF) == 8) {
                    sound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x54010005;
                    pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                    depth = Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8);
                    SndEvt_EnqueueType6(sound, (s8)pan, (s8)(depth + abs(Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8)) / 2));
                } else {
                    sound2 = (((u16)ctx->field_8 >> 0xC) << 8) | 0x54010005;
                    pan2   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                    SndEvt_EnqueueType6(sound2, (s8)pan2, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
                }
                timer = (s16)work->field_6;
            }
            if (timer >= 4) {
                coord = arg0->field_2C->field_8;
                y     = coord->coord.t[1];
                if (y < 0x2EE0) {
                    coord->coord.t[1] = y + ((timer - 3) * 0x21);
                }
            }
            if ((s16)work->field_6 == 0x64) {
                Gp_UnlinkObj(&work->objs[0]);
                Gp_UnlinkObj(&work->objs[1]);
                Gp_UnlinkObj(&work->objs[3]);
                Gp_UnlinkObj(&work->objs[2]);
                ctx->field_54       = 0;
                ctx->field_40       = 0;
                hiddenObj           = arg0->field_2C;
                hiddenObj->field_C |= 0x80;
            }
            if ((s16)work->field_6 == 0x65) {
                finishedObj           = arg0->field_2C;
                finishedObj->field_C |= 4;
            }
            if (((s16)work->field_6 >= 0x79) && (work->field_C18 != 1) && (D_80114C12 != 1) && D_80071075 == 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, (s32)((u16)ctx->field_8 >> 0xC), 0);
                work->field_C2A = 1;
                arg0->field_30++;
            }

            break;
    }
    *(s32*)G_SCRATCH_HEAD += 0x14;
}
