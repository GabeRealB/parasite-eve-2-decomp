#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "weapons/gunblade.h"

/// Muzzle vector for the gunblade's blade sweep.
SVECTOR D_gunblade_8011E704[1] = { { 0, 0x0060, 0x0080, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_gunblade_8011E704[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
SVECTOR D_gunblade_8011E70C = { 0, 0x0060, 0x0380, 0 };

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
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        keep = Gp_State1C->eventState < 4;
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                coord->sub          = work->parent;
                coord->coord.t[0]   = D_gunblade_8011E704[0].vx;
                D_gunblade_8012E244 = task;
                coord->coord.t[1]   = D_gunblade_8011E704[0].vy;
                D_gunblade_8012E248 = work;
                coord->coord.t[2]   = D_gunblade_8011E704[0].vz;
                coord->flg          = 0;
                Gp_UpdateCoord(coord);
                task->state      = 1;
                vec              = &D_gunblade_8011E704[1];
                local.sub        = work->parent;
                local.coord.t[0] = vec->vx;
                local.coord.t[1] = vec->vy;
                local.coord.t[2] = vec->vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst        = &D_gunblade_8012E254[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_gunblade_8012E4D4[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                return;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);
                local.sub        = work->parent;
                local.coord.t[0] = D_gunblade_8011E70C.vx;
                local.coord.t[1] = D_gunblade_8011E70C.vy;
                local.coord.t[2] = D_gunblade_8011E70C.vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                dst        = &D_gunblade_8012E254[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_gunblade_8012E4D4[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_gunblade_8012E254[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_gunblade_8012E4D4[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                if (work->age < 9) {
                    func_gunblade_8011D70C(work->age & 7, 0x112);
                    return;
                }
                if (work->index == 1) {
                    work->index++;
                    eff = Gp_SpawnEff(0x6029A, coord, task->spawnArg1, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->task);
                    }
                }
                func_gunblade_8011D70C(work->age & 7, 0x331);
                keep = work->age < 0xD;
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
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
            addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GunbladeBeamScratch);
}

/// Charge-up / blast flash for the gunblade's three shot grades
/// (`Task::spawnArg1` 13, 14 and 15). Frame 0 of each grade spawns the same
/// four effects with a grade-coloured parameter plus a burst of sparks, then
/// seeds the ring size (`scale`), its spin (`angle`), the arc size
/// (`period`) and the arc angle (`step`). Every frame draws the ring at
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

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(coord);
    work->age++;

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
                task->state = 1;
                work->scale = work->period = 0xE0;
                work->angle = work->step = 0x80;
            }
            rgb[0] = rgb[1] = work->scale;
            rgb[2]          = (u16)work->scale >> 2;
            work->angle    += 0x10;
            Gp_DrawRing(coord, (s16)(work->angle * 2), rgb);
            if (work->period >= 0x11) {
                rgb[0] = rgb[1] = work->period;
                rgb[2]          = (u16)work->period >> 2;
                Gp_DrawArc(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
                if (work->age & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->step * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->period -= 0x10;
                work->step   += 0x40;
                return;
            }
            work->scale -= 0x20;
            if (work->scale < 0x20) {
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
                task->state = 1;
                work->scale = work->period = 0xE0;
                work->angle = work->step = 0x80;
            }
            rgb[0]       = work->scale;
            rgb[1]       = (u16)work->scale >> 1;
            rgb[2]       = (u16)work->scale >> 2;
            work->angle += 0x10;
            Gp_DrawRing(coord, (s16)(work->angle * 2), rgb);
            if (work->period >= 0x11) {
                rgb[0] = work->period;
                rgb[1] = (u16)work->period >> 1;
                rgb[2] = (u16)work->period >> 2;
                Gp_DrawArc(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
                if (work->age & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->step * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->period -= 0x10;
                work->step   += 0x40;
                return;
            }
            work->scale -= 0x20;
            if (work->scale < 0x20) {
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
                task->state = 1;
                work->scale = work->period = 0xE0;
                work->angle = work->step = 0x80;
            }
            rgb[0]       = (u16)work->scale >> 2;
            rgb[1]       = (u16)work->scale >> 1;
            rgb[2]       = work->scale;
            work->angle += 0x10;
            Gp_DrawRing(coord, (s16)(work->angle * 2), rgb);
            if ((s16)(u16)work->period >= 0x11) {
                rgb[0] = (s16)(u16)work->period >> 2;
                rgb[1] = (u16)work->period >> 1;
                rgb[2] = work->period;
                Gp_DrawArc(coord, (s16)(work->step * 3 / 2), 0x60, rgb);
                if (work->age & 1) {
                    Gp_DrawArc(coord, 0x60, (s16)(work->step * 3 / 2), rgb);
                }
                Gp_DrawFadeQuad(rgb, 1);
                work->period -= 0x10;
                work->step   += 0x40;
                return;
            }
            work->scale -= 0x20;
            if (work->scale < 0x20) {
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
        work->index++;
    }
}
