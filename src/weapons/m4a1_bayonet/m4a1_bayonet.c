#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/m4a1_bayonet.h"

/// Muzzle vector for the bayonet's thrust.
SVECTOR D_m4a1_bayonet_8011DEC8[1] = { { 0, 0x0300, 0x0040, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_m4a1_bayonet_8011DEC8[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
SVECTOR D_m4a1_bayonet_8011DED0 = { 0, 0x0180, 0x0040, 0 };

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// Per-frame task for the M4A1 bayonet's blade trail. Nothing runs once the
/// room is fading (`Gp_State1C->field_4` non-zero); the task is then released
/// as soon as that phase reaches 4. State 0 places the tip frame at
/// `D_m4a1_bayonet_8011DEC8[0]` under the muzzle and the hilt frame at
/// `[1]` under it, then seeds all sixteen trail slots with that pose. State 1
/// re-poses both frames every frame, writes them into trail slot
/// `field_22 & 7`, re-runs the whole ring so the older slots follow their
/// parents, and hands the ribbon to `func_m4a1_bayonet_8011D69C`. The task
/// lives 13 frames.
void func_m4a1_bayonet_8011D1E4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* slot;
    GsCOORDINATE2  hilt;
    s32            phase;
    SVECTOR*       vec;
    s32            vx;
    s32            vy;
    s32            vz;
    s32            i;
    s32            alive;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    phase = Gp_State1C->field_4;
    if (phase == 0) {
        work->field_22++;
        switch (task->state) {
            case 0:
                coord->sub        = work->field_8;
                coord->coord.t[0] = D_m4a1_bayonet_8011DEC8[0].vx;
                coord->coord.t[1] = D_m4a1_bayonet_8011DEC8[0].vy;
                coord->coord.t[2] = D_m4a1_bayonet_8011DEC8[0].vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->state = 1;

                vx              = D_m4a1_bayonet_8011DEC8[1].vx;
                vec             = &D_m4a1_bayonet_8011DEC8[1];
                vy              = vec->vy;
                vz              = vec->vz;
                hilt.sub        = coord;
                hilt.flg        = 0;
                hilt.coord.t[0] = vx;
                hilt.coord.t[1] = vy;
                hilt.coord.t[2] = vz;
                Gp_UpdateCoord(&hilt);

                for (i = 0; i < 8; i++) {
                    slot        = &D_m4a1_bayonet_8012D398[i];
                    slot->sub   = &Gfx_ViewCoord;
                    slot->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);

                    slot        = &D_m4a1_bayonet_8012D618[i];
                    slot->sub   = &Gfx_ViewCoord;
                    slot->workm = hilt.workm;
                    gte_SetRotMatrix(&hilt.workm);
                    gte_SetTransMatrix(&hilt.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);
                }
                break;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);

                hilt.sub        = work->field_8;
                hilt.flg        = 0;
                hilt.coord.t[0] = D_m4a1_bayonet_8011DED0.vx;
                hilt.coord.t[1] = D_m4a1_bayonet_8011DED0.vy;
                hilt.coord.t[2] = D_m4a1_bayonet_8011DED0.vz;
                Gp_UpdateCoord(&hilt);

                slot        = &D_m4a1_bayonet_8012D398[work->field_22 & 7];
                slot->sub   = &Gfx_ViewCoord;
                slot->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);

                slot        = &D_m4a1_bayonet_8012D618[work->field_22 & 7];
                slot->sub   = &Gfx_ViewCoord;
                slot->workm = hilt.workm;
                gte_SetRotMatrix(&hilt.workm);
                gte_SetTransMatrix(&hilt.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);

                for (i = 0; i < 8; i++) {
                    slot      = &D_m4a1_bayonet_8012D398[i];
                    slot->flg = 0;
                    Gp_UpdateCoord(slot);
                    slot      = &D_m4a1_bayonet_8012D618[i];
                    slot->flg = 0;
                    Gp_UpdateCoord(slot);
                }
                func_m4a1_bayonet_8011D69C(work->field_22 & 7, 0x112);
                break;
        }
        alive = work->field_22 < 0xD;
    } else {
        alive = phase < 4;
    }
    if (!alive) {
        Gp_ReleaseState1CMem(work, task);
    }
}

void func_m4a1_bayonet_8011D69C(s16 slot, s16 flags)
{
    M4a1BayonetBeamScratch* blk;
    GsCOORDINATE2*          a;
    GsCOORDINATE2*          b;
    POLY_G4*                prim;
    s32                     i;
    s32                     j;
    s32                     i0;
    s32                     i1;
    s32                     hi;
    s32                     lo;
    s32                     fade;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(M4a1BayonetBeamScratch);
        blk                     = (M4a1BayonetBeamScratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_m4a1_bayonet_8012D398[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        b            = &D_m4a1_bayonet_8012D618[i0];
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        a            = &D_m4a1_bayonet_8012D398[i1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        b            = &D_m4a1_bayonet_8012D618[i1];
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(M4a1BayonetBeamScratch);
}
