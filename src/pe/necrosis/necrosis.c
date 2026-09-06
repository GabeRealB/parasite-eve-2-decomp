#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/necrosis.h"

/// This overlay's id. Every package opens with one: a u16 in a u32
/// slot, distinct across all 448, with the families in contiguous blocks.
const u32 D_necrosis_8012EF30 = 51;

/// Per-level tuning for the necrosis burst: rows are PE levels 1-3, selected
/// by `field_20`. `field_0` is the `Gp_SpawnEff` draw parameter; `field_2` is
/// the last spawn-loop tick, and `field_2 + 0xC` the pad-rumble duration.
NecrosisStep D_necrosis_801306BC[] = {
    { 0x03C0, 0x000A },
    { 0x0480, 0x000F },
    { 0x0540, 0x0014 },
};

/// The `SndEvt_EnqueueType6` id for each `D_necrosis_801306BC` row.
s32 D_necrosis_801306C8[] = { 0xE0150001, 0xE0180001, 0xE01B0001 };

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0` / `gpf 1`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_necrosis_8012F6EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_necrosis_8012FE64(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_necrosis_80130288(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Runs one frame of the necrosis cast. State 0 copies the player rotation onto
/// the effect coordinate, rotates a (0, 0, 0x90) offset into that frame, and
/// links a `NecrosisWork` collision pair (list 1 + list 7) whose packed id is
/// the combo digits plus `0x28000`. State 1 GPF-scales that offset by 0x1100
/// each frame, walks the coordinate, and spawns `0x80060019`; a `0x100000` hit
/// on `obj2` zeros the offset and unlinks the list-7 object. State 2 waits
/// `field_2 + 0x10` ticks. Any state releases if the player is dying
/// (`Gp_StateC08.field_3` / `D_80114C0B`) or the room is fading (`Gp_State1C`).
void func_necrosis_8012EF34(Task* arg0)
{
    NecrosisWork*  work;
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    GpMtxWords*    dstm;
    GpMtxWords*    srcm;
    GpRec18*       rec;
    GpEffWork*     spawned;
    s32            pan;
    u16            old;
    s32            tick;
    s16            fade;

    work          = (NecrosisWork*)arg0->idMap;
    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    old           = mem->field_22;
    tick          = old + 1;
    mem->field_22 = tick;
    SOFT_USE_REG(tick);
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_3 == -2) {
                goto release;
            }
            fade = Gp_State1C->field_E;
            if (fade >= 4) {
                goto release;
            }
            if (fade != 0) {
                mem->field_22 = old;
                return;
            }
            work = Mem_Calloc(0x58, 0);
            if (work == NULL) {
                mem->field_22 = 0;
                return;
            }
            player     = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
            dstm       = (GpMtxWords*)&coord->coord;
            srcm       = (GpMtxWords*)&player->coord;
            dstm->w0   = srcm->w0;
            dstm->w1   = srcm->w1;
            dstm->w2   = srcm->w2;
            dstm->w3   = srcm->w3;
            dstm->h4   = srcm->h4;
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            mem->field_10 = 0;
            mem->field_12 = 0;
            mem->field_14 = 0x90;
            gte_SetRotMatrix((MATRIX*)srcm);
            gte_ldv0(&mem->field_10);
            gte_rtv0_real();
            gte_stsv(&mem->field_10);
            rec               = &work->rec;
            mem->field_20     = (Gp_StateC08.field_0 % 10) - 1;
            arg0->idMap       = (TaskIdMap*)work;
            work->obj.field_8 = coord;
            work->obj.field_C = rec;
            work->obj.field_18 =
                ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 + ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 + (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
            work->obj.field_1C = D_necrosis_801306BC[mem->field_20].field_0;
            work->obj.flags    = 1;
            Gp_LinkObj(1, &work->obj);
            rec->field_0        = 2;
            work->obj2.field_8  = coord;
            work->obj2.field_C  = rec;
            work->obj2.field_18 = 0;
            work->obj2.field_1C = 0x80;
            work->obj2.flags    = 1;
            work->obj.flags    |= 0x8000;
            Gp_LinkObj(7, &work->obj2);
            work->obj2.flags = (work->obj2.flags & 0x7FFF) | 0x4400;
            pan              = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(D_necrosis_801306C8[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)Gp_GetObjDepth((GpObj38*)coord));
            Gp_SpawnPadLerp((s16)((u16)D_necrosis_801306BC[mem->field_20].field_2 + 0xC), 0xFF, 8);
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (Gp_State1C->field_E == 0) {
                gte_lddp(0x1100);
                gte_ldsv(&mem->field_10);
                gte_gpf12_real();
                gte_stsv(&mem->field_10);
                coord->coord.t[0] += mem->field_10;
                coord->coord.t[1] += mem->field_12;
                coord->coord.t[2] += mem->field_14;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                spawned = Gp_SpawnEff(0x80060019, coord,
                                      (s16)D_necrosis_801306BC[mem->field_20].field_0 + ((s16)mem->field_22 * 0x60),
                                      NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
                work->obj.field_1C = (u16)work->obj.field_1C + 0x20;
            } else {
                mem->field_22 = (u16)mem->field_22 - 1;
            }
            if ((D_80114C0B == -2) || (Gp_State1C->field_E >= 4)) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                goto release;
            }
            if ((s16)mem->field_22 > D_necrosis_801306BC[mem->field_20].field_2) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            if (Gp_FindRec18(work->obj2.field_C, 0x100000) != 0) {
                mem->field_10 = 0;
                mem->field_12 = 0;
                mem->field_14 = 0;
                Gp_UnlinkObj(&work->obj2);
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 2:
            if (D_80114C0B == -2) {
                goto release;
            }
            if (Gp_State1C->field_E >= 4) {
                goto release;
            }
            tick = (s16)tick;
            if ((D_necrosis_801306BC[mem->field_20].field_2 + 0x10) < tick) {
            release:
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

void func_necrosis_8012F52C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            rng;
    s32            val;
    s32            step;
    GpEffWork*     spawned;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_E != 0) {
        return;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        mem->field_24 = (u16)arg0->spawnArg1 & 0xFFF;
        rng           = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng;
        mem->field_26 = ((u32)rng >> 16) & 0xFFF;
        SOFT_COMPILER_BARRIER();
        val           = (u16)mem->field_24;
        step          = val;
        val           = val - 0x100;
        mem->field_28 = val;
        mem->field_2A = (s32)(step << 16) >> 20;
        arg0->state   = 1;
    }
    Gp_UpdateCoord(coord);
    func_necrosis_8012F6EC(coord, (s16)(mem->field_22 % 6), mem->field_24, mem->field_26);
    mem->field_24 = (u16)mem->field_24 - (u16)mem->field_2A;
    if ((s16)mem->field_24 < mem->field_2A) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if ((s16)(mem->field_22 % 3) == 0) {
        spawned = Gp_SpawnEff(0x6001A, coord, mem->field_28, 0);
        if (spawned != NULL) {
            Task_Reparent(arg0, spawned->field_0);
        }
    }
}

/// Draws one frame of the necrosis spray sprite. `arg0`'s world position is
/// projected through `GsWSMATRIX` by a single `RTPS` and the quad is dropped
/// when that sets a negative `gte_stflg`. `arg1` picks one of the 0x28-wide
/// texture frames on tpage 0x2A (CLUT 0x428F), `arg3` spins the quad and
/// `arg2` sizes it: the corners sit `arg2 * 39 / otz` from the projected
/// centre along `arg3` and `arg3 + 0x400`, so the sprite shrinks with depth.
/// Same shape as `Gp_DrawFxQuad` with a wider texture cell and no CLUT table.
void func_necrosis_8012F6EC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        prim->clut  = 0x428F;
        u0          = arg1 * 0x28;
        u1          = u0 + 0x27;
        setUV4(prim, u0, 0x38, u1, 0x38, u0, 0x5F, u1, 0x5F);
        block->dx = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 39) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

void func_necrosis_8012FAF8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            tick;
    s32            rng1;
    s32            rng2;
    s32            rng3;
    s32            temp_lo;
    s32            var_v1;
    u16            temp_v0;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_E != 0) {
        return;
    }

    mem->field_22 = (u16)mem->field_22 + 1;
    switch (arg0->state) {
        case 0:
            mem->field_22 = 0;
            temp_v0       = arg0->spawnArg1;
            mem->field_28 = temp_v0 & 0xFFF;
            rng1          = (Gp_LcgState * 5) + 0x71357911;
            mem->field_24 = ((u32)rng1 >> 16) & 0xFFF;
            Gp_LcgState   = rng1;
            mem->field_26 = mem->field_28 / 20;
            mem->field_10 = (rsin(mem->field_24) * mem->field_26) >> 12;
            temp_lo       = rcos(mem->field_24) * mem->field_26;
            rng2          = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState   = rng2;
            mem->field_12 = temp_lo >> 12;
            mem->field_14 = (rsin(((u32)rng2 >> 16) & 0xFFF) * mem->field_10) >> 12;
            rng3          = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState   = rng3;
            if ((s32)(((u32)rng3 >> 16) & 3) < ((u16)(Gp_StateC08.field_0 % 10U) - 1)) {
                mem->field_2A = 0x1000;
            }
            if ((u16)(Gp_StateC08.field_0 % 10U) - 1 < 2) {
                arg0->state = 1;
                return;
            }
            var_v1 = 2;
            if (mem->field_2A != 0) {
                var_v1 = 1;
            }
            arg0->state = var_v1;
            return;
        case 1:
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            tick          = (u16)mem->field_20 + 1;
            mem->field_20 = tick;
            if (tick < 8) {
                func_necrosis_8012FE64(coord, (s16)(tick | (u16)mem->field_2A), mem->field_28,
                                       mem->field_24);
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        case 2:
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            tick          = (u16)mem->field_20 + 1;
            mem->field_20 = tick;
            if (tick < 6) {
                func_necrosis_80130288(coord, (s16)(tick | (u16)mem->field_2A), mem->field_28,
                                       mem->field_24);
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

/// Draws one frame of the necrosis mist puff. `arg0`'s world position is
/// projected through `GsWSMATRIX` by a single `RTPS` and the quad is dropped
/// when that sets a negative `gte_stflg`. `arg1` is packed by the caller: the
/// low nibble picks one of the 0x20-wide texture cells on row 0x18..0x37, and
/// bit 0x1000 swaps the pale tpage/CLUT pair (0x2A / 0x428F) for the dark one
/// (0x4A / 0x42C2). `arg3` spins the quad and `arg2` sizes it: the corners sit
/// `arg2 * 31 / otz` from the projected centre along `arg3` and `arg3 + 0x400`,
/// so the puff shrinks with depth. Same shape as `func_necrosis_8012F6EC`.
void func_necrosis_8012FE64(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    register void**  scratch asm("a1");
    register s16     frame asm("a1");
    u8*              head;
    s32              u1;
    s32              ang;
    register s32     sinArg asm("a0");
    s32              ang2;
    u16              vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *(u8* volatile*)scratch;
    {
        register u16 vx asm("v0");
        vx                                        = *(volatile u16*)&arg0->workm.t[0];
        ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (GpFxQuadScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    frame         = arg1;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        if (arg1 & 0x1000) {
            prim->tpage = 0x2A;
            prim->clut  = 0x428F;
        } else {
            prim->tpage = 0x4A;
            prim->clut  = 0x42C2;
        }
        ang    = arg3;
        sinArg = ang;
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        {
            register s32 cell asm("v0");
            cell = (frame & 0xF) << 5;
            u1   = cell + 0x1F;
            setUV4(prim, cell, 0x18, u1, 0x18, cell, 0x37, u1, 0x37);
        }
        block->dx = (((arg2 * 31) / block->otz) * rsin(sinArg)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws one frame of the necrosis spore cloud. Same shape as
/// `func_necrosis_8012FE64`: `arg0`'s world position is projected through
/// `GsWSMATRIX` by a single `RTPS` and the quad is dropped when that sets a
/// negative `gte_stflg`. `arg1` is packed by the caller: the low nibble picks
/// one of the 0x28-wide texture cells on row 0x50..0x77, and bit 0x1000 swaps
/// the dark tpage/CLUT pair (0x49 / 0x42C2) for the pale one (0x29 / 0x428F).
/// `arg3` spins the quad and `arg2` sizes it: the corners sit `arg2 * 39 / otz`
/// from the projected centre along `arg3` and `arg3 + 0x400`, so the cloud
/// shrinks with depth.
void func_necrosis_80130288(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    void**           scratch;
    u8*              head;
    s32              u1;
    s32              ang2;
    u16              vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *(u8* volatile*)scratch;
    {
        register u16 vx asm("v0");
        vx                                        = *(volatile u16*)&arg0->workm.t[0];
        ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (GpFxQuadScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyFT4(prim);
        if (arg1 & 0x1000) {
            prim->tpage = 0x29;
            prim->clut  = 0x428F;
        } else {
            prim->tpage = 0x49;
            prim->clut  = 0x42C2;
        }
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        {
            s32 cell;
            cell = (arg1 & 0xF) * 40;
            u1   = cell + 0x27;
            setUV4(prim, cell, 0x50, u1, 0x50, cell, 0x77, u1, 0x77);
        }
        block->dx = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 39) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
