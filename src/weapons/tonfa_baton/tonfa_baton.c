#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/tonfa_baton.h"

/// Near vector for the baton's swing arc.
SVECTOR D_tonfa_baton_8011E0F0[1] = { { 0, 0x0080, 0, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_tonfa_baton_8011E0F0[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
SVECTOR D_tonfa_baton_8011E0F8 = { 0, -0x0200, 0, 0 };

/// Package header word 0: the overlay id, `item id - 0x78`
/// (item 0x92, Tonfa Baton).
const s32 D_tonfa_baton_8011D1C0 = 0x1A;

void WeaponsShared8011db78(Task* task);

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

void func_tonfa_baton_8011D1EC(Task* task)
{
    GsCOORDINATE2  local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;
    s32            flags;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        switch (task->state) {
            case 0:
                coord->sub        = work->field_8;
                coord->coord.t[0] = D_tonfa_baton_8011E0F0[0].vx;
                coord->coord.t[1] = D_tonfa_baton_8011E0F0[0].vy;
                coord->coord.t[2] = D_tonfa_baton_8011E0F0[0].vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->state      = 1;
                vec              = &D_tonfa_baton_8011E0F0[1];
                local.sub        = coord;
                local.coord.t[0] = vec->vx;
                local.coord.t[1] = vec->vy;
                local.coord.t[2] = vec->vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst        = &D_tonfa_baton_8012BBEC[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_tonfa_baton_8012BE6C[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                }
                flags = 0x13;
                if (task->spawnArg1 == 0) {
                    flags = 1;
                }
                D_tonfa_baton_8012C0EC = flags;
                break;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);
                local.sub        = work->field_8;
                local.coord.t[0] = D_tonfa_baton_8011E0F8.vx;
                local.coord.t[1] = D_tonfa_baton_8011E0F8.vy;
                local.coord.t[2] = D_tonfa_baton_8011E0F8.vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                dst        = &D_tonfa_baton_8012BBEC[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_tonfa_baton_8012BE6C[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_tonfa_baton_8012BBEC[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_tonfa_baton_8012BE6C[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_tonfa_baton_8011D6B0(work->field_22 & 7, D_tonfa_baton_8012C0EC);
                break;
        }
        if (work->field_22 >= 0x1F) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

void func_tonfa_baton_8011D6B0(s16 slot, s16 flags)
{
    TonfaBeamScratch* blk;
    GsCOORDINATE2*    a;
    GsCOORDINATE2*    b;
    POLY_G4*          prim;
    s32               i;
    s32               j;
    s32               i0;
    s32               i1;
    s32               hi;
    s32               lo;
    s32               fade;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(TonfaBeamScratch);
        blk                     = (TonfaBeamScratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_tonfa_baton_8012BBEC[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        b            = &D_tonfa_baton_8012BE6C[i0];
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        a            = &D_tonfa_baton_8012BBEC[i1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        b            = &D_tonfa_baton_8012BE6C[i1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps_real();
        prim           = (POLY_G4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt_real();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade = 0x40 - i * 9;
            hi   = fade & 0xFF;
            lo   = (fade - 9) & 0xFF;
            setRGB0(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB1(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB2(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            setRGB3(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            addPrim((u_long*)((((u32)(blk->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(TonfaBeamScratch);
}

void func_tonfa_baton_8011DA48(Task* arg0)
{
    TmdObject* extra;
    s32*       ptr;

    extra              = (TmdObject*)arg0->extra;
    ptr                = extra->field_8;
    arg0->state        = arg0->state + 1;
    arg0->exitCallback = WeaponsShared8011db78;
    *ptr               = 0;
    extra->field_C     = 0;
}

void func_tonfa_baton_8011DA74(Task* arg0)
{
    TmdObject*  extra;
    TonfaCoord* coord;
    GameActor*  actor;
    s32         mode;

    extra          = (TmdObject*)arg0->extra;
    coord          = (TonfaCoord*)extra->field_8;
    actor          = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    coord->flg     = 0;
    extra->field_C = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_C;

    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0x60;
    coord->coord.t[2] = 0;

    if (*(u32*)&actor->field_954 != 0x40000) {
        arg0->spawnArg1 = 0;
    }

    mode = arg0->spawnArg1 & 0xF;
    switch (mode) {
        case 0:
            if (coord->angle > 0) {
                coord->angle = coord->angle - 0x100;
            }
            break;
        case 1:
            if (coord->angle < 0x800) {
                coord->angle = coord->angle + 0x1C0;
            }
            break;
    }
    Gfx_RotMatrixZ(&coord->coord, coord->angle, 1);
}

void func_tonfa_baton_8011DB6C(Task* arg0)
{
    arg0->state = 3;
}
