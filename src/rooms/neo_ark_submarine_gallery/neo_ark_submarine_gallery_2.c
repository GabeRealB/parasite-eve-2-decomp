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

void func_neo_ark_submarine_gallery_8017F3DC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_neo_ark_submarine_gallery_8017FBCC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_neo_ark_submarine_gallery_8017FFB8(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_neo_ark_submarine_gallery_80180254(SVECTOR* pos, s32 arg1, s32 arg2);
void func_neo_ark_submarine_gallery_80180AC8(SVECTOR* pos, s32 arg1, s32 arg2);
void func_neo_ark_submarine_gallery_80180E80(GsCOORDINATE2* coord, s16 arg1);

/// Per-view draw callback for the gallery's display cases. The first state
/// latches the two effect ids the display cases animate with; every later run
/// draws one fixed set of positions for the current camera view. Views 2 to 6
/// each cover a run of `D_neo_ark_submarine_gallery_801818*` entries, and view 2
/// also hands the task's own coordinate to `func_neo_ark_submarine_gallery_80180E80`.
void func_neo_ark_submarine_gallery_8017EFEC(Task* arg0)
{
    SVECTOR*       pos;
    GsCOORDINATE2* coord;
    s32            view;

    coord = ((TmdObject*)arg0->extra)->coords;
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
void func_neo_ark_submarine_gallery_8017F288(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_neo_ark_submarine_gallery_8017F3DC(coord, (s16)work->field_26, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->field_22++;
        if (task->state == 0) {
            work->field_24 = 0x40;
            work->field_26 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->field_26 += 0x20;
        func_neo_ark_submarine_gallery_8017F3DC(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Scales the unit quad `D_80111E38` by `arg1`, rotates it by `arg0->workm`
/// (no GTE translation) and adds `workm.t`, then projects the four corners
/// through `GsWSMATRIX`. When `gte_stflg` is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut 0x43D1, UV 0,0x38..0x37,0x6F)
/// coloured `(arg2, arg2, arg2)`.
void func_neo_ark_submarine_gallery_8017F3DC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    wm  = &arg0->workm;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
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
void func_neo_ark_submarine_gallery_8017F710(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_neo_ark_submarine_gallery_8017FBCC(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_neo_ark_submarine_gallery_8017FFB8(coord, work->field_20, (s16)work->field_24);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_neo_ark_submarine_gallery_8017FBCC(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_neo_ark_submarine_gallery_8017FFB8(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
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
void func_neo_ark_submarine_gallery_8017FBCC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
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
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x43D2). `arg1` selects a 56-texel UV tile in
/// a 4-wide 2-row grid: u = `(arg1 & 3) * 56`, v = `((arg1 & 7) >> 2) * 56`,
/// then the quad is biased to v+0x70..v-0x59. `arg2` is a signed half-extent;
/// the on-screen radius is `(s16)arg2 * 55 / otz`. The quad is axis-aligned
/// and 2*radius on a side, shifted up so the projected point sits at
/// three-quarters height (`y0 = sy - r - r/2`, `y2 = sy + r/2`).
void func_neo_ark_submarine_gallery_8017FFB8(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw23Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    u32                cell;
    s32                u1;
    s32                vbase;
    s32                v0;
    s32                v1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG_USE(arg2, scratch);
    head                                        = *scratch;
    ((RoomDraw23Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw23Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    tex                                         = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw23Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw23Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw23Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        SOFT_BARRIER();
        cell  = (u16)tex;
        tex   = (cell & 3) * 0x38;
        vbase = ((cell & 7) >> 2) * 0x38;
        v0    = vbase + 0x70;
        SOFT_USE_REG(v0);
        u1       = tex + 0x37;
        v1       = vbase - 0x59;
        prim->v2 = v1;
        prim->v3 = v1;
        TOUCH_REG(u1);
        sarg          = (s16)arg2;
        prim->v0      = v0;
        prim->v1      = v0;
        t             = sarg * 0x38;
        prim->u0      = tex;
        prim->u1      = u1;
        prim->u2      = tex;
        prim->u3      = u1;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        v1            = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        xy            = v1;
        ds            = &gDisplayState;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + (block->radius >> 1);
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Projects `arg0` and `arg0 + 1` through `Gfx_ViewWorldMtx` and sweeps three
/// gouraud `POLY_G4` wedges per 0x400 step around the screen-space angle between
/// the two centres, lit with the colour packed in `arg2`. Each `otz` past 0x50
/// is pulled 0x40 closer before it sets the radius and the OT slot.
void func_neo_ark_submarine_gallery_80180254(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    s32                sum;
    s32                otz;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (RoomDraw08Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        otz = ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
        if (otz > 0x50) {
            ((RoomDraw08Scratch*)(head - 0x1C))->otz0 = otz - 0x40;
        }
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            otz = block->otz1;
            if (otz > 0x50) {
                block->otz1 = otz - 0x40;
            }
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
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
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
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
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues four gouraud `POLY_G4` wedges around
/// the projected centre. An `otz` past 0x50 is pulled 0x40 closer before it
/// sets the radius and the OT slot.
void func_neo_ark_submarine_gallery_80180AC8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
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

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz = ((RoomDraw13Scratch*)(head - 0x10))->otz;
        if (otz > 0x50) {
            ((RoomDraw13Scratch*)(head - 0x10))->otz = otz - 0x40;
        }
        arg1 = arg1 << 16;
        arg1 = arg1 >> 10;
        arg1 = arg1 / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
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
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Draws one prism from `D_neo_ark_submarine_gallery_801818C8[arg1..arg1 + 7]`
/// as five `POLY_G4` quads: entries 0..3 are one ring of corners and 4..7 the
/// opposite ring. Each corner is rotated by `coord->workm` and moved by its
/// translation before projection through `GsWSMATRIX`. The four side quads fade
/// from a pulsing grey on the first ring to black on the second; the closing
/// cap over the first ring is flat grey. The grey swings a couple of steps
/// around 0x18 with `gDisplayState.animFrame`.
void func_neo_ark_submarine_gallery_80180E80(GsCOORDINATE2* coord, s16 arg1)
{
    RoomQuadProjScratch* blk;
    POLY_G4*             prim;
    s32                  i;
    s32                  next;
    s32                  far;
    s32                  farNext;
    u8                   shade;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomQuadProjScratch);
    blk                     = (RoomQuadProjScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x18;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
        blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
        blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
        blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
        blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
        blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
        blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
        blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
        blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
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
            addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
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
    blk->v[0].vx = *(u16*)&blk->v[0].vx + *(u16*)&coord->workm.t[0];
    blk->v[0].vy = *(u16*)&blk->v[0].vy + *(u16*)&coord->workm.t[1];
    blk->v[0].vz = *(u16*)&blk->v[0].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = *(u16*)&blk->v[1].vx + *(u16*)&coord->workm.t[0];
    blk->v[1].vy = *(u16*)&blk->v[1].vy + *(u16*)&coord->workm.t[1];
    blk->v[1].vz = *(u16*)&blk->v[1].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = *(u16*)&blk->v[2].vx + *(u16*)&coord->workm.t[0];
    blk->v[2].vy = *(u16*)&blk->v[2].vy + *(u16*)&coord->workm.t[1];
    blk->v[2].vz = *(u16*)&blk->v[2].vz + *(u16*)&coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = *(u16*)&blk->v[3].vx + *(u16*)&coord->workm.t[0];
    blk->v[3].vy = *(u16*)&blk->v[3].vy + *(u16*)&coord->workm.t[1];
    blk->v[3].vz = *(u16*)&blk->v[3].vz + *(u16*)&coord->workm.t[2];
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
        addPrim((u_long*)((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomQuadProjScratch);
}
