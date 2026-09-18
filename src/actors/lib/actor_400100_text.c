#include "common.h"

#include "actors/actor_400100.h"
#include "actors/actor_400100_damage.h"
#include "actors/actor_400100_facing.h"
#include "actors/actor_400100_motion.h"
#include "actors/actor_400100_move.h"
#include "actors/actor_400100_patrol.h"
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
        if ((*(u32*)&gGameSession->field_4 & 0xFFFF0000) == 0x4010000) {
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

void Actor00100_Fn04864(Actor00100* arg0)
{
    Actor00100Work*          work;
    Actor00100Ctx*           ctx;
    Actor00100MoveWork*      move;
    Actor00100PatrolScratch* head;
    Actor00100PatrolScratch* scratch;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           playerCoord;
    GsCOORDINATE2*           turnCoord;
    GpRec18*                 records;
    u16                      angle;
    s16                      delta;
    s32                      value;
    s32                      magnitude;
    s16                      yaw;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj           = arg0->field_2C;
        ctx->field_14 = 0;
        obj->field_C  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_82E        = 0;
        work->field_83E        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        Actor00100_Fn02788(arg0);
        work->field_6                  = 0;
        work->objs[3].field_20.field_C = 0x26C;
        return;
    }
    head            = *(Actor00100PatrolScratch**)G_SCRATCH_HEAD;
    scratch         = (*(Actor00100PatrolScratch**)G_SCRATCH_HEAD = head - 1);
    move            = (Actor00100MoveWork*)work;
    head[-1].vec.vx = move->pos[move->index][0] - arg0->field_2C->field_8->coord.t[0];
    scratch->vec.vy = 0;
    scratch->vec.vz = move->pos[move->index][1] - arg0->field_2C->field_8->coord.t[2];
    if (!Actor00100_OutsideRadius(&scratch->vec, 0xA0) || work->field_6 >= 0x15) {
        if (move->index == 0)
            move->index = 1;
        else
            move->index = 0;
        work->field_6 = 0;
    }
    Actor00100_Fn02788(arg0);
    coord           = arg0->field_2C->field_8;
    angle           = ratan2(scratch->vec.vx, scratch->vec.vz);
    delta           = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    value           = Actor00100_NormalizeYaw(delta);
    scratch->yaw    = value;
    work->field_840 = value;
    if (scratch->yaw >= 0x11)
        scratch->yaw = 0x10;
    if (scratch->yaw < -0x10)
        scratch->yaw = -0x10;
    work->field_83E = scratch->yaw;
    turnCoord       = arg0->field_2C->field_8;
    yaw             = (u16)scratch->yaw + ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]);
    scratch->yaw    = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, yaw, 1);
    records = &work->objs[0].field_20;
    if ((s16)work->field_82A == 0) {
        if (Actor00100_HasRecord10(arg0)) {
            Actor00100_MoveForward(arg0->field_2C->field_8, 20);
        } else {
            Actor00100_MoveForward(arg0->field_2C->field_8, 20);
        }
        records = &work->objs[0].field_20;
    }
    Actor00100_Fn00508(arg0->field_2C->field_8, records, 5, &scratch->vec);
    if (Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5) == 1) {
        magnitude = abs((s16)work->field_840);
        if (magnitude < 0x80)
            work->field_6 = (u16)work->field_6 + 1;
    }
    arg0->field_2C->field_8->flg = 0;
    if (Actor00100_Fn00BF8(arg0) != 1) {
        playerCoord     = arg0->field_2C->field_8;
        scratch->vec.vx = Player_Status.coordMtx->t[0] - playerCoord->coord.t[0];
        scratch->vec.vy = Player_Status.coordMtx->t[1] - playerCoord->coord.t[1];
        scratch->vec.vz = Player_Status.coordMtx->t[2] - playerCoord->coord.t[2];
        SCHED_BARRIER();
        if ((ctx->field_8 >> 12) == D_80070F70 % 15) {
            if (!Actor00100_PatrolOutsideRadius(&scratch->vec, 2000)) {
                Gp_ArmStateF0(1);
                work->field_0 = 0x26;
            } else if (!Actor00100_PatrolOutsideRadius(&scratch->vec, 4000)) {
                coord        = arg0->field_2C->field_8;
                angle        = ratan2(scratch->vec.vx, scratch->vec.vz);
                delta        = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                value        = Actor00100_NormalizeYaw(delta);
                scratch->yaw = value;
                value        = abs(value);
                if (value < 0x300) {
                    Gp_ArmStateF0(1);
                    work->field_0 = 0x26;
                }
            }
        }
        if (D_801153F2 & 1)
            work->field_0 = 0x26;
    }
    *(Actor00100PatrolScratch**)G_SCRATCH_HEAD += 1;
}

void Actor00100_Fn0503C(Actor00100* arg0)
{
    PlayerStatus* config = &Player_Status;
    SVECTOR       initialDelta;

    Actor00100Work* work;
    Actor00100Ctx*  ctx;
    GsCOORDINATE2*  temp_a1_2;

    GsCOORDINATE2* temp_a2_2;
    GsCOORDINATE2* temp_a2_3;

    GsCOORDINATE2* temp_s0_14;
    GsCOORDINATE2* temp_s0_17;
    GsCOORDINATE2* temp_s0_20;
    GsCOORDINATE2* temp_s0_4;

    GsCOORDINATE2* temp_s0_8;

    GsCOORDINATE2* temp_v0_5;
    GsCOORDINATE2* temp_v0_7;

    Actor00100FacingScratch* scratch;
    TmdObject*               obj;

    s16 temp_a1_3;
    s16 temp_a1_4;
    s16 temp_a1_5;
    s16 temp_s0_10;
    s16 temp_s0_13;
    s16 temp_s0_16;
    s16 temp_s0_19;
    s16 temp_s0_22;

    s16 temp_s0_6;
    s16 temp_v0_4;

    s32 temp_v1_3;

    s32 var_v0_19;
    s16 var_v0_21;
    s32 var_v0_22;
    s16 var_v0_29;
    s32 var_v0_30;
    s32 var_v0_31;

    s16  var_v1_2;
    s16  var_v1_4;
    s16  var_v1_5;
    s16  var_v1_6;
    s16  var_v1_7;
    s16  var_v1_8;
    s32* scratchHead;
    s32  temp_s0_12;
    s32  temp_s0_15;
    s32  temp_s0_18;
    s32  temp_s0_21;

    s32 temp_s0_5;
    s32 temp_s0_9;
    s32 temp_v0;

    s32 spawnEffect;

    s32 var_a1_4;
    s32 effectFlags;
    s32 effectJoint;

    s32 var_v0_12;
    s32 var_v0_13;

    s32 var_v0_17;
    s32 var_v0_18;

    s32 var_v0_26;
    s32 var_v0_27;

    s32 var_v0_5;
    s32 var_v0_6;

    s32            pan;
    u16            temp_v0_6;
    u16            temp_v1_2;
    u16            var_a0;
    u16            var_v1_3;
    u32            distanceSquared;
    GsCOORDINATE2* temp_s0_11;
    Task*          player;
    GameActor*     playerWork;

    work       = arg0->field_1C;
    player     = Game_GetPtrSlot(3);
    playerWork = (GameActor*)player->work;
    ctx        = arg0->field_20;
    if (work->field_4 != 0) {
        obj              = arg0->field_2C;
        initialDelta.pad = Actor00100_PositionYaw(arg0, &initialDelta, config);
        temp_v0          = (s16)initialDelta.pad;
        if (temp_v0 > 0x300) {
            work->field_0 = 8;
        } else if (temp_v0 < -0x300) {
            work->field_0 = 9;
        }
        ctx->field_14 = 0;
        obj->field_C  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_82E        = 2;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        work->objs[3].field_20.field_C           = 0x320;
        work->field_6                            = 0;
        work->field_8                            = 0;
        work->field_C1A                          = 0;
        work->field_840                          = 0;
        work->pad_8EB[0x19]                      = 9;
        work->pad_8EB[0x1A]                      = 1;
        ((Actor00100FacingWork*)work)->field_906 = 1;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)(&work->pad_8EB[0x19]), 0x7DB);

        return;
    }
    scratch = (*(Actor00100FacingScratch**)G_SCRATCH_HEAD -= 1);
    if ((s16)work->field_82E == 3) {
        work->field_6 += 1;
    }

    if ((Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5) != 0) && ((s16)work->field_6 >= 0xB)) {
        distanceSquared          = Actor00100_D1BA90.vx * Actor00100_D1BA90.vx;
        scratch->distanceSquared = distanceSquared;
        scratch->distanceSquared = (u32)(distanceSquared + (Actor00100_D1BA90.vz * Actor00100_D1BA90.vz));
        temp_s0_4                = arg0->field_2C->field_8;
        temp_s0_5                = ratan2((s32)Actor00100_D1BA90.vx, (s32)Actor00100_D1BA90.vz);
        temp_s0_6                = temp_s0_5 - ratan2((s32)-temp_s0_4->coord.m[2][0], (s32)temp_s0_4->coord.m[2][2]);
        var_v1_2                 = Actor00100_NormalizeYaw(temp_s0_6);
        var_v0_5                 = var_v1_2 << 0x10;

        var_v0_6 = var_v0_5 >> 0x10;
        if (var_v0_6 < 0) {
            var_v0_6 = -var_v0_6;
        }

        if ((var_v0_6 >= 0x601) && ((u32)scratch->distanceSquared >= 0xE11U)) {
            if (((*(u32*)&gGameSession->field_4 & 0xFFFF0000) == 0x04010000) && Actor00100_InRegion(arg0)) {
                if (Actor00100_FacingAway(arg0->field_2C->field_8)) {
                    work->field_0 = 6;
                } else {
                    work->field_0 = 0x1D;
                }
            } else {
                work->field_0 = 0x23;
            }
        }
    }
    if ((Actor00100_Fn01388(arg0->field_2C->field_8, &work->objs[0].field_20, 5, (SVECTOR*)scratch) << 0x10) != 0) {
        if (((s16)work->field_82E == 3) && (playerWork->field_954 != 2)) {
            ((Actor00100FacingWork*)work)->field_900 = 0x80;
            temp_a1_2                                = arg0->field_2C->field_8;
            scratch->vx                              = (s16)(Player_Status.coordMtx->t[0] - temp_a1_2->coord.t[0]);
            scratch->vy                              = Player_Status.coordMtx->t[1] - temp_a1_2->coord.t[1];
            temp_v0_4                                = Player_Status.coordMtx->t[2] - temp_a1_2->coord.t[2];
            scratch->vz                              = temp_v0_4;
            scratch->contactYaw                      = ratan2((s32)scratch->vx, (s32)temp_v0_4);
            temp_v0_5                                = arg0->field_2C->field_8;
            temp_v1_2                                = scratch->contactYaw - ratan2((s32)-temp_v0_5->coord.m[2][0], (s32)temp_v0_5->coord.m[2][2]);
            var_a0                                   = temp_v1_2;
            scratch->contactYaw                      = temp_v1_2;

            var_a0    = Actor00100_NormalizeYaw(temp_v1_2);
            var_v0_12 = var_a0 << 0x10;

            var_v0_13           = var_v0_12 >> 0x10;
            scratch->contactYaw = (u16)var_v0_13;
            var_v0_13           = abs(var_v0_13);
            if (var_v0_13 < 0x180) {
                printf(&Actor00100_D000D4, playerWork->field_954, playerWork->field_956);
                if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)(&work->pad_8EB[1]), 0) == 0) {
                    Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, (SVECTOR*)scratch);
                    temp_v0_6           = ratan2((s32)scratch->vx, (s32)scratch->vz) + 0x800;
                    var_v1_3            = temp_v0_6;
                    scratch->contactYaw = temp_v0_6;

                    var_v1_3 = Actor00100_NormalizeYaw(temp_v0_6);

                    scratch->contactYaw = var_v1_3;
                    temp_s0_8           = arg0->field_2C->field_8;
                    temp_s0_9           = ratan2((s32)scratch->vx, (s32)scratch->vz);
                    temp_s0_10          = temp_s0_9 - ratan2((s32)-temp_s0_8->coord.m[2][0], (s32)temp_s0_8->coord.m[2][2]);
                    var_v1_4            = Actor00100_NormalizeYaw(temp_s0_10);

                    scratch->turnYaw = var_v1_4;
                    scratch->vy      = 0;
                    scratch->vx      = (s16) - (s16)(u16)scratch->vx;
                    scratch->vz      = -(s16)(u16)scratch->vz;
                    temp_s0_11       = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
                    temp_s0_12       = ratan2((s32)scratch->vx, (s32)scratch->vz);
                    temp_s0_13       = temp_s0_12 - ratan2((s32)-temp_s0_11->coord.m[2][0], (s32)temp_s0_11->coord.m[2][2]);
                    var_v1_5         = Actor00100_NormalizeYaw(temp_s0_13);
                    var_v0_17        = var_v1_5 << 0x10;

                    var_v0_18          = var_v0_17 >> 0x10;
                    scratch->playerYaw = (s16)var_v0_18;
                    var_v0_18          = abs(var_v0_18);
                    if (var_v0_18 < 0x400) {
                        work->field_BF8 = &Actor00100_D1B9AC;
                    } else {
                        work->field_BF8     = &Actor00100_D1B9D0;
                        scratch->contactYaw = (u16)(scratch->contactYaw + 0x800);
                    }
                    ((Actor00100FacingWork*)work)->field_8D0 = 0;
                    ((Actor00100FacingWork*)work)->field_8D2 = (u16)scratch->contactYaw;
                    ((Actor00100FacingWork*)work)->field_8D4 = 0;
                    ((Actor00100FacingWork*)work)->field_8C0 = (s32)((TmdObject*)player->extra)->field_8->coord.t[0];
                    ((Actor00100FacingWork*)work)->field_8C4 = (s32)((TmdObject*)player->extra)->field_8->coord.t[1];
                    ((Actor00100FacingWork*)work)->field_8C8 = (s32)((TmdObject*)player->extra)->field_8->coord.t[2];
                    Gp_DispatchMsg(player, 0x3E9, (s32)(&work->pad_8B8[8]), 0);
                    if (work->field_C1A < 0x3E8) {
                        var_v0_19 = scratch->playerYaw;
                        if (var_v0_19 < 0) {
                            var_v0_19 = -var_v0_19;
                        }
                        if (var_v0_19 < 0x400) {
                            scratch->messageResult = Actor00100_PlayerContactMessage(ctx, 2);
                        } else {
                            scratch->messageResult = Actor00100_PlayerContactMessage(ctx, 3);
                        }
                        if (scratch->messageResult != 1) {
                            work->field_BFC = 3;
                            work->field_C00 = 0;
                            work->field_C04 = 0;
                            work->field_8D8 = 0;
                            work->field_8DC = 0;
                            work->field_8E0 = 0;
                            work->field_8E8 = 7;
                            work->field_8EA = 1;
                            work->field_C18 = 1;
                            Gp_DispatchMsg(player, 0x3FF, (s32)(&work->field_BF8), 0);
                        }
                        var_v0_21 = 0x25;
                    } else {
                        var_v0_22 = scratch->playerYaw;
                        if (var_v0_22 < 0) {
                            var_v0_22 = -var_v0_22;
                        }
                        if (var_v0_22 < 0x400) {
                            scratch->messageResult = Actor00100_PlayerContactMessage(ctx, 0);
                        } else {
                            scratch->messageResult = Actor00100_PlayerContactMessage(ctx, 1);
                        }
                        if (scratch->messageResult == 1) {
                            ((GameActor*)player->work)->field_956 = 0xA;
                        }
                        work->field_BFC = 1;
                        work->field_C00 = 0;
                        work->field_C04 = 0;
                        work->field_8D8 = 0;
                        work->field_8DC = 0;
                        work->field_8E0 = 0;
                        work->field_8E8 = 7;
                        work->field_8EA = 1;
                        work->field_C18 = 1;
                        Gp_DispatchMsg(player, 0x3FF, (s32)(&work->field_BF8), 0);
                        var_v0_21 = 0x1E;
                    }
                    work->field_0 = var_v0_21;
                }
            }
            temp_a2_2   = arg0->field_2C->field_8;
            scratch->vx = (s16)(Player_Status.coordMtx->t[0] - temp_a2_2->coord.t[0]);
            scratch->vy = Player_Status.coordMtx->t[1] - temp_a2_2->coord.t[1];
            temp_a1_3   = Player_Status.coordMtx->t[2] - temp_a2_2->coord.t[2];
            scratch->vz = temp_a1_3;
            temp_s0_14  = arg0->field_2C->field_8;
            temp_s0_15  = ratan2((s32)scratch->vx, (s32)temp_a1_3);
            temp_s0_16  = temp_s0_15 - ratan2((s32)-temp_s0_14->coord.m[2][0], (s32)temp_s0_14->coord.m[2][2]);
            var_v1_6    = Actor00100_NormalizeYaw(temp_s0_16);

            scratch->targetYaw = var_v1_6;
        } else {
            goto updatePlayerYaw;
        }
    } else {
    updatePlayerYaw:
        temp_a2_3   = arg0->field_2C->field_8;
        scratch->vx = (s16)(Player_Status.coordMtx->t[0] - temp_a2_3->coord.t[0]);
        scratch->vy = Player_Status.coordMtx->t[1] - temp_a2_3->coord.t[1];
        temp_a1_4   = Player_Status.coordMtx->t[2] - temp_a2_3->coord.t[2];
        scratch->vz = temp_a1_4;
        temp_s0_17  = arg0->field_2C->field_8;
        temp_s0_18  = ratan2((s32)scratch->vx, (s32)temp_a1_4);
        temp_s0_19  = temp_s0_18 - ratan2((s32)-temp_s0_17->coord.m[2][0], (s32)temp_s0_17->coord.m[2][2]);
        var_v1_7    = Actor00100_NormalizeYaw(temp_s0_19);
        var_v0_26   = var_v1_7 << 0x10;

        var_v0_27          = var_v0_26 >> 0x10;
        scratch->targetYaw = (s16)var_v0_27;
        var_v0_27          = abs(var_v0_27);
        if ((var_v0_27 >= 0x601) && ((s16)work->field_82E == 3)) {
            work->field_0 = 0x1D;
        }
    }
    arg0->field_2C->field_8->flg = 0;
    temp_s0_20                   = arg0->field_2C->field_8;
    temp_s0_21                   = ratan2((s32)((Actor00100FacingWork*)work)->field_BF0, (s32)((Actor00100FacingWork*)work)->field_BF4);
    temp_s0_22                   = temp_s0_21 - ratan2((s32)-temp_s0_20->coord.m[2][0], (s32)temp_s0_20->coord.m[2][2]);
    var_v1_8                     = Actor00100_NormalizeYaw(temp_s0_22);

    scratch->turnYaw = var_v1_8;
    Actor00100_Fn02788(arg0);
    if ((s16)work->field_82E == 2) {
        if (scratch->turnYaw >= 0x41) {
            scratch->turnYaw = 0x40;
        }
        if (scratch->turnYaw < -0x40) {
            scratch->turnYaw = -0x40;
        }
        temp_v0_7        = arg0->field_2C->field_8;
        temp_a1_5        = (u16)scratch->turnYaw + ratan2((s32)-temp_v0_7->coord.m[2][0], (s32)temp_v0_7->coord.m[2][2]);
        scratch->turnYaw = temp_a1_5;
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s32)temp_a1_5, 1);
        arg0->field_2C->field_8->flg = 0;
    } else {
        var_a1_4 = Actor00100_HasRecord10(arg0);
        if (var_a1_4 != 0) {
            Actor00100_MoveForward(arg0->field_2C->field_8, 0x55);
            var_v0_29 = (u16)work->field_C1A + 0x55;
        } else {
            Actor00100_MoveForward(arg0->field_2C->field_8, 0xC8);
            var_v0_29 = (u16)work->field_C1A + 0xC8;
        }
        work->field_C1A = var_v0_29;
    }
    if ((s16)work->field_82E == 2) {
        work->field_8 = (u16)work->field_8 + 1;
    }
    if (work->field_8 > ((Actor00100FacingWork*)work)->field_C1E) {
        temp_v1_3 = (s16)work->field_82E;
        if (temp_v1_3 == 2) {
            var_v0_30 = scratch->targetYaw;
            if (var_v0_30 < 0) {
                var_v0_30 = -var_v0_30;
            }
            if ((var_v0_30 < 0x80) || (work->field_68 & 0x100)) {
                work->field_82E = 3;
                work->field_828 = (u16)temp_v1_3;
                pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                SndEvt_EnqueueType6(0x40010006, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
                work->pad_8EB[0x19]                      = 9;
                work->pad_8EB[0x1A]                      = 1;
                ((Actor00100FacingWork*)work)->field_906 = 4;
                Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&work->pad_8EB[0x19], 0x7DB);
            }
        }
    }
    if (work->field_8 >= 0xF) {
        var_v0_31 = scratch->targetYaw;
        if (var_v0_31 < 0) {
            var_v0_31 = -var_v0_31;
        }
        if (var_v0_31 >= 0x81) {
            goto updatePlayerDelta;
        }
    } else {
    updatePlayerDelta:
        if ((s16)work->field_82E == 2) {
            ((Actor00100FacingWork*)work)->field_BF0 = (s16)(config->coordMtx->t[0] - arg0->field_2C->field_8->coord.t[0]);
            ((Actor00100FacingWork*)work)->field_BF2 = (s16)(config->coordMtx->t[1] - arg0->field_2C->field_8->coord.t[1]);
            ((Actor00100FacingWork*)work)->field_BF4 = (s16)(config->coordMtx->t[2] - arg0->field_2C->field_8->coord.t[2]);
        }
    }
    if ((s16)work->field_82E == 3) {
        switch (work->field_5A & 0x3FF) {
            case 5:
                spawnEffect        = 1;
                effectJoint        = 7;
                effectFlags        = 0x4300;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x2BC;
                break;
            case 8:
                spawnEffect        = 1;
                effectJoint        = 9;
                effectFlags        = 0x3500;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x2BC;
                break;
            case 10:
                spawnEffect        = 1;
                effectJoint        = 14;
                effectFlags        = 0x5A00;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x258;
                break;
            case 13:
                spawnEffect        = 1;
                effectJoint        = 17;
                effectFlags        = 0x4800;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x258;
                break;
            default:
                spawnEffect = 0;
                effectJoint = 0;
                effectFlags = 1;
                break;
        }
        if (Gp_State1C->field_A == 2) {
            scratchHead = (s32*)G_SCRATCH_HEAD;
            if (spawnEffect == 1) {
                Gp_SpawnEff(0x60054, &arg0->field_2C->field_8[effectJoint], effectFlags | 0x80000000, &work->field_898);
                goto releaseScratch;
            }
        } else {
            goto releaseScratch;
        }
    } else {
    releaseScratch:
        scratchHead = (s32*)G_SCRATCH_HEAD;
    }
    *scratchHead += 0x18;
}

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

void Actor00100_Fn06654(Actor00100* arg0)
{
    Actor00100Work* work;
    Actor00100Ctx*  ctx;
    TmdObject*      obj;
    SVECTOR*        head;
    SVECTOR*        vec;
    s32             x, z;
    s16             yaw;
    s32             outside;
    s32             state;

    head = *(SVECTOR**)G_SCRATCH_HEAD;
    vec  = (*(SVECTOR**)G_SCRATCH_HEAD = head - 2);
    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj           = arg0->field_2C;
        ctx->field_14 = 0;
        obj->field_C  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82E        = 5;
        work->field_82A        = 0;
        work->field_83E        = 0;
        work->field_C28        = 0;
        work->field_6          = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(0x20);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        x                              = head[-2].vx;
        work->field_8DC                = 0;
        work->field_8D8                = x;
        z                              = vec->vz;
        work->field_8E8                = 7;
        work->field_8EA                = 1;
        work->objs[3].field_20.field_C = 0x320;
        work->field_8E0                = z;
        Gp_SpawnPadLerp(3, 0xFFU, 8U);
    }
    work->field_6 += 1;
    Actor00100_Fn02788(arg0);
    state = (s16)work->field_82E;
    switch (state) {
        case 5:
            if (work->field_68 & 0x100) {
                Actor00100_ConfigPositionDelta(&Player_Status, arg0->field_2C->field_8, vec);
                outside = Actor00100_OutsideRadius(vec, 2000);
                if (outside) {
                    work->field_0 = 0x26;
                } else {
                    work->field_0 = 0x1F;
                }
            }
            break;
        case 3:
            yaw       = Actor00100_PositionYaw(arg0, vec, &Player_Status);
            vec[1].vz = yaw;
            if (Actor00100_HasRecord10(arg0)) {
                Actor00100_MoveForward(arg0->field_2C->field_8, 85);
            } else {
                Actor00100_MoveForward(arg0->field_2C->field_8, 200);
            }
            if (Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5)) {
                work->field_0 = 0x23;
            }
            if (work->field_6 >= 0x15) {
                work->field_828 = 1;
                work->field_82A = 0;
                work->field_82E = 5;
                work->field_832 = work->field_834;
            }
            break;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

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
        work->field_8                  = 0;
        work->objs[3].field_20.field_C = -0x2D0;
    }
    work->field_6 += 1;
    Actor00100_Fn02788(arg0);
    if (work->field_68 & 0x100) {
        work->field_0 = 0x26;
    }
    if (((u32)((work->field_5A & 0x3FF) - 6) < 8U) && (work->field_8 < 5)) {
        if (Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5) != 0) {
            work->field_8 = (s16)((u16)work->field_8 + 1);
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
    head[-1].x                   = (s16)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
    scratch->y                   = (s16)(Player_Status.coordMtx->t[1] - coord->coord.t[1]);
    scratch->z                   = (s16)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
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
    scratch->x         = (s16)(Player_Status.coordMtx->t[0] - coord2->coord.t[0]);
    scratch->y         = (s16)(Player_Status.coordMtx->t[1] - coord2->coord.t[1]);
    z                  = Player_Status.coordMtx->t[2] - coord2->coord.t[2];
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

void Actor00100_Fn0782C(Actor00100* arg0)
{
    s32                    radius = 0x5DC;
    Actor00100Ctx*         ctx;
    Actor00100Work*        work;
    GpRec18*               record;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord2;
    GsCOORDINATE2*         coord3;
    GsCOORDINATE2*         facing3;
    GsCOORDINATE2*         facing4;
    GsCOORDINATE2*         facing5;
    GsCOORDINATE2*         facing;
    GsCOORDINATE2*         facing2;
    GsCOORDINATE2*         turnCoord;
    MATRIX*                matrix;
    Actor00100MoveScratch* scratch;
    SVECTOR*               target;
    SVECTOR*               target2;
    Actor00100MoveScratch* head;
    SVECTOR*               direction;
    Actor00100MoveScratch* head2;
    TmdObject*             obj;
    s16                    targetDelta;
    s16                    delta;
    s16                    yaw;
    s16                    delta3;
    s16                    delta4;
    s16                    delta5;
    s32                    playerX;
    s16                    delta1;
    s16                    delta2;
    s16                    targetYaw;
    s16                    z;
    s32                    magnitude;
    s32                    targetMagnitude;
    s16                    adjustedDelta;
    s32                    originalMagnitude;
    s16                    wrapped;
    s16                    wrapped2;
    s16                    wrapped3;
    s16                    wrapped4;
    s16                    wrapped5;
    s16                    wrappedYaw;
    s32                    angle3;
    s32                    angle4;
    s32                    angle5;
    s32                    angle;
    s32                    angle2;
    s32                    finalYaw;
    s32                    turnDelta;
    s32                    finalDelta;
    s32                    yawDifference;
    u16                    unsignedDelta;
    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        head          = *(Actor00100MoveScratch**)G_SCRATCH_HEAD;
        obj           = arg0->field_2C;
        scratch       = (*(Actor00100MoveScratch**)G_SCRATCH_HEAD = head - 1);
        ctx->field_14 = 0;
        obj->field_C  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].field_1C = 0x19C;
        work->field_828        = 1;
        work->field_82A        = 0;
        work->field_82E        = 0;
        work->objs[2].flags   |= 0x4000;
        work->field_832        = work->field_834;
        Actor00100_Fn02788(arg0);
        Actor00100_Fn02788(arg0);
        work->field_6   = 0;
        work->field_8   = 0;
        coord           = arg0->field_2C->field_8;
        head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
        scratch->vec.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        z               = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        scratch->vec.vz = z;
        facing          = arg0->field_2C->field_8;
        angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
        delta1          = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
        wrapped         = delta1;
        if (delta1 < 0) {
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
        work->field_840 = wrapped;
        matrix          = &scratch->matrix;
        Gfx_RotMatrixY(matrix, (s16)ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz) + 0x3E8, 1);
        Gfx_MatrixCol2(matrix, &scratch->vec);
        VectorNormalSS(&scratch->vec, &scratch->vec);
        gte_lddp(1000);
        gte_ldsv(&scratch->vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(&scratch->vec);
        ((Actor00100MoveWork*)work)->index        = 0;
        ((Actor00100MoveWork*)work)->pos[0][0]    = (s16)((u16)scratch->vec.vx + arg0->field_2C->field_8->coord.t[0]);
        *(Actor00100MoveScratch**)G_SCRATCH_HEAD += 1;
        ((Actor00100MoveWork*)work)->pos[0][1]    = (s16)((u16)scratch->vec.vz + arg0->field_2C->field_8->coord.t[2]);
        work->objs[3].field_20.field_C            = 0x26C;
        return;
    }
    work->field_8      += 1;
    head2               = *(Actor00100MoveScratch**)G_SCRATCH_HEAD;
    scratch             = (*(Actor00100MoveScratch**)G_SCRATCH_HEAD = head2 - 1);
    head2[-1].vec.vx    = (s16)(((Actor00100MoveWork*)work)->pos[((Actor00100MoveWork*)work)->index][0] - arg0->field_2C->field_8->coord.t[0]);
    scratch->vec.vy     = 0;
    scratch->vec.vz     = ((Actor00100MoveWork*)work)->pos[((Actor00100MoveWork*)work)->index][1] - arg0->field_2C->field_8->coord.t[2];
    coord2              = arg0->field_2C->field_8;
    head2[-1].target.vx = (s16)(Player_Status.coordMtx->t[0] - coord2->coord.t[0]);
    target              = &head2[-1].target;
    target->vy          = Player_Status.coordMtx->t[1] - coord2->coord.t[1];
    target->vz          = Player_Status.coordMtx->t[2] - coord2->coord.t[2];
    if (!Actor00100_OutsideRadius(&scratch->vec, 0xA0) || work->field_6 >= 0x15) {
        facing2  = arg0->field_2C->field_8;
        angle2   = ratan2((s32)head2[-1].target.vx, (s32)target->vz);
        delta2   = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
        wrapped2 = delta2;
        if (delta2 < 0) {
        wrapNegative2:
            if (wrapped2 < -0x800) {
                wrapped2 += 0x1000;
                goto wrapNegative2;
            }
        } else {
        wrapPositive2:
            if (wrapped2 >= 0x801) {
                wrapped2 -= 0x1000;
                goto wrapPositive2;
            }
        }
        work->field_840 = wrapped2;
        if (((Actor00100MoveWork*)work)->index == 0) {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) - 0x2EE, 1);
            ((Actor00100MoveWork*)work)->index = 1;
        } else {
            Gfx_RotMatrixY(&scratch->matrix, (s16)ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x2EE, 1);
            ((Actor00100MoveWork*)work)->index = 0;
        }
        direction = &scratch->target;
        Gfx_MatrixCol2(&scratch->matrix, direction);
        VectorNormalSS(direction, direction);
        gte_lddp(2000);
        gte_ldsv(direction);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(direction);
        ((Actor00100MoveWork*)work)->pos[((Actor00100MoveWork*)work)->index][0] = (s16)((u16)scratch->target.vx + arg0->field_2C->field_8->coord.t[0]);
        ((Actor00100MoveWork*)work)->pos[((Actor00100MoveWork*)work)->index][1] = (s16)((u16)scratch->target.vz + arg0->field_2C->field_8->coord.t[2]);
        work->field_6                                                           = 0;
    }
    Actor00100_Fn02788(arg0);
    facing3  = arg0->field_2C->field_8;
    angle3   = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz);
    delta3   = angle3 - ratan2((s32)-facing3->coord.m[2][0], (s32)facing3->coord.m[2][2]);
    wrapped3 = delta3;
    if (delta3 < 0) {
    wrapNegative3:
        if (wrapped3 < -0x800) {
            wrapped3 += 0x1000;
            goto wrapNegative3;
        }
    } else {
    wrapPositive3:
        if (wrapped3 >= 0x801) {
            wrapped3 -= 0x1000;
            goto wrapPositive3;
        }
    }
    work->field_840 = wrapped3;
    facing4         = arg0->field_2C->field_8;
    angle4          = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
    delta4          = angle4 - ratan2((s32)-facing4->coord.m[2][0], (s32)facing4->coord.m[2][2]);
    wrapped4        = delta4;
    if (delta4 < 0) {
    wrapNegative4:
        if (wrapped4 < -0x800) {
            wrapped4 += 0x1000;
            goto wrapNegative4;
        }
    } else {
    wrapPositive4:
        if (wrapped4 >= 0x801) {
            wrapped4 -= 0x1000;
            goto wrapPositive4;
        }
    }
    turnDelta         = wrapped4;
    scratch->original = (scratch->delta = (s16)turnDelta);
    delta             = scratch->delta;
    unsignedDelta     = (u16)scratch->delta;
    magnitude         = abs(scratch->delta);
    if (magnitude >= 0x601) {
        targetDelta     = work->field_840;
        targetMagnitude = abs(targetDelta);
        if ((targetMagnitude >= 0x101) && ((targetDelta * delta) < 0)) {
            adjustedDelta = unsignedDelta - 0x1000;
            if (delta < 0) {
                adjustedDelta = unsignedDelta + 0x1000;
            }
            scratch->delta = adjustedDelta;
        }
    }
    if (scratch->delta >= 0x21) {
        scratch->delta = 0x20;
    }
    if (scratch->delta < -0x20) {
        scratch->delta = -0x20;
    }
    work->field_83E = scratch->delta * 0x10;
    turnCoord       = arg0->field_2C->field_8;
    yaw             = (u16)scratch->delta + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->delta  = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s32)yaw, 1);
    record = &work->objs[0].field_20;
    if ((s16)work->field_82A == 0) {
        if (Actor00100_HasRecord10(arg0)) {
            Actor00100_MoveForward(arg0->field_2C->field_8, 20);
        } else {
            Actor00100_MoveForward(arg0->field_2C->field_8, 20);
        }
        record = &work->objs[0].field_20;
    }
    Actor00100_Fn00508(arg0->field_2C->field_8, record, 5, &scratch->vec);
    if (Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5) == 1) {
        originalMagnitude = abs(scratch->original);
        if (originalMagnitude < 0x20) {
            work->field_6 += 1;
        }
    }
    arg0->field_2C->field_8->flg = 0;
    if ((Actor00100_Fn00BF8(arg0) != 1) && (target2 = &scratch->target, coord3 = arg0->field_2C->field_8, scratch->target.vx = (s16)(Player_Status.coordMtx->t[0] - coord3->coord.t[0]), target2->vy = Player_Status.coordMtx->t[1] - coord3->coord.t[1], target2->vz = Player_Status.coordMtx->t[2] - coord3->coord.t[2], ((work->field_8 > work->field_C22) != 0))) {
        if (work->field_C26 <= 0) {
            if (((u16)ctx->field_8 >> 0xC) == (D_80070F70 % 15)) {
                if (Actor00100_OutsideRadius(&scratch->target, radius)) {
                    if (!Actor00100_OutsideRadius(&scratch->target, 0x1F40) && work->field_8 >= 0x1C3) {
                        facing5  = arg0->field_2C->field_8;
                        angle5   = ratan2((s32)scratch->vec.vx, (s32)scratch->vec.vz);
                        delta5   = angle5 - ratan2((s32)-facing5->coord.m[2][0], (s32)facing5->coord.m[2][2]);
                        wrapped5 = delta5;
                        if (delta5 < 0) {
                        wrapNegative5:
                            if (wrapped5 < -0x800) {
                                wrapped5 += 0x1000;
                                goto wrapNegative5;
                            }
                        } else {
                        wrapPositive5:
                            if (wrapped5 >= 0x801) {
                                wrapped5 -= 0x1000;
                                goto wrapPositive5;
                            }
                        }
                        finalDelta     = wrapped5;
                        scratch->delta = (s16)finalDelta;
                        finalDelta     = abs(finalDelta);
                        if (finalDelta < 0x300) {
                            goto changeState;
                        }
                    }
                } else {
                changeState:
                    work->field_0 = 0x1C;
                }
                playerX            = -((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][0];
                scratch->playerYaw = ratan2((s32)playerX, (s32)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8->coord.m[2][2]);
                targetYaw          = ratan2((s32)scratch->target.vx, (s32)scratch->target.vz) + 0x800;
                wrappedYaw         = targetYaw;
                scratch->yaw       = targetYaw;
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
                finalYaw      = wrappedYaw;
                scratch->yaw  = (s16)finalYaw;
                yawDifference = finalYaw - scratch->playerYaw;
                if (yawDifference < 0) {
                    yawDifference = -yawDifference;
                }
                if ((yawDifference >= 0x601) || (Gp_NodeSlotMask(&ctx->field_10) == 0)) {
                    work->field_0 = 0x1C;
                }
            }
            goto checkFlag;
        }
    } else {
    checkFlag:
        if ((work->field_C26 <= 0) && (D_801153F2 & 2)) {
            work->field_0 = 0x1C;
        }
    }
    *(Actor00100MoveScratch**)G_SCRATCH_HEAD += 1;
}

void Actor00100_Fn08588(Actor00100* arg0)
{
    TmdObject*             obj;
    Actor00100Ctx*         ctx;
    Actor00100Work*        work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord2;
    GsCOORDINATE2*         targetCoord;
    GsCOORDINATE2*         facing;
    GsCOORDINATE2*         facing2;
    Actor00100TurnScratch* head;
    Actor00100TurnScratch* scratch;
    s16                    yaw;
    s16                    delta;
    s16                    z;
    s16                    steps;
    s16                    wrapped;
    s32                    angle;
    s32                    firstDelta;

    head    = *(Actor00100TurnScratch**)G_SCRATCH_HEAD;
    scratch = (*(Actor00100TurnScratch**)G_SCRATCH_HEAD = head - 1);
    work    = arg0->field_1C;
    ctx     = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->field_C           = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 1;
        work->field_82E        = 0x11;
        work->field_832        = 0x10;
        work->field_6          = 0;
    }
    work->field_6 += 1;
    Actor00100_Fn02788(arg0);
    targetCoord     = arg0->field_2C->field_8;
    head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->vec.vy = Player_Status.coordMtx->t[1] - targetCoord->coord.t[1];
    z               = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->vec.vz = z;
    facing          = arg0->field_2C->field_8;
    angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
    delta           = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped         = delta;
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
    firstDelta      = wrapped;
    scratch->delta  = (s16)firstDelta;
    work->field_840 = (u16)firstDelta;
    if (scratch->delta < 0) {
        if (abs(scratch->delta) >= 0x401) {
            work->field_840 = firstDelta + 0x800;
            scratch->delta += 0x800;
        }
    }
    if (abs(scratch->delta) < 0x80) {
        work->field_0 = 0x1C;
    }
    steps          = 0x1E - work->field_6;
    scratch->steps = steps;
    if (steps == 0) {
        scratch->steps = 1;
    }
    facing2      = arg0->field_2C->field_8;
    yaw          = ((s16)scratch->delta / (s16)scratch->steps) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->yaw = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s32)yaw, 1);
    Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, &scratch->vec);
    VectorNormalSS(&scratch->vec, &scratch->vec);
    gte_lddp(-0x1A);
    gte_ldsv(&scratch->vec);
    __asm__ volatile("nop; nop; .word 0x4B98003D");
    gte_stsv(&scratch->vec);
    coord               = arg0->field_2C->field_8;
    coord->coord.t[0]  += scratch->vec.vx;
    coord2              = arg0->field_2C->field_8;
    coord2->coord.t[2] += scratch->vec.vz;
    Actor00100_MoveForward(arg0->field_2C->field_8, -8);
    arg0->field_2C->field_8->flg = 0;
    if (abs(scratch->delta) < 0x20) {
        work->field_0 = 0x1C;
    }
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
    *(Actor00100TurnScratch**)G_SCRATCH_HEAD += 1;
}

void Actor00100_Fn08A14(Actor00100* arg0)
{
    TmdObject*             obj;
    Actor00100Ctx*         ctx;
    Actor00100Work*        work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord2;
    GsCOORDINATE2*         targetCoord;
    GsCOORDINATE2*         facing;
    GsCOORDINATE2*         facing2;
    Actor00100TurnScratch* head;
    Actor00100TurnScratch* scratch;
    s16                    yaw;
    s16                    delta;
    s16                    z;
    s16                    steps;
    s16                    wrapped;
    s32                    angle;
    s32                    firstDelta;

    head    = *(Actor00100TurnScratch**)G_SCRATCH_HEAD;
    scratch = (*(Actor00100TurnScratch**)G_SCRATCH_HEAD = head - 1);
    work    = arg0->field_1C;
    ctx     = arg0->field_20;
    if (work->field_4 != 0) {
        obj                    = arg0->field_2C;
        work->field_BE4        = 0;
        obj->field_C           = 0;
        work->objs[0].field_1C = 0x19C;
        work->objs[2].flags   |= 0x4000;
        ctx->field_14          = 0;
        work->field_828        = 1;
        work->field_82E        = 0x12;
        work->field_832        = 0x10;
        work->field_6          = 0;
    }
    work->field_6 += 1;
    Actor00100_Fn02788(arg0);
    targetCoord     = arg0->field_2C->field_8;
    head[-1].vec.vx = (s16)(Player_Status.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->vec.vy = Player_Status.coordMtx->t[1] - targetCoord->coord.t[1];
    z               = Player_Status.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->vec.vz = z;
    facing          = arg0->field_2C->field_8;
    angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
    delta           = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped         = delta;
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
    firstDelta      = wrapped;
    scratch->delta  = (s16)firstDelta;
    work->field_840 = (u16)firstDelta;
    if (scratch->delta > 0) {
        if (abs(scratch->delta) >= 0x401) {
            work->field_840 = firstDelta - 0x800;
            scratch->delta -= 0x800;
        }
    }
    steps          = 0x1E - work->field_6;
    scratch->steps = steps;
    if (steps == 0) {
        scratch->steps = 1;
    }
    facing2      = arg0->field_2C->field_8;
    yaw          = ((s16)scratch->delta / (s16)scratch->steps) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->yaw = yaw;
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s32)yaw, 1);
    Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, &scratch->vec);
    VectorNormalSS(&scratch->vec, &scratch->vec);
    gte_lddp(0x1A);
    gte_ldsv(&scratch->vec);
    __asm__ volatile("nop; nop; .word 0x4B98003D");
    gte_stsv(&scratch->vec);
    coord               = arg0->field_2C->field_8;
    coord->coord.t[0]  += scratch->vec.vx;
    coord2              = arg0->field_2C->field_8;
    coord2->coord.t[2] += scratch->vec.vz;
    Actor00100_MoveForward(arg0->field_2C->field_8, -8);
    Actor00100_Fn00A54(arg0->field_2C->field_8, &work->objs[2].field_20, 5);
    if (abs(scratch->delta) < 0x20) {
        work->field_0 = 0x1C;
    }
    if (work->field_68 & 0x100) {
        work->field_0 = 0x1C;
    }
    *(Actor00100TurnScratch**)G_SCRATCH_HEAD += 1;
}

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
    head[-1].x                   = (s16)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
    scratch->y                   = (s16)(Player_Status.coordMtx->t[1] - coord->coord.t[1]);
    scratch->z                   = (s16)(Player_Status.coordMtx->t[2] - coord->coord.t[2]);
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
    scratch->x         = (s16)(Player_Status.coordMtx->t[0] - coord2->coord.t[0]);
    scratch->y         = (s16)(Player_Status.coordMtx->t[1] - coord2->coord.t[1]);
    z                  = Player_Status.coordMtx->t[2] - coord2->coord.t[2];
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
        if ((*(u32*)&gGameSession->field_4 & 0xFFFF0000) == 0x04010000) {
            Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, (s32)((u16)ctx->field_8 >> 0xC), 0);
        }
        arg0->field_30++;
    }
}

void Actor00100_Fn09724(Actor00100* arg0)
{
    Actor00100Ctx*  ctx;
    Actor00100Work* work;
    TmdObject*      obj;
    s16             timer;
    s16             state;
    s32             sound;
    s32             pan;

    work                   = arg0->field_1C;
    *(s32*)G_SCRATCH_HEAD -= 0x10;
    ctx                    = arg0->field_20;
    if (work->field_4 != 0) {
        obj           = arg0->field_2C;
        ctx->field_14 = 0;
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
        func_801811C4(0x7D0);
    }
    work->field_6 += 1;
    Actor00100_Fn02788(arg0);
    state = work->field_82E;
    switch (state) {
        case 3:
            timer = (s16)work->field_6;
            if (timer < 0x1E) {
                Actor00100_ScaleTransform(&work->field_BA0, (timer << 12) / 30);
            } else {
                work->field_82E = 0xD;
                work->field_828 = 1;
                work->field_6   = 0;
                sound           = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40010010;
                pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
            }
            Actor00100_MoveForward(arg0->field_2C->field_8, 200);
            break;
        case 13:
            if ((s16)work->field_6 <= ((s16)work->field_834 * 17) / 16) {
                Actor00100_MoveForwardNonzero(arg0->field_2C->field_8, ((s16)work->field_834 * 2000) / 272);
            } else if ((s16)work->field_6 <= ((s16)work->field_834 * 25) / 16) {
                Actor00100_MoveForwardNonzero(arg0->field_2C->field_8, ((s16)work->field_834 * 1000) / 192);
            }
            if (work->field_68 & 0x100) {
                work->field_0 = 0x26;
            }
            break;
    }
    *(s32*)G_SCRATCH_HEAD       += 0x10;
    arg0->field_2C->field_8->flg = 0;
}

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
