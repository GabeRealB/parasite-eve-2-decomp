#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern SVECTOR D_neo_ark_submarine_gallery_801818C8[];
extern SVECTOR D_neo_ark_submarine_gallery_801818D8[];
extern SVECTOR D_neo_ark_submarine_gallery_801818F8[];
extern SVECTOR D_neo_ark_submarine_gallery_80181928[];

extern s32 D_80115738;
extern s32 D_8011574C;

static void func_neo_ark_submarine_gallery_8017F3DC(GpCoord* arg0, s32 arg1, s32 arg2);
static void func_neo_ark_submarine_gallery_8017FBCC(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_neo_ark_submarine_gallery_8017FFB8(GpCoord* arg0, s32 arg1, s32 arg2);
static void func_neo_ark_submarine_gallery_80180254(SVECTOR* pos, s32 arg1, s32 arg2);
static void func_neo_ark_submarine_gallery_80180AC8(SVECTOR* pos, s32 arg1, s32 arg2);
static void func_neo_ark_submarine_gallery_80180E80(GpCoord* coord, s16 arg1);

/// Per-view draw callback for the gallery's display cases. The first state
/// latches the two effect ids the display cases animate with; every later run
/// draws one fixed set of positions for the current camera view. Views 2 to 6
/// each cover a run of `D_neo_ark_submarine_gallery_801818*` entries, and view 2
/// also hands the task's own coordinate to `func_neo_ark_submarine_gallery_80180E80`.
static void func_neo_ark_submarine_gallery_8017EFEC(Task* arg0)
{
    SVECTOR* pos;
    GpCoord* coord;
    s32      view;

    coord = arg0->extra.tmd->coords;
    if (arg0->state == 0) {
        D_8011574C  = 0x60193;
        D_80115738  = 0x60194;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex() & 0xFF;
    switch (view) {
        case 2:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818C8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818C8[2], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818C8[4], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[16], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[17], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[18], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818C8[31], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180E80(coord, 0x20);
            break;
        case 3:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_80181928[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[4], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[5], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[14], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[15], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[16], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[17], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_80181928[18], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_80181928[19];
            func_neo_ark_submarine_gallery_80180AC8(pos, 0x200, 0x444);
            break;
        case 4:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[2], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[4], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818F8[18], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818F8[19], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180AC8(&D_neo_ark_submarine_gallery_801818F8[20], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818F8[21];
            func_neo_ark_submarine_gallery_80180AC8(pos, 0x200, 0x444);
            break;
        case 5:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818D8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818D8[2], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818D8[3];
            func_neo_ark_submarine_gallery_80180254(pos, 0x200, 0x444);
            break;
        case 6:
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[0], 0x200, 0x444);
            func_neo_ark_submarine_gallery_80180254(&D_neo_ark_submarine_gallery_801818F8[2], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818F8[4];
            func_neo_ark_submarine_gallery_80180254(pos, 0x200, 0x444);
            break;
    }
}

/// `Gp_State1C` effect task drawing a growing, fading quad through
/// `func_neo_ark_submarine_gallery_8017F3DC`. The first frame sets the brightness
/// to 0x40, takes the size from the spawn parameter's low 12 bits and turns
/// the coordinate to a random Y rotation. Every frame then rebuilds the
/// coordinate, grows the size by 0x20, draws, and dims by 2, releasing the
/// effect once the brightness falls under 2. Once the room's event state
/// leaves zero it only draws, and releases at state 4.
static void func_neo_ark_submarine_gallery_8017F288(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_neo_ark_submarine_gallery_8017F3DC(coord, work->angle, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->angle += 0x20;
        func_neo_ark_submarine_gallery_8017F3DC(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
static void func_neo_ark_submarine_gallery_8017F3DC(GpCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        tbl   = &D_80111E38[i];
        v     = &block->vec[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = block->sxy0.vx;
        prim->y0 = block->sxy0.vy;
        prim->x1 = block->sxy1.vx;
        prim->y1 = block->sxy1.vy;
        prim->x2 = block->sxy2.vx;
        prim->y2 = block->sxy2.vy;
        prim->x3 = block->sxy3.vx;
        prim->y3 = block->sxy3.vy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Per-frame driver of a particle effect task, drawn as the spinning sprite of
/// `func_neo_ark_submarine_gallery_8017FBCC` (state 1) or, when the spawn
/// argument's top nibble is set, as the upright sprite of
/// `func_neo_ark_submarine_gallery_8017FFB8` (state 2). The first frame takes
/// the size from the argument's low 12 bits, a random spin angle, and the ticks
/// per animation frame from bits 12-15. Unless the work block already holds a
/// velocity, it picks one by the kind in bits 24-27 - none, a random upward
/// burst, a random spray, a narrow upward jet, or the block's stored direction -
/// scaled to the speed in bits 16-23 (0x40 when zero). Every later tick draws,
/// moves the coordinate by the velocity with gravity pulling it down, and
/// releases the block after animation frame 7. While the room's event state is
/// non-zero it only draws, releasing the block from event state 4 on.
static void func_neo_ark_submarine_gallery_8017F710(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_neo_ark_submarine_gallery_8017FBCC(coord, (u16)work->index, work->scale, work->angle);
            } else {
                func_neo_ark_submarine_gallery_8017FFB8(coord, (u16)work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_neo_ark_submarine_gallery_8017FBCC(coord, (u16)work->index, work->scale, work->angle);
            break;
        case 2:
            func_neo_ark_submarine_gallery_8017FFB8(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x43D3) spun about the projected centre.
/// `arg1` selects the 32-texel UV column `(arg1 & 0xFFFF) << 5` at v=0xE0..0xFF.
/// `arg2` is a signed half-extent; the on-screen radius is
/// `(s16)arg2 * 31 / otz`. `arg3` is the spin angle, applied at `arg3` and
/// `arg3 + 0x400` through `rsin`/`rcos`.
static void func_neo_ark_submarine_gallery_8017FBCC(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG_USE(arg2, scratch);
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws an upright sprite at the coordinate's world position, projected
/// through `GsWSMATRIX`. If the projection is valid, one semi-transparent
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued, its texture the 56-texel
/// cell `arg1 & 7` of a four-wide, two-row grid starting at v 0x70. The quad
/// is `2 * r` on a side with `r = (s16)arg2 * 55 / otz`, and the projected
/// point sits a quarter of the way up from its bottom edge. The work block
/// lives on the scratchpad stack.
static void func_neo_ark_submarine_gallery_8017FFB8(GpCoord* arg0, s32 arg1, s32 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    u16            idx;
    u32            cell;
    s32            row;
    u8             u0;
    u8             u1;
    u8             v0;
    u8             v1;

    idx           = arg1;
    block         = SCRATCH_PUSH(GpRingScratch);
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = idx;
        u0          = (cell & 3) * 0x38;
        row         = ((cell & 7) >> 2) * 0x38;
        v0          = row + 0x70;
        v1          = row + 0xA7;
        u1          = u0 + 0x37;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        block->step = ((s16)arg2 * 55) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Projects `arg0` and `arg0 + 1` through `gGfxViewCoord.workm` and sweeps three
/// gouraud `POLY_G4` wedges per 0x400 step around the screen-space angle between
/// the two centres, lit with the colour packed in `arg2`. Each `otz` past 0x50
/// is pulled 0x40 closer before it sets the radius and the OT slot.
static void func_neo_ark_submarine_gallery_80180254(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    s32                      otz;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        otz = ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
        if (otz > 0x50) {
            ((OverlayPointPairScratch*)(head - 0x1C))->otz0 = otz - 0x40;
        }
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            otz = block->otz1;
            if (otz > 0x50) {
                block->otz1 = otz - 0x40;
            }
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues four gouraud `POLY_G4` wedges around
/// the projected centre. An `otz` past 0x50 is pulled 0x40 closer before it
/// sets the radius and the OT slot.
static void func_neo_ark_submarine_gallery_80180AC8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                otz;
    u8                 r;
    u8                 g;
    u8                 b;

    block = SCRATCH_PUSH(RoomDraw13Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz = block->otz;
        if (otz > 0x50) {
            block->otz = otz - 0x40;
        }
        arg1          = arg1 << 16;
        arg1          = arg1 >> 10;
        arg1          = arg1 / block->otz;
        ang           = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

/// Draws one prism from `D_neo_ark_submarine_gallery_801818C8[arg1..arg1 + 7]`
/// as five `POLY_G4` quads: entries 0..3 are one ring of corners and 4..7 the
/// opposite ring. Each corner is rotated by `coord->workm` and moved by its
/// translation before projection through `GsWSMATRIX`. The four side quads fade
/// from a pulsing grey on the first ring to black on the second; the closing
/// cap over the first ring is flat grey. The grey swings a couple of steps
/// around 0x18 with `gDisplayState.animFrame`.
static void func_neo_ark_submarine_gallery_80180E80(GpCoord* coord, s16 arg1)
{
    RoomQuadProjScratch* blk;
    POLY_G4*             prim;
    s32                  i;
    s32                  next;
    s32                  far;
    s32                  farNext;
    u8                   shade;

    SCRATCH_PUSH(RoomQuadProjScratch);
    blk = SCRATCH_HEAD(RoomQuadProjScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x18;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
        blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
        blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
        blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
        blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
        blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
        blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
        blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
        blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sxy[0]);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sxy[1], &blk->sxy[2], &blk->sxy[3]);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, shade, shade, shade);
            setRGB1(prim, shade, shade, shade);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            prim->x0 = blk->sxy[0].vx;
            prim->y0 = blk->sxy[0].vy;
            prim->x1 = blk->sxy[1].vx;
            prim->y1 = blk->sxy[1].vy;
            prim->x2 = blk->sxy[2].vx;
            prim->y2 = blk->sxy[2].vy;
            prim->x3 = blk->sxy[3].vx;
            prim->y3 = blk->sxy[3].vy;
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
    blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
    blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
    blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
    blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
    blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
    blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
    blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
    blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    gte_stsxy(&blk->sxy[0]);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&blk->sxy[1], &blk->sxy[2], &blk->sxy[3]);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        setRGB0(prim, shade, shade, shade);
        setRGB1(prim, shade, shade, shade);
        setRGB2(prim, shade, shade, shade);
        setRGB3(prim, shade, shade, shade);
        prim->x0 = blk->sxy[0].vx;
        prim->y0 = blk->sxy[0].vy;
        prim->x1 = blk->sxy[1].vx;
        prim->y1 = blk->sxy[1].vy;
        prim->x2 = blk->sxy[2].vx;
        prim->y2 = blk->sxy[2].vy;
        prim->x3 = blk->sxy[3].vx;
        prim->y3 = blk->sxy[3].vy;
        addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)), prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    SCRATCH_POP(RoomQuadProjScratch);
}
