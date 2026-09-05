#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/acropolis_helicopter_landing_pad.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. Same reason as above for spelling out the word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
extern s8        D_8007106B;
extern s32       D_80070F70;
extern s32       Gp_LcgState;
extern SVECTOR   D_acropolis_helicopter_landing_pad_80184E80[12];
extern s32       D_acropolis_helicopter_landing_pad_80184EE0[12];
extern AhlpLight D_80115188[2];
extern TaskDesc  D_acropolis_helicopter_landing_pad_80184E68;
extern GpSaveLoc D_acropolis_helicopter_landing_pad_80187F90;
void             func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0);
void             func_acropolis_helicopter_landing_pad_8017EE2C(Task* arg0);
void             func_acropolis_helicopter_landing_pad_8017F010(SVECTOR* pos, s16 index, s32 level);
void             func_acropolis_helicopter_landing_pad_80180664(GsCOORDINATE2* coord);
void             func_acropolis_helicopter_landing_pad_80180A64(GsCOORDINATE2* coord);

void func_acropolis_helicopter_landing_pad_8017ED00(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_helicopter_landing_pad_80184E68, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

/// Asks the slot-7 task to warp to stage 0xF, room 3 (message 0x13EE with the
/// room's `GpSaveLoc`); advances on success, otherwise kills the task.
void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0)
{
    Task* slot = Game_GetPtrSlot(7);

    D_acropolis_helicopter_landing_pad_80187F90.field_4 = 1;
    D_acropolis_helicopter_landing_pad_80187F90.field_3 = 1;
    *(u16*)&D_acropolis_helicopter_landing_pad_80187F90 = 0xF;
    D_acropolis_helicopter_landing_pad_80187F90.field_2 = 3;
    D_acropolis_helicopter_landing_pad_80187F90.field_5 = 0;
    if (Gp_DispatchMsg(slot, 0x13EE, (s32)&D_acropolis_helicopter_landing_pad_80187F90,
                       (s32)&D_acropolis_helicopter_landing_pad_80187F90) != 0) {
        arg0->state += 1;
    } else {
        Task_Kill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EDD4(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    slot         = Game_GetPtrSlot(3);
    msg.field_10 = 0;
    msg.field_12 = 0;
    msg.field_14 = 0;
    Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_6", func_acropolis_helicopter_landing_pad_8017EE2C);

void func_acropolis_helicopter_landing_pad_8017EE80(Task* arg0)
{
    s32   args[2];
    Task* slot;

    slot    = Game_GetPtrSlot(3);
    args[0] = 1;
    args[1] = 3;
    Gp_DispatchMsg(slot, 0x3EF, (s32)args, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_helicopter_landing_pad_8017EEDC(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        Mc_SaveData.field_6 = D_acropolis_helicopter_landing_pad_80187F90.field_0;
        Mc_SaveData.field_8 = D_acropolis_helicopter_landing_pad_80187F90.field_2;
        Mc_SaveData.field_5 = D_acropolis_helicopter_landing_pad_80187F90.field_3;
        Task_Spawn(0, 0x11, 0, 0);
        Task_Kill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EF60(void)
{
    Task_Spawn(2, 0xF, 0, 0);
}

/// Five-state dispatcher of the room's intro task; the handler table is built
/// on the stack. Marks the player actor's `field_930` as 2 before every step.
void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0)
{
    GameActor* actor     = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    TaskFunc   states[5] = {
        func_acropolis_helicopter_landing_pad_8017ED50,
        func_acropolis_helicopter_landing_pad_8017EDD4,
        func_acropolis_helicopter_landing_pad_8017EE2C,
        func_acropolis_helicopter_landing_pad_8017EE80,
        func_acropolis_helicopter_landing_pad_8017EEDC,
    };

    actor->field_930 = 2;
    states[arg0->state](arg0);
}

/// Draws one helipad floodlight glow. Light `index` owns record `index & 1`
/// of `D_80115188`; the light is skipped while `Gp_State1C::field_4` is
/// non-zero (clearing the record's `state` once it reaches 4) and unless the
/// current view's bit is set in the light's
/// `D_acropolis_helicopter_landing_pad_80184EE0` mask. Otherwise `pos` is
/// projected through `Gfx_ViewWorldMtx` into a `G_SCRATCH_HEAD` block and,
/// when the GTE flag word is clean, the record is refreshed and two rings of
/// flat-shaded `POLY_G4` fans are linked into the OT at the light's `otz`: 16
/// wedges of the outer radius (a dim `level >> 1` layer under a `level` one)
/// and four inner-radius blades whose intensity is `level >> 1`.
void func_acropolis_helicopter_landing_pad_8017F010(SVECTOR* pos, s16 index, s32 level)
{
    AhlpLight*        light;
    AhlpLightWork*    work;
    void**            scratch;
    u8*               head;
    AhlpLightScratch* blk;
    POLY_G4*          prim;
    s32               a;
    s32               b;
    s32               c;
    s32               d;
    s16               lvl;
    s32               half;
    s32               mask;

    lvl   = level;
    light = &D_80115188[index & 1];
    work  = &light->work;
    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            light->state = 0;
        }
    } else {
        mask = D_acropolis_helicopter_landing_pad_80184EE0[index] & (1 << ((Gp_GetViewIndex() & 0xFF) - 1));
        if (mask == 0) {
            return;
        }
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        *scratch = head - 0x14;
        blk      = (AhlpLightScratch*)(head - 0x14);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(pos);
        gte_rtps_real();
        gte_stsxy(&((AhlpLightScratch*)(head - 0x14))->sx);
        gte_stflg(&((AhlpLightScratch*)(head - 0x14))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            light->state        = 2;
            work->field_58      = 0x640;
            work->field_5C      = 0x3200;
            work->field_50      = level * 16;
            work->field_52      = 0;
            work->field_54      = 0;
            work->x             = pos->vx;
            work->y             = pos->vy;
            work->z             = pos->vz;
            light->work.field_0 = 0;
            blk->outer          = 0xC000 / ((AhlpLightScratch*)(head - 0x14))->otz;
            blk->inner          = 0x1800 / ((AhlpLightScratch*)(head - 0x14))->otz;

            for (a = 0; a < 0x1000; a += 0x200) {
                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                half = lvl >> 1;
                setRGB2(prim, half, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->outer * rsin(a)) >> 12);
                prim->y0 = blk->sy + ((blk->outer * rcos(a)) >> 12);
                b        = a + 0x100;
                prim->x1 = blk->sx + ((blk->outer * rsin(b)) >> 12);
                prim->y1 = blk->sy + ((blk->outer * rcos(b)) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                c        = a + 0x200;
                prim->x3 = blk->sx + ((blk->outer * rsin(c)) >> 12);
                prim->y3 = blk->sy + ((blk->outer * rcos(c)) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->outer * rsin(a)) >> 13);
                prim->y0 = blk->sy + ((blk->outer * rcos(a)) >> 13);
                prim->x1 = blk->sx + ((blk->outer * rsin(b)) >> 13);
                prim->y1 = blk->sy + ((blk->outer * rcos(b)) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->outer * rsin(c)) >> 13);
                prim->y3 = blk->sy + ((blk->outer * rcos(c)) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }

            lvl = half;
            for (a = 0x200; a < 0x1000; a += 0x800) {
                d              = a - 0x400;
                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->inner * rsin(d)) >> 13);
                prim->y0 = blk->sy + ((blk->inner * rcos(d)) >> 13);
                prim->x1 = blk->sx + ((blk->outer * rsin(a)) >> 12);
                prim->y1 = blk->sy + ((blk->outer * rcos(a)) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                d        = a + 0x400;
                prim->x3 = blk->sx + ((blk->inner * rsin(d)) >> 13);
                prim->y3 = blk->sy + ((blk->inner * rcos(d)) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)Gpu_PrimCursor;
                Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
                setlen(prim, 8);
                setcode(prim, 0x38);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->inner * rsin(a)) >> 12);
                prim->y0 = blk->sy + ((blk->inner * rcos(a)) >> 12);
                prim->x1 = blk->sx + ((blk->outer * rsin(d)) >> 11);
                prim->y1 = blk->sy + ((blk->outer * rcos(d)) >> 11);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                d        = a + 0x800;
                prim->x3 = blk->sx + ((blk->inner * rsin(d)) >> 12);
                prim->y3 = blk->sy + ((blk->inner * rcos(d)) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    }
}

/// Draws one helipad ember / spark sprite. `spawnArg1` non-zero spawns the
/// bright variant (`field_24` 0x300..0x3FF, no drift beyond a fixed -0x18 on
/// Y, `field_2A` 2..5 with `field_28` up to 0x3F); zero spawns the dim one
/// (`field_24` 0x100..0x1FF, random 3D drift, `field_2A` / `field_28` 1..4).
/// The sprite lives `field_2A * 6` frames counted in `field_22`. Each frame
/// the coord's translation is projected through `GsWSMATRIX` into a
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut 0x4383, one of the 32x32
/// cells on row 0x28) whose corners are the projected centre plus / minus
/// `field_24 * 31 / otz` rotated by `field_26` and `field_26 + 0x400`. A
/// bright sprite (`spawnArg1 == 1`) flickers a random green / blue-white tint
/// on 1-in-4 LCG rolls and, before its last two frames, fires a 0x600E0
/// effect on 1-in-16. While `Gp_State1C::field_4` is 0 the coord drifts,
/// `field_24` grows by `field_28` and the frame counter advances until it
/// expires, which releases the state-1C memory; `field_4 >= 4` releases it at
/// once and 2..3 idles.
void func_acropolis_helicopter_landing_pad_8017FA30(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    AhlpFlareScratch* blk;
    POLY_FT4*         prim;
    u32               tmp;
    s16               n;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 >= 2) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            if (arg0->spawnArg1 != 0) {
                mem->field_10 = 0;
                mem->field_12 = -0x18;
                mem->field_14 = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x300;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_28 = ((u32)Gp_LcgState >> 16) & 0x3F;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_2A = (((u32)Gp_LcgState >> 16) & 3) + 2;
            } else {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_24 = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x100;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_28 = (((u32)Gp_LcgState >> 16) & 3) + 1;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_2A = (((u32)Gp_LcgState >> 16) & 3) + 1;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = (((u32)Gp_LcgState >> 16) & 7) - 4;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = ~(((u32)Gp_LcgState >> 16) & 0xF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = (((u32)Gp_LcgState >> 16) & 7) - 4;
            }
            arg0->state++;
        }
        scratch     = (void**)G_SCRATCH_HEAD;
        head        = *scratch;
        *scratch    = head - 0x1C;
        blk         = (AhlpFlareScratch*)(head - 0x1C);
        blk->pos.vx = coord->workm.t[0];
        blk->pos.vy = coord->workm.t[1];
        blk->pos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->pos);
        gte_rtps_real();
        gte_stsxy(&((AhlpFlareScratch*)(head - 0x1C))->sx);
        gte_stflg(&((AhlpFlareScratch*)(head - 0x1C))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&((AhlpFlareScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyFT4(prim);
            if (arg0->spawnArg1 == 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
                    setRGB0(prim, tmp >> 1, tmp, 0xFF);
                } else {
                    prim->code |= 1;
                }
                if (mem->field_22 < mem->field_2A * 6 - 2) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && Gp_State1C->field_4 == 0) {
                        Gp_SpawnEff(0x600E0, coord, 0x100, NULL);
                    }
                }
            } else {
                prim->code = 0x2D;
            }
            prim->tpage = 0x2B;
            prim->clut  = 0x4383;
            prim->code |= 2;
            prim->u0    = (mem->field_22 / mem->field_2A + 1) * 0x20;
            prim->v0    = 0x28;
            prim->u1    = (mem->field_22 / mem->field_2A + 1) * 0x20 + 0x1F;
            prim->v1    = 0x28;
            prim->u2    = (mem->field_22 / mem->field_2A + 1) * 0x20;
            prim->v2    = 0x47;
            prim->u3    = (mem->field_22 / mem->field_2A + 1) * 0x20 + 0x1F;
            prim->v3    = 0x47;
            blk->dx     = ((mem->field_24 * 0x1F / blk->otz) * rsin(mem->field_26)) >> 12;
            blk->dy     = ((mem->field_24 * 0x1F / blk->otz) * rcos(mem->field_26)) >> 12;
            prim->x0    = blk->sx + (u16)blk->dx;
            prim->x3    = blk->sx - (u16)blk->dx;
            prim->y0    = blk->sy - (u16)blk->dy;
            prim->y3    = blk->sy + (u16)blk->dy;
            blk->dx     = ((mem->field_24 * 0x1F / blk->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            blk->dy     = ((mem->field_24 * 0x1F / blk->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1    = blk->sx + (u16)blk->dx;
            prim->x2    = blk->sx - (u16)blk->dx;
            prim->y1    = blk->sy - (u16)blk->dy;
            prim->y2    = blk->sy + (u16)blk->dy;
            addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 == 0) {
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            mem->field_24     += mem->field_28;
            n                  = mem->field_22 + 1;
            mem->field_22      = n;
            if (n > mem->field_2A * 6 - 1) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
        }
    }
}

/// Effect task for the helipad floodlights anchored to `Gp_RoomCoords[4]` and
/// `[5]`. On first run it parents the coord to the work's `field_8` and
/// positions it from `field_18..1C`. State 0 rolls 0-3 spawns of
/// `func_acropolis_helicopter_landing_pad_80180664`, a 1-in-4 roll of
/// `func_acropolis_helicopter_landing_pad_80180A64`, and claims slot 4 as a
/// light (refcount 4). State 1 (also reached by fallthrough) rearms
/// `field_24` on a 1-in-4 roll every 8th frame; when armed it plays sound
/// `0x51100001` panned at the coord, spawns one 0x6003B and six 0x600A4
/// effects reparented under this task, and claims slot 5 as a light. State 2
/// releases the state-1C memory, the only step taken while
/// `Gp_State1C::field_4` is set.
void func_acropolis_helicopter_landing_pad_801802E0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpEffWork*     eff;
    s32            i;
    s32            n;
    s32            pan;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        if (arg0->state == 2) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (mem->field_20 == 0) {
        coord->sub        = mem->field_8;
        coord->coord.t[0] = mem->field_18;
        coord->coord.t[1] = mem->field_1A;
        coord->coord.t[2] = mem->field_1C;
        coord->flg        = 0;
        Gp_UpdateCoord(coord);
        mem->field_24 = 1;
        mem->field_20++;
    }
    mem->field_22++;
    switch (arg0->state) {
        case 0:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            n           = ((u32)Gp_LcgState >> 16) & 3;
            for (i = 0; i < n; i++) {
                func_acropolis_helicopter_landing_pad_80180664(coord);
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                if (Gp_State1C->field_4 == 0) {
                    func_acropolis_helicopter_landing_pad_80180A64(coord);
                }
            }
            base           = &Gp_RoomCoords[4];
            slot           = (GpCoordTail*)&base->coord;
            base->field_0  = 4;
            slot->field_58 = 0x15E0;
            slot->field_5C = 0x1900;
            slot->field_50 = 0x800;
            slot->field_52 = 0x800;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            slot->field_54 = (((u32)Gp_LcgState >> 16) & 0x700) + 0x900;
            Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &base->coord.coord);
            base->coord.flg = 0;
            /* fallthrough */
        case 1:
            if ((D_80070F70 & 7) == 0) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    mem->field_24 = 1;
                }
            }
            if (mem->field_24 != 0) {
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(0x51100001, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                mem->field_24 = 0;
                eff           = Gp_SpawnEff(0x6003B, coord, 0x200, NULL);
                if (eff != NULL) {
                    Task_Reparent(arg0, eff->field_0);
                }
                for (i = 0; i < 6; i++) {
                    eff = Gp_SpawnEff(0x600A4, coord, 1, NULL);
                    if (eff != NULL) {
                        Task_Reparent(arg0, eff->field_0);
                    }
                }
                base           = &Gp_RoomCoords[5];
                slot           = (GpCoordTail*)&base->coord;
                base->field_0  = 4;
                slot->field_58 = 0xFA0;
                slot->field_5C = 0x12C0;
                slot->field_50 = 0xC00;
                slot->field_52 = 0xC00;
                slot->field_54 = 0x600;
                Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &base->coord.coord);
                base->coord.flg = 0;
            }
            break;
        case 2:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Draws one random spark line off the floodlight coord, the same shape as
/// `func_acropolis_helicopter_landing_pad_80180A64` with a different box:
/// `a` is rolled 64 wide and 128 tall hanging 0xC0..0x41 below the coord,
/// `b` is centred (128 wide, 255 tall via an LCG modulo). Both are rotated by
/// the coord's `workm`, offset by its translation and projected through
/// `GsWSMATRIX` into a semi-transparent `LINE_F2` whose green is an LCG byte
/// and red half of it. Nothing is queued when the GTE flag word is negative.
void func_acropolis_helicopter_landing_pad_80180664(GsCOORDINATE2* coord)
{
    void**            scratch;
    u8*               head;
    AhlpSparkScratch* blk;
    LINE_F2*          prim;
    SVECTOR*          vec;
    u32               tmp;
    u16               lvl;

    Gp_UpdateCoord(coord);
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    *scratch    = head - 0x20;
    blk         = (AhlpSparkScratch*)(head - 0x20);
    vec         = &blk->a;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vy   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0xC0;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    __asm__("" : "+r"(vec));
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(vec);
    gte_rtv0_real();
    gte_stsv(vec);
    blk->a.vx   = (u16)blk->a.vx + (u16)coord->workm.t[0];
    blk->a.vy   = (u16)blk->a.vy + (u16)coord->workm.t[1];
    blk->a.vz   = (u16)blk->a.vz + (u16)coord->workm.t[2];
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vx   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0x40;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vy   = (((u32)Gp_LcgState >> 16) % 0xFF) - 0x80;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vz   = (((u32)Gp_LcgState >> 16) & 0x7F) - 0x40;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtv0_real();
    gte_stsv(&((AhlpSparkScratch*)(head - 0x20))->b);
    blk->b.vx = *(u16*)&blk->b.vx + *(u16*)&coord->workm.t[0];
    blk->b.vy = *(u16*)&blk->b.vy + *(u16*)&coord->workm.t[1];
    blk->b.vz = *(u16*)&blk->b.vz + *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x0);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtps_real();
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
    lvl         = tmp;
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x1);
    gte_stflg(&((AhlpSparkScratch*)(head - 0x20))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&((AhlpSparkScratch*)(head - 0x20))->otz);
        prim           = (LINE_F2*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setLineF2(prim);
        setRGB0(prim, tmp >> 1, lvl, 0xFF);
        prim->x0 = blk->x0;
        prim->y0 = blk->y0;
        prim->x1 = blk->x1;
        prim->y1 = blk->y1;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
}

/// Draws one random spark line off the floodlight coord: two endpoints are
/// rolled from the LCG (`a` in a 64x128x64 box, `b` in 64x256x64), rotated by
/// the coord's `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` into a semi-transparent `LINE_F2` whose green is an LCG byte
/// and red half of it. Nothing is queued when the GTE flag word is negative.
void func_acropolis_helicopter_landing_pad_80180A64(GsCOORDINATE2* coord)
{
    void**            scratch;
    u8*               head;
    AhlpSparkScratch* blk;
    LINE_F2*          prim;
    SVECTOR*          vec;
    u32               tmp;
    u16               lvl;

    Gp_UpdateCoord(coord);
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    *scratch    = head - 0x20;
    blk         = (AhlpSparkScratch*)(head - 0x20);
    vec         = &blk->a;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vy   = ((u32)Gp_LcgState >> 16) & 0x7F;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->a.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    __asm__("" : "+r"(vec));
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(vec);
    gte_rtv0_real();
    gte_stsv(vec);
    blk->a.vx   = (u16)blk->a.vx + (u16)coord->workm.t[0];
    blk->a.vy   = (u16)blk->a.vy + (u16)coord->workm.t[1];
    blk->a.vz   = (u16)blk->a.vz + (u16)coord->workm.t[2];
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vx   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vy   = ((u32)Gp_LcgState >> 16) & 0xFF;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    blk->b.vz   = (((u32)Gp_LcgState >> 16) & 0x3F) - 0x20;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtv0_real();
    gte_stsv(&((AhlpSparkScratch*)(head - 0x20))->b);
    blk->b.vx = *(u16*)&blk->b.vx + *(u16*)&coord->workm.t[0];
    blk->b.vy = *(u16*)&blk->b.vy + *(u16*)&coord->workm.t[1];
    blk->b.vz = *(u16*)&blk->b.vz + *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x0);
    gte_ldv0(&((AhlpSparkScratch*)(head - 0x20))->b);
    gte_rtps_real();
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    tmp         = ((u32)Gp_LcgState >> 16) & 0xFF;
    lvl         = tmp;
    gte_stsxy(&((AhlpSparkScratch*)(head - 0x20))->x1);
    gte_stflg(&((AhlpSparkScratch*)(head - 0x20))->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&((AhlpSparkScratch*)(head - 0x20))->otz);
        prim           = (LINE_F2*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setLineF2(prim);
        setRGB0(prim, tmp >> 1, lvl, 0xFF);
        prim->x0 = blk->x0;
        prim->y0 = blk->y0;
        prim->x1 = blk->x1;
        prim->y1 = blk->y1;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
}

/// Effect task for the helipad beacon anchored to `Gp_RoomCoords[4]`. State 0
/// spawns two 0x6005E effects, takes the slot (refcount 4) and seeds its
/// light parameters from the coord and an LCG draw; state 1 spawns two more
/// with arg 0; state 2 fires a 0x6005A effect on 1-in-16 LCG rolls every
/// 64th frame; state 3 releases the state-1C memory. Idle while
/// `Gp_State1C::field_4` is set.
void func_acropolis_helicopter_landing_pad_80180E40(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;

    base  = &Gp_RoomCoords[4];
    slot  = (GpCoordTail*)&base->coord;
    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (arg0->state == 3) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (Gp_State1C->field_4 != 0 && arg0->state < 3) {
        return;
    }
    Gp_UpdateCoord(coord);
    switch (arg0->state) {
        case 0:
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            Gp_SpawnEff(0x6005E, coord, 1, NULL);
            base->field_0          = 4;
            slot->field_58         = 0x1900;
            slot->field_5C         = 0x1C20;
            slot->field_50         = 0x800;
            slot->field_52         = 0x800;
            Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
            slot->field_54         = (((u32)Gp_LcgState >> 16) & 0x700) + 0x900;
            slot->coord.coord.t[0] = coord->coord.t[0];
            slot->coord.coord.t[1] = coord->coord.t[1];
            slot->coord.coord.t[2] = coord->coord.t[2];
            base->coord.flg        = 0;
            break;
        case 1:
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            Gp_SpawnEff(0x6005E, coord, 0, NULL);
            break;
        case 2:
            if (D_80070F70 & 0x40) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                    Gp_SpawnEff(0x6005A, coord, 2, NULL);
                }
            }
            break;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Effect task for one helipad lens flare. State 0 seeds the `GpEffWork`
/// from the LCG: a 0x200..0x3FF radius (`field_24`), a 12-bit angle
/// (`field_26`), a 1..4 lifetime scale (`field_2A`, the flare lives
/// `field_2A * 6` frames counted in `field_22`) and a per-frame drift
/// (`field_10` / `field_12` / `field_14`). Each frame the coord's translation
/// is projected through `GsWSMATRIX` into a semi-transparent `POLY_FT4`
/// (tpage 0x2B, clut 0x4384, one of `field_2A` 40x40 cells on row 0x48)
/// whose four corners are the projected centre plus / minus
/// `field_24 * 39 / otz` rotated by `field_26` and `field_26 + 0x400`. The
/// last eight frames fade to grey; before that a spawned flare
/// (`spawnArg1`) flickers a random green / blue-white tint on 1-in-4 LCG rolls
/// and fires a 0x600E0 effect on 1-in-16, and every flare fires 0x6005A on
/// 1-in-16. While `Gp_State1C::field_4` is 0 the coord drifts and the frame
/// counter advances until it expires, which releases the state-1C memory;
/// `field_4 >= 4` releases it at once and 2..3 idles.
void func_acropolis_helicopter_landing_pad_80181064(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    AhlpFlareScratch* blk;
    POLY_FT4*         prim;
    s32               span;
    s32               n;
    s32               lvl;
    u8                tmp;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (Gp_State1C->field_4 >= 2) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_24 = (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_2A = (((u32)Gp_LcgState >> 16) & 3) + 1;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = -(((u32)Gp_LcgState >> 16) & 0x1F) - 0x40;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            arg0->state++;
        }
        scratch     = (void**)G_SCRATCH_HEAD;
        head        = *scratch;
        *scratch    = head - 0x1C;
        blk         = (AhlpFlareScratch*)(head - 0x1C);
        blk->pos.vx = coord->workm.t[0];
        blk->pos.vy = coord->workm.t[1];
        blk->pos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->pos);
        gte_rtps_real();
        gte_stsxy(&((AhlpFlareScratch*)(head - 0x1C))->sx);
        gte_stflg(&((AhlpFlareScratch*)(head - 0x1C))->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&((AhlpFlareScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyFT4(prim);
            span = mem->field_2A * 6;
            n    = mem->field_22;
            if (span - 8 < n) {
                lvl = (span - n + 1) * 16;
                setRGB0(prim, lvl, lvl, lvl);
            } else {
                if (arg0->spawnArg1 != 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        tmp         = (u32)Gp_LcgState >> 16;
                        setRGB0(prim, tmp >> 1, tmp, 0xFF);
                    } else {
                        prim->code |= 1;
                    }
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && Gp_State1C->field_4 == 0) {
                        Gp_SpawnEff(0x600E0, coord, 0x100, NULL);
                    }
                } else {
                    prim->code = 0x2D;
                }
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 0xF) == 0 && Gp_State1C->field_4 == 0) {
                    Gp_SpawnEff(0x6005A, coord, 2 - arg0->spawnArg1, NULL);
                }
            }
            prim->tpage = 0x2B;
            prim->code |= 2;
            prim->clut  = 0x4384;
            prim->u0    = (mem->field_22 / mem->field_2A) * 0x28;
            prim->v0    = 0x48;
            prim->u1    = (mem->field_22 / mem->field_2A) * 0x28 + 0x27;
            prim->v1    = 0x48;
            prim->u2    = (mem->field_22 / mem->field_2A) * 0x28;
            prim->v2    = 0x6F;
            prim->u3    = (mem->field_22 / mem->field_2A) * 0x28 + 0x27;
            prim->v3    = 0x6F;
            blk->dx     = ((mem->field_24 * 0x27 / blk->otz) * rsin(mem->field_26)) >> 12;
            blk->dy     = ((mem->field_24 * 0x27 / blk->otz) * rcos(mem->field_26)) >> 12;
            prim->x0    = blk->sx + (u16)blk->dx;
            prim->x3    = blk->sx - (u16)blk->dx;
            prim->y0    = blk->sy - (u16)blk->dy;
            prim->y3    = blk->sy + (u16)blk->dy;
            blk->dx     = ((mem->field_24 * 0x27 / blk->otz) * rsin(mem->field_26 + 0x400)) >> 12;
            blk->dy     = ((mem->field_24 * 0x27 / blk->otz) * rcos(mem->field_26 + 0x400)) >> 12;
            prim->x1    = blk->sx + (u16)blk->dx;
            prim->x2    = blk->sx - (u16)blk->dx;
            prim->y1    = blk->sy - (u16)blk->dy;
            prim->y2    = blk->sy + (u16)blk->dy;
            addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->field_4 == 0) {
            coord->coord.t[0] += mem->field_10;
            coord->coord.t[1] += mem->field_12;
            coord->coord.t[2] += mem->field_14;
            coord->flg         = 0;
            mem->field_22++;
            if (mem->field_22 > mem->field_2A * 6 - 1) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
        }
    }
}

/// Per-frame driver of the twelve helipad lights. Flags `Gp_State1C::field_8`
/// while view 0x12 is active, folds the frame counter `D_80070F70 * 4` into a
/// 0..0xFE triangle wave kept in the effect work's `field_24` (the low two bits
/// are dropped on the rising half so the ramp steps in fours), then runs
/// `func_acropolis_helicopter_landing_pad_8017F010` once per light position.
void func_acropolis_helicopter_landing_pad_801818F0(Task* arg0)
{
    GpEffWork* work = (GpEffWork*)arg0->spawnArg2;
    SVECTOR*   pos;
    s32        i;
    s32        v;
    s32        level;

    if ((Gp_GetViewIndex() & 0xFF) == 0x12) {
        Gp_State1C->field_8 = -1;
    } else {
        Gp_State1C->field_8 = 0;
    }

    v              = D_80070F70 << 2;
    work->field_24 = v;
    if (v & 0x80) {
        level = 0x7F - (v & 0x7F);
    } else {
        level = v & 0x7C;
    }
    work->field_24 = level * 2;

    i   = 0;
    pos = D_acropolis_helicopter_landing_pad_80184E80;
    for (; i < 12; i++) {
        func_acropolis_helicopter_landing_pad_8017F010(pos++, i, work->field_24);
    }
}
