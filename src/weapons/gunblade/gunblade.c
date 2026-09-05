#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "weapons/gunblade.h"

/// Package header word 0: the overlay id, `item id - 0x78`
/// (item 0x96, Gunblade).
const s32 D_gunblade_8011D1C0 = 0x1E;

extern s32 Gp_LcgState;

/// `rtps` / `rtpt`. The `inline_c.h` macros of those names assemble to
/// different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

void func_gunblade_8011D1E4(Task* task)
{
    GsCOORDINATE2  local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    GpEffWork*     eff;
    s32            keep;
    SVECTOR*       vec;
    s32            i;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        keep = Gp_State1C->field_4 < 4;
    } else {
        work->field_22++;
        switch (task->state) {
            case 0:
                coord->sub          = work->field_8;
                coord->coord.t[0]   = D_gunblade_8011E704[0].vx;
                D_gunblade_8012E244 = task;
                coord->coord.t[1]   = D_gunblade_8011E704[0].vy;
                D_gunblade_8012E248 = work;
                coord->coord.t[2]   = D_gunblade_8011E704[0].vz;
                coord->flg          = 0;
                Gp_UpdateCoord(coord);
                task->state      = 1;
                vec              = &D_gunblade_8011E704[1];
                local.sub        = work->field_8;
                local.coord.t[0] = vec->vx;
                local.coord.t[1] = vec->vy;
                local.coord.t[2] = vec->vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst        = &D_gunblade_8012E254[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_gunblade_8012E4D4[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                }
                return;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);
                local.sub        = work->field_8;
                local.coord.t[0] = D_gunblade_8011E70C.vx;
                local.coord.t[1] = D_gunblade_8011E70C.vy;
                local.coord.t[2] = D_gunblade_8011E70C.vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                dst        = &D_gunblade_8012E254[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_gunblade_8012E4D4[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_gunblade_8012E254[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_gunblade_8012E4D4[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                if (work->field_22 < 9) {
                    func_gunblade_8011D70C(work->field_22 & 7, 0x112);
                    return;
                }
                if (work->field_20 == 1) {
                    work->field_20++;
                    eff = Gp_SpawnEff(0x6029A, coord, task->spawnArg1, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->field_0);
                    }
                }
                func_gunblade_8011D70C(work->field_22 & 7, 0x331);
                keep = work->field_22 < 0xD;
                break;
            default:
                return;
        }
    }
    if (!keep) {
        D_gunblade_8012E248 = NULL;
        Gp_ReleaseState1CMem(work, task);
    }
}

void func_gunblade_8011D70C(s16 slot, s16 flags)
{
    GunbladeBeamScratch* blk;
    GsCOORDINATE2*       a;
    GsCOORDINATE2*       b;
    POLY_G4*             prim;
    s32                  i;
    s32                  j;
    s32                  i0;
    s32                  i1;
    s32                  hi;
    s32                  lo;
    s32                  fade;

    {
        register u8* tmp asm("v0");

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(GunbladeBeamScratch);
        blk                     = (GunbladeBeamScratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_gunblade_8012E254[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        b            = &D_gunblade_8012E4D4[i0];
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        a            = &D_gunblade_8012E254[i1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        b            = &D_gunblade_8012E4D4[i1];
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GunbladeBeamScratch);
}

/// Charge-up / blast flash for the gunblade's three shot grades
/// (`Task::spawnArg1` 13, 14 and 15). Frame 0 of each grade spawns the same
/// four effects with a grade-coloured parameter plus a burst of sparks, then
/// seeds the ring size (`field_24`), its spin (`field_26`), the arc size
/// (`field_28`) and the arc angle (`field_2A`). Every frame draws the ring at
/// twice the spin, then either the two crossing arcs and a full-screen fade
/// while the arc is still large, or shrinks the ring and releases the pool
/// block once it falls under 0x20. The three grades differ only in which RGB
/// channel gets the full brightness, so the tails are identical and the
/// compiler cross-jumps them.
void func_gunblade_8011DAA4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];
    s32            i;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;

    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(coord);
    work->field_22++;

    switch (task->spawnArg1) {
        case 13:
            if (task->state == 0) {
                Gp_SpawnEff(0x60076, coord, 0x600, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x10000, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x102AA, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x10555, NULL);
                for (i = 0; i < 8; i++) {
                    Gp_SpawnEff(0x60092, coord, 0, NULL);
                }
                task->state    = 1;
                work->field_24 = work->field_28 = 0xE0;
                work->field_26 = work->field_2A = 0x80;
            }
            rgb[0] = rgb[1] = work->field_24;
            rgb[2]          = (u16)work->field_24 >> 2;
            work->field_26 += 0x10;
            Gp_DrawRing(coord, (s16)(work->field_26 * 2), rgb);
            if (work->field_28 >= 0x11) {
                rgb[0] = rgb[1] = work->field_28;
                rgb[2]          = (u16)work->field_28 >> 2;
                Gp_DrawArc(coord, (s16)(work->field_2A * 3 / 2), 0x60, rgb);
                if (work->field_22 & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->field_2A * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->field_28 -= 0x10;
                work->field_2A += 0x40;
                return;
            }
            work->field_24 -= 0x20;
            if (work->field_24 < 0x20) {
                Gp_ReleaseState1CMem(work, task);
            }
            return;
        case 14:
            if (task->state == 0) {
                Gp_SpawnEff(0x60076, coord, 0x600, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x20000, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x202AA, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x20555, NULL);
                for (i = 0; i < 4; i++) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6007C, coord, (((u32)Gp_LcgState >> 16) & 0x3F) | 0x100, NULL);
                }
                task->state    = 1;
                work->field_24 = work->field_28 = 0xE0;
                work->field_26 = work->field_2A = 0x80;
            }
            rgb[0]          = work->field_24;
            rgb[1]          = (u16)work->field_24 >> 1;
            rgb[2]          = (u16)work->field_24 >> 2;
            work->field_26 += 0x10;
            Gp_DrawRing(coord, (s16)(work->field_26 * 2), rgb);
            if (work->field_28 >= 0x11) {
                rgb[0] = work->field_28;
                rgb[1] = (u16)work->field_28 >> 1;
                rgb[2] = (u16)work->field_28 >> 2;
                Gp_DrawArc(coord, (s16)(work->field_2A * 3 / 2), 0x60, rgb);
                if (work->field_22 & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->field_2A * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->field_28 -= 0x10;
                work->field_2A += 0x40;
                return;
            }
            work->field_24 -= 0x20;
            if (work->field_24 < 0x20) {
                Gp_ReleaseState1CMem(work, task);
            }
            return;
        case 15:
            if (task->state == 0) {
                Gp_SpawnEff(0x60076, coord, 0x600, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x30000, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x302AA, NULL);
                Gp_SpawnEff(0x600C1, coord, 0x30555, NULL);
                for (i = 0; i < 8; i++) {
                    Gp_SpawnEff(0x60092, coord, 1, NULL);
                }
                task->state    = 1;
                work->field_24 = work->field_28 = 0xE0;
                work->field_26 = work->field_2A = 0x80;
            }
            rgb[0]          = (u16)work->field_24 >> 2;
            rgb[1]          = (u16)work->field_24 >> 1;
            rgb[2]          = work->field_24;
            work->field_26 += 0x10;
            Gp_DrawRing(coord, (s16)(work->field_26 * 2), rgb);
            if ((s16)(u16)work->field_28 >= 0x11) {
                rgb[0] = (s16)(u16)work->field_28 >> 2;
                rgb[1] = (u16)work->field_28 >> 1;
                rgb[2] = work->field_28;
                Gp_DrawArc(coord, (s16)(work->field_2A * 3 / 2), 0x60, rgb);
                if (work->field_22 & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->field_2A * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->field_28 -= 0x10;
                work->field_2A += 0x40;
                return;
            }
            work->field_24 -= 0x20;
            if (work->field_24 < 0x20) {
                Gp_ReleaseState1CMem(work, task);
            }
            return;
    }
}

void func_gunblade_8011E008(s32 arg0)
{
    GpEffWork* work = D_gunblade_8012E248;

    if (work != NULL) {
        D_gunblade_8012E244->spawnArg1 = arg0;
        work->field_20++;
    }
}
