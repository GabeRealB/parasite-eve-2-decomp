#include "pe/necrosis.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// One 4-byte row of `D_necrosis_801306BC`, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the `Gp_SpawnEff` draw
/// parameter (plus `field_22 * 0x60` each frame) and is copied into the
/// first `GpObj.radius`. `field_2` is the last `GpEffWork.age` tick
/// of the spawn loop; state 2 waits an extra 0x10 ticks past it. `field_2 +
/// 0xC` is also the pad-rumble duration at ignition.
typedef struct NecrosisStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} NecrosisStep;
STATIC_ASSERT_SIZEOF(NecrosisStep, 4);

/// Collision pair allocated by `func_necrosis_8012EF34` (`memCalloc(0x58)`)
/// and stored in `Task::work`. `obj` is linked on list 1, `obj2` on list 7;
/// both point `ctx.recs` at the one-element `rec` table (terminator `field_0
/// = 2`).
typedef struct NecrosisWork {
    /* 0x00 */ GpObj                 obj;
    /* 0x20 */ GpObj                 obj2;
    /* 0x40 */ WorldCollisionContact rec;
} NecrosisWork;
STATIC_ASSERT_SIZEOF(NecrosisWork, 0x58);

/// Per-level tuning for the necrosis burst: rows are PE levels 1-3, selected
/// by `index`. `field_0` is the `Gp_SpawnEff` draw parameter; `field_2` is
/// the last spawn-loop tick, and `field_2 + 0xC` the pad-rumble duration.
static NecrosisStep D_necrosis_801306BC[] = {
    { 0x03C0, 0x000A },
    { 0x0480, 0x000F },
    { 0x0540, 0x0014 },
};

/// The `SndEvt_EnqueueType6` id for each `D_necrosis_801306BC` row.
static s32 D_necrosis_801306C8[] = { 0xE0150001, 0xE0180001, 0xE01B0001 };

static void func_necrosis_8012F6EC(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_necrosis_8012FE64(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_necrosis_80130288(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Runs one frame of the necrosis cast. State 0 copies the player rotation onto
/// the effect coordinate, rotates a (0, 0, 0x90) offset into that frame, and
/// links a `NecrosisWork` collision pair (list 1 + list 7) whose packed id is
/// the combo digits plus `0x28000`. State 1 GPF-scales that offset by 0x1100
/// each frame, walks the coordinate, and spawns `0x80060019`; a `0x100000` hit
/// on `obj2` zeros the offset and unlinks the list-7 object. State 2 waits
/// `field_2 + 0x10` ticks. Any state releases if the player is dying
/// (`Gp_StateC08.field_3` / `Gp_StateC08.field_3`) or the room is fading (`Gp_State1C`).
void func_necrosis_8012EF34(Task* arg0)
{
    NecrosisWork*          work;
    GpEffWork*             mem;
    GfxCoord*              coord;
    GfxCoord*              player;
    GpMtxWords*            dstm;
    GpMtxWords*            srcm;
    WorldCollisionContact* rec;
    GpEffWork*             spawned;
    s32                    pan;
    u16                    old;
    s32                    tick;
    s16                    fade;

    work     = (NecrosisWork*)arg0->work;
    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    old      = mem->age;
    tick     = old + 1;
    mem->age = tick;
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_3 == -2) {
                goto release;
            }
            fade = Gp_State1C->fadeState;
            if (fade >= 4) {
                goto release;
            }
            if (fade != 0) {
                mem->age = old;
                return;
            }
            work = memCalloc(0x58, 0);
            if (work == NULL) {
                mem->age = 0;
                return;
            }
            player              = (gameGetPtrSlot(3))->extra.tmd->coords;
            dstm                = (GpMtxWords*)&coord->coord;
            srcm                = (GpMtxWords*)&player->coord;
            dstm->m00_m01       = srcm->m00_m01;
            dstm->m02_m10       = srcm->m02_m10;
            dstm->m11_m12       = srcm->m11_m12;
            dstm->m20_m21       = srcm->m20_m21;
            dstm->m22           = srcm->m22;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            mem->move.vx = 0;
            mem->move.vy = 0;
            mem->move.vz = 0x90;
            gte_SetRotMatrix((MATRIX*)srcm);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            rec                = &work->rec;
            mem->index         = (Gp_StateC08.field_0 % 10) - 1;
            arg0->work         = (TaskIdMap*)work;
            work->obj.coord    = coord;
            work->obj.ctx.recs = rec;
            work->obj.key =
                ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 + ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 + (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
            work->obj.radius = D_necrosis_801306BC[mem->index].field_0;
            work->obj.flags  = 1;
            Gp_LinkObj(1, &work->obj);
            rec->flags          = 2;
            work->obj2.coord    = coord;
            work->obj2.ctx.recs = rec;
            work->obj2.key      = 0;
            work->obj2.radius   = 0x80;
            work->obj2.flags    = 1;
            work->obj.flags    |= 0x8000;
            Gp_LinkObj(7, &work->obj2);
            work->obj2.flags = (work->obj2.flags & 0x7FFF) | 0x4400;
            pan              = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(D_necrosis_801306C8[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)gpGetObjDepth(coord));
            Gp_SpawnPadLerp((s16)((u16)D_necrosis_801306BC[mem->index].field_2 + 0xC), 0xFF, 8);
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (Gp_State1C->fadeState == 0) {
                gte_lddp(0x1100);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                spawned = Gp_SpawnEff(0x80060019, coord,
                                      (s16)D_necrosis_801306BC[mem->index].field_0 + (mem->age * 0x60),
                                      NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
                work->obj.radius = (u16)work->obj.radius + 0x20;
            } else {
                mem->age = mem->age - 1;
            }
            if ((Gp_StateC08.field_3 == -2) || (Gp_State1C->fadeState >= 4)) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                goto release;
            }
            if (mem->age > D_necrosis_801306BC[mem->index].field_2) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            if (Gp_FindRec18(work->obj2.ctx.recs, 0x100000) != 0) {
                mem->move.vx = 0;
                mem->move.vy = 0;
                mem->move.vz = 0;
                Gp_UnlinkObj(&work->obj2);
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 2:
            if (Gp_StateC08.field_3 == -2) {
                goto release;
            }
            if (Gp_State1C->fadeState >= 4) {
                goto release;
            }
            tick = (s16)tick;
            if ((D_necrosis_801306BC[mem->index].field_2 + 0x10) < tick) {
            release:
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

void func_necrosis_8012F52C(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    GpEffWork* spawned;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_State1C->fadeState != 0) {
        return;
    }

    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        mem->scale  = arg0->spawnArg1.value & 0xFFF;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->angle  = (Gp_LcgState >> 16) & 0xFFF;
        mem->period = mem->scale - 0x100;
        mem->step   = mem->scale >> 4;
        arg0->state = 1;
    }
    Gp_UpdateCoord(coord);
    func_necrosis_8012F6EC(coord, mem->age % 6, mem->scale, mem->angle);
    mem->scale = mem->scale - mem->step;
    if (mem->scale < mem->step) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (mem->age % 3 == 0) {
        spawned = Gp_SpawnEff(0x6001A, coord, (s32)(mem->period), 0);
        if (spawned != NULL) {
            Task_Reparent(arg0, spawned->task);
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
static void func_necrosis_8012F6EC(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang2;
    u16              vz;

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    SCRATCH_HEAD(GpFxQuadScratch)             = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 39) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_necrosis_8012FAF8(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        tick;
    s32        rng1;
    s32        rng2;
    s32        rng3;
    s32        temp_lo;
    s32        var_v1;
    u16        temp_v0;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_State1C->fadeState != 0) {
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->age     = 0;
            temp_v0      = arg0->spawnArg1.value;
            mem->period  = temp_v0 & 0xFFF;
            rng1         = (Gp_LcgState * 5) + 0x71357911;
            mem->scale   = ((u32)rng1 >> 16) & 0xFFF;
            Gp_LcgState  = rng1;
            mem->angle   = mem->period / 20;
            mem->move.vx = (rsin(mem->scale) * mem->angle) >> 12;
            temp_lo      = rcos(mem->scale) * mem->angle;
            rng2         = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState  = rng2;
            mem->move.vy = temp_lo >> 12;
            mem->move.vz = (rsin(((u32)rng2 >> 16) & 0xFFF) * mem->move.vx) >> 12;
            rng3         = (Gp_LcgState * 5) + 0x71357911;
            Gp_LcgState  = rng3;
            if ((s32)(((u32)rng3 >> 16) & 3) < ((u16)(Gp_StateC08.field_0 % 10U) - 1)) {
                mem->step = 0x1000;
            }
            if ((u16)(Gp_StateC08.field_0 % 10U) - 1 < 2) {
                arg0->state = 1;
                return;
            }
            var_v1 = 2;
            if (mem->step != 0) {
                var_v1 = 1;
            }
            arg0->state = var_v1;
            return;
        case 1:
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            tick       = mem->index + 1;
            mem->index = tick;
            if (tick < 8) {
                func_necrosis_8012FE64(coord, (s16)(tick | mem->step), mem->period,
                                       mem->scale);
                return;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        case 2:
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            tick       = mem->index + 1;
            mem->index = tick;
            if (tick < 6) {
                func_necrosis_80130288(coord, (s16)(tick | mem->step), mem->period,
                                       mem->scale);
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
static void func_necrosis_8012FE64(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s32              u0;
    s32              u1;
    s32              ang2;

    block         = SCRATCH_PUSH(GpFxQuadScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        if (arg1 & 0x1000) {
            prim->tpage = 0x2A;
            prim->clut  = 0x428F;
        } else {
            prim->tpage = 0x4A;
            prim->clut  = 0x42C2;
        }
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        u0 = (arg1 & 0xF) << 5;
        u1 = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + block->dx;
        prim->x3  = block->sx - block->dx;
        prim->y0  = block->sy - block->dy;
        prim->y3  = block->sy + block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + block->dx;
        prim->x2  = block->sx - block->dx;
        prim->y1  = block->sy - block->dy;
        prim->y2  = block->sy + block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpFxQuadScratch);
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
static void func_necrosis_80130288(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    s32              u0;
    s32              u1;
    s32              ang2;

    block         = SCRATCH_PUSH(GpFxQuadScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
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
        u0 = (arg1 & 0xF) * 40;
        u1 = u0 + 0x27;
        setUV4(prim, u0, 0x50, u1, 0x50, u0, 0x77, u1, 0x77);
        block->dx = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + block->dx;
        prim->x3  = block->sx - block->dx;
        prim->y0  = block->sy - block->dy;
        prim->y3  = block->sy + block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 39) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 39) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + block->dx;
        prim->x2  = block->sx - block->dx;
        prim->y1  = block->sy - block->dy;
        prim->y2  = block->sy + block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpFxQuadScratch);
}
