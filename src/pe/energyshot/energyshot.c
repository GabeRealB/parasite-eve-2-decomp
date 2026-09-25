#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/energyshot.h"

#include <psyq/inline_c.h>
#include "gte.h"

/// Per-level tuning for the energy shot: rows are PE levels 1-3.
EnergyShotScale D_energyshot_801300E4[] = {
    { 0x0008, 0x0090, 0x0005, 0x0400 },
    { 0x000C, 0x00C0, 0x0006, 0x0500 },
    { 0x0010, 0x00F0, 0x0007, 0x0600 },
};

/// The `SndEvt_EnqueueType6` id for each `D_energyshot_801300E4` row.
s32 D_energyshot_801300FC[] = { 0xE02A0001, 0xE02D0001, 0xE0300001 };

void func_energyshot_8012FA50(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u8* arg3);
void func_energyshot_8012F750(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Two scratch rings the shot walks while in flight.
s16 D_energyshot_80130108[16] = { 0 };
s16 D_energyshot_80130128[16] = { 0 };

/// Energy shot PE. `Task::spawnArg2` is the `GpEffWork` block; `Task::extra`
/// reaches the coordinate. Cancel (`Gp_StateC08.field_3 == -2` or
/// `Gp_State1C->fadeState >= 4`) releases the work block.
///
/// State 0 parents the coordinate, seeds 16 texture-frame offsets and 16 wedge
/// yaws from `Gp_LcgState`, and plays the combo-indexed cue. State 1 grows
/// brightness / radius, draws three rings plus `field_0` wedges and the beam,
/// and parents a `0x600F4` spark; once brightness exceeds the row cap it
/// advances to state 2, which shrinks brightness until it drops below 0x11.
void func_energyshot_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            i;
    u8             rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if ((state->field_3 != -2) && (Gp_State1C->fadeState < 4)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0: {
                GpMtxWords* rot;
                GpState1C*  st1c;
                s16         count;
                u16         level;

                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->parent;
                rot->m00_m01      = 0x1000;
                rot->m02_m10      = 0;
                rot->m11_m12      = 0x1000;
                rot->m20_m21      = 0;
                rot->m22          = 0x1000;
                coord->coord.t[2] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[0] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                state->field_6    |= 8;
                st1c               = Gp_State1C;
                st1c->burstRequest = 0;
                st1c->peFxFlags   &= 0xFBFF;
                arg0->state        = 1;
                mem->index         = (Gp_StateC08.field_0 % 10) - 1;
                i                  = 0;
                {
                    s16* frames;

                    frames = D_energyshot_80130108;
                    do {
                        s32 rng;

                        i          += 1;
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        *frames     = ((u32)rng >> 16) & 0xFF;
                        frames     += 1;
                        Gp_LcgState = rng;
                    } while (i < 0x10);
                }
                i = 0;
                {
                    EnergyShotScale* tbl;

                    tbl   = D_energyshot_801300E4;
                    count = tbl[mem->index].field_0;
                    level = mem->index;
                    if (count > 0) {
                        do {
                            s32 lo;
                            s32 rng;

                            lo                       = i * (0x1000 / D_energyshot_801300E4[(s16)level].field_0);
                            rng                      = Gp_LcgState * 5 + 0x71357911;
                            D_energyshot_80130128[i] = lo + (((u32)rng >> 16) & 0x1FF);
                            i                       += 1;
                            Gp_LcgState              = rng;
                            count                    = D_energyshot_801300E4[mem->index].field_0;
                            level                    = mem->index;
                        } while (i < count);
                    }
                }
                {
                    s32 pan;

                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(D_energyshot_801300FC[mem->index], pan,
                                        (s8)gpGetObjDepth(coord));
                }
                SOFT_USE_REG(arg0);
                return;
            }
            case 1: {
                EnergyShotScale* table;
                EnergyShotScale* t2;
                s32              rng;
                s16              ang;
                s16*             p;
                s16              count;

                table             = D_energyshot_801300E4;
                mem->scale        = mem->scale + table[mem->index].field_4;
                rgb[0]            = *(u8*)&mem->scale;
                rgb[1]            = mem->scale >> 1;
                rgb[2]            = *(u8*)&mem->scale;
                coord->coord.t[1] = -table[mem->index].field_6;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(mem->scale * 4), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 8), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 0xC), rgb);
                i     = 0;
                count = table[mem->index].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        func_energyshot_8012F750(coord, (s16)(mem->scale * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].field_0);
                }
                coord->coord.t[1] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        func_energyshot_8012FA50(coord, (s16)(mem->scale * 8),
                                                 (s16)(u16)D_energyshot_801300E4[2].field_6 >> 1, rgb);
                    }
                    func_energyshot_8012FA50(
                        coord, (s16)(mem->scale * 4),
                        (u16)D_energyshot_801300E4[mem->index].field_6 * 2, rgb);
                }
                func_energyshot_8012FA50(
                    coord, (s16)(mem->scale * 6),
                    (u16)D_energyshot_801300E4[mem->index].field_6 - 0x100, rgb);
                rng          = Gp_LcgState * 5 + 0x71357911;
                ang          = ((u32)rng >> 16) & 0xFFF;
                Gp_LcgState  = rng;
                mem->angle   = ang;
                mem->move.vx = (u32)(rsin(ang) * mem->scale * 3) >> 11;
                mem->move.vz = (u32)(rcos(mem->angle) * mem->scale * 3) >> 11;
                Gp_SpawnEff(0x600F4, coord,
                            D_energyshot_801300E4[mem->index].field_6 | 0x8000,
                            &mem->move);
                if (D_energyshot_801300E4[mem->index].field_2 < mem->scale) {
                    Gp_SpawnEff(0x800600F3, coord, 0, 0);
                    mem->period = mem->scale;
                    arg0->state = 2;
                }
                return;
            }
            case 2: {
                EnergyShotScale* table;
                EnergyShotScale* t2;
                s16*             p;
                s16              count;

                if (mem->scale < 0x11) {
                    goto release;
                }
                mem->scale        = mem->scale - 0x10;
                rgb[0]            = *(u8*)&mem->scale;
                rgb[1]            = mem->scale >> 1;
                rgb[2]            = *(u8*)&mem->scale;
                table             = D_energyshot_801300E4;
                coord->coord.t[1] = -table[mem->index].field_6;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 4), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 8), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].field_2 * 0xC), rgb);
                i     = 0;
                count = table[mem->index].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        func_energyshot_8012F750(coord, (s16)(mem->period * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].field_0);
                }
                coord->coord.t[1] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        mem->period =
                            mem->period + D_energyshot_801300E4[2].field_4;
                        func_energyshot_8012FA50(
                            coord, (s16)(mem->period * 8),
                            (s16)(u16)D_energyshot_801300E4[mem->index].field_6 >> 1,
                            rgb);
                    }
                    func_energyshot_8012FA50(
                        coord, (s16)(mem->period * 4),
                        (u16)D_energyshot_801300E4[mem->index].field_6 * 2, rgb);
                }
                func_energyshot_8012FA50(
                    coord, (s16)(mem->period * 6),
                    (u16)D_energyshot_801300E4[mem->index].field_6 - 0x100, rgb);
                return;
            }
        }
        return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws one wedge of the drain funnel as a Gouraud triangle. `arg0`'s origin
/// is projected once through `GsWSMATRIX`; the two outer corners sit `arg1`
/// screen units away at `arg2 - 0x20` and `arg2 + 0x20`, so the wedge is a
/// 0x40-wide fan blade about `arg2`. Only the apex carries `rgb`, the rim
/// fading to black. A negative `gte_stflg` drops the wedge.
void func_energyshot_8012F750(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    u8*            head;
    GpRingScratch* block;
    SVECTOR*       vec;
    POLY_G3*       prim;
    s32            ang;
    s32            ang2;
    u16            vz;

    head                                    = SCRATCH_HEAD(u8);
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
    SCRATCH_HEAD(GpRingScratch)             = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_G3*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setRGB0(prim, rgb[0], rgb[1], rgb[2]);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->step = ((s16)arg1 * 128) / block->otz;
        ang         = (s16)arg2;
        ang2        = ang - 0x20;
        prim->x0    = *(u16*)&block->sx;
        prim->y0    = *(u16*)&block->sy;
        prim->x1    = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
        prim->y1    = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
        ang        += 0x20;
        prim->x2    = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
        prim->y2    = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws the energy shot's beam: an inner ring of radius `arg1 + 0x400` sunk
/// `arg2` along local Y and an outer ring of radius `arg1 / 2 + 0x100` in the
/// local XY plane are built by `rsin` / `rcos`, rotated by `arg0`'s `workm`
/// and offset by its translation, then each of the 16 segments is projected
/// through `GsWSMATRIX` as one semi-transparent `POLY_FT4`. The texture cell
/// is one of six 0x28-wide frames picked per vertex by `D_energyshot_80130108`
/// plus the frame counter, the quad is tinted by the three bytes at `arg3`,
/// and a negative `gte_stflg` drops the segment.
void func_energyshot_8012FA50(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u8* arg3)
{
    u8*            head;
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_FT4*      prim;
    s32            i;
    s32            next;
    s32            ang;
    s32            u;
    s16            idx;
    s16            r0;
    s16            r1;

    r1                 = arg1 / 2 + 0x100;
    r0                 = arg1 + 0x400;
    head               = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(void) = head - 0x118;
    block              = (GpBandScratch*)(head - 0x118);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -arg2;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg0->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg0->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        idx = (u32)(D_energyshot_80130108[i] + gDisplayState.animFrame) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setRGB0(prim, arg3[0], arg3[1], arg3[2]);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x118);
}

void func_energyshot_8012FFB8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;

    mem      = arg0->spawnArg2;
    coord    = arg0->extra.tmd->coords;
    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        mem->move.vx = 0;
        mem->move.vz = 0;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vy = 0xFFF0 - (((u32)Gp_LcgState >> 16) & 0x3F);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
        arg0->state  = 1;
    }

    y                 = coord->coord.t[1] + mem->move.vy;
    coord->flg        = 0;
    coord->coord.t[1] = y;
    Gp_UpdateCoord(coord);
    if ((mem->age & 3) == 0) {
        mem->index = mem->index + 1;
    }
    if (mem->index < 8) {
        Gp_DrawFxQuad(coord, mem->index, 0x400, mem->scale);
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}
