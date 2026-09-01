#include "common.h"

#include "actors/actor_400100.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include <psyq/inline_c.h>

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn001FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn00508);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn00A54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn00BF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn00E58);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn01388);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn01900);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn01D74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn01EEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn02788);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn02C54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn03340);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0375C);

#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

extern GsCOORDINATE2 Gfx_ViewCoord;
extern s8            D_80114C12;
extern u8            D_80071075;

void  Gp_UnlinkObj(Actor00100Obj* node);
void  Gp_SetLightMode(Actor00100Ctx* arg0, s32 arg1);
void* Gp_SpawnEff(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, SVECTOR* arg3);
s32   Gp_DispatchMsg(void* arg0, s32 arg1, s32 arg2, s32 arg3);

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
        Gp_UnlinkObj(&work->objs[0]);
        Gp_UnlinkObj(&work->objs[1]);
        Gp_UnlinkObj(&work->objs[3]);
        Gp_UnlinkObj(&work->objs[2]);
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
            Gp_SetLightMode(ctx, 0);
            Gp_SetLightMode(ctx, 1);
            /* fallthrough */
        case 0xA:
            arg0->field_2C->field_C = 2;
            Gp_SetLightMode(ctx, 2);
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

        o     = arg0->field_2C;
        k1000 = 0x1000;
        sy    = k1000 - v;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(h));
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

        o2 = arg0->field_2C;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(h2));
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

        __asm__ volatile("lui %0, 0x1F80" : "=r"(h3));
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

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn06398);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn06654);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn06C10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn070DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0747C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn07650);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0782C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn08588);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn08A14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn08E7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn09310);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn09724);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn09CCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0A288);

void Actor00100_Fn0B134(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B13C);

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

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B264);

void Actor00100_L0B2AC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B2B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B3B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B3DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B4D8);

void Actor00100_L0B524(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B52C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B658);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B730);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B7DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B8D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0B98C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0BA70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0BB2C);

void Actor00100_Fn0BC14(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0BC1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0BCBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_text", Actor00100_Fn0BD28);
