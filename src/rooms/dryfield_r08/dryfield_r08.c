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
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_r08_8017F464[];
extern SVECTOR D_dryfield_r08_8017F4C4[];
extern s32     D_dryfield_r08_80180C24;

/// The room's active data bank pointer, which starts out holding bank 0, and
/// the two banks `func_dryfield_r08_8017F438` chooses between.
extern void* D_dryfield_r08_8017F708;
extern u32   D_dryfield_r08_801809C0[];
extern u32   D_dryfield_r08_80180B58[];

void func_dryfield_r08_8017DEFC(GsCOORDINATE2* arg0, u16 arg1, s32 arg2, s32 arg3);
void func_dryfield_r08_8017E36C(GsCOORDINATE2* arg0, u16 arg1, s32 arg2, s32 arg3);
void func_dryfield_r08_8017E7C8(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_dryfield_r08_8017D5F8(Task* task)
{
    s32 i;
    u8  view;

    if (task->state == 0) {
        D_dryfield_r08_80180C24 = 0;
        task->state             = task->state + 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            func_dryfield_r08_8017E7C8(&q[0], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[2], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[3], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[6], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[14], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[15], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[16], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[17], 0x200, 0x444);
            break;
        }
        case 3:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 4:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 5: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            func_dryfield_r08_8017E7C8(&q[0], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[1], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[2], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[7], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[8], 0x200, 0x444);
            func_dryfield_r08_8017E7C8(&q[21], 0x200, 0x400);
            func_dryfield_r08_8017E7C8(&q[22], 0x200, 0x400);
            func_dryfield_r08_8017E7C8(&q[23], 0x200, 0x400);
            func_dryfield_r08_8017E7C8(&q[29], 0x200, 0x400);
            break;
        }
        case 6: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            func_dryfield_r08_8017E7C8(&q[0], 0x200, 0x433);
            func_dryfield_r08_8017E7C8(&q[2], 0x200, 0x433);
            func_dryfield_r08_8017E7C8(&q[3], 0x200, 0x433);
            func_dryfield_r08_8017E7C8(&q[6], 0x200, 0x433);
            func_dryfield_r08_8017E7C8(&q[14], 0x200, 0x433);
            func_dryfield_r08_8017E7C8(&q[15], 0x200, 0x433);
            func_dryfield_r08_8017E7C8(&q[16], 0x200, 0x433);
            func_dryfield_r08_8017E7C8(&q[17], 0x200, 0x433);
            break;
        }
    }
}

/// Per-frame handler for one animated sprite effect, drawn by
/// `func_dryfield_r08_8017DEFC` or `func_dryfield_r08_8017E36C`. Its first
/// frame unpacks `spawnArg1`: the low 12 bits are the sprite size, bits 12..14
/// the frames per animation cell (1 when zero), bits 28..30 are kept as the
/// drawer's clut selector, and the sign bit picks the second drawer. When the
/// work block arrives without a velocity, bits 24..27 choose how one is rolled
/// from `Gp_LcgState` (0 leaves it still) and it
/// is normalised to a speed from bits 16..23 (0x40 when zero). Each later frame
/// draws the current cell, moves the coordinate by the velocity and bends its
/// Y component, then frees the effect after the drawer's last cell (12 or 10).
/// While the player is in an event it only draws, and frees once the event
/// aborts.
void func_dryfield_r08_8017D8B4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            step;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_dryfield_r08_8017DEFC(coord, work->index, work->scale, work->angle);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = 1;
            task->state  = task->spawnArg1 < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1 >> 16) & 0x7000;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
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
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
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
            break;
        case 1:
            func_dryfield_r08_8017DEFC(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_dryfield_r08_8017E36C(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Same projected, spinning `POLY_FT4` as `func_dryfield_r08_8017E36C`, with
/// its own texture window: the low 12 bits of `arg1` pick a 48x48 cell from a
/// five-column grid (u = `cell % 5 * 48`, v = `cell / 5 * 48 + 0x68`), and the
/// top four bits select the clut - row `0x10E + sel` at column `cell & 0x3F`
/// for 0 and 1, the fixed clut 0x428F otherwise.
void func_dryfield_r08_8017DEFC(GsCOORDINATE2* arg0, u16 arg1, s32 arg2, s32 arg3)
{
    void**               scratch;
    u8*                  head;
    OverlaySparkScratch* block;
    POLY_FT4*            prim;
    s32                  ang;
    s32                  ang2;
    s32                  span;
    s32                  u0;
    s32                  v0;
    s32                  u1;
    s32                  v1;
    u16                  vz;
    s32                  tex;
    u16                  sel;
    s32                  sine;
    OverlaySparkScratch* tmp;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG(arg2);
    head = *scratch;
    tmp  = (OverlaySparkScratch*)(head - 0x1C);
    SOFT_TOUCH_REG(tmp);
    block         = tmp;
    block->vec.vx = *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    tex           = arg1 & 0xFFF;
    sel           = arg1 >> 12;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((OverlaySparkScratch*)(head - 0x1C))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((OverlaySparkScratch*)(head - 0x1C))->sx);
    gte_stflg(&((OverlaySparkScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim->tpage = 0x2B;
        prim->code |= 3;
        if (sel >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((sel + 0x10E) << 6) | (tex & 0x3F);
        }
        ang = (s16)arg3;
        u0  = ((u16)tex % 5) * 0x30;
        v0  = ((u16)tex / 5) * 0x30;
        u1  = u0 + 0x2F;
        v1  = v0 - 0x69;
        v0  = v0 + 0x68;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        sine      = rsin(ang);
        span      = (s16)arg2 * 0x2F;
        block->dx = ((span / block->otz) * sine) >> 12;
        block->dy = ((span / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((span / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((span / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Projects `arg0`'s world translation through `GsWSMATRIX` and, unless the
/// GTE flag word is negative, queues one semi-transparent `POLY_FT4` centred on
/// the projected point. The low 12 bits of `arg1` pick a 48x48 cell from a
/// five-column grid (u = `cell % 5 * 48`, v = `cell / 5 * 48 - 0x80`); any of
/// its top four bits set selects clut 0x428F instead of 0x43D0. The quad's
/// diagonals are `(s16)arg2 * 47 / otz` long, turned by `arg3` and
/// `arg3 + 0x400`, so it shrinks with distance and spins with the angle.
void func_dryfield_r08_8017E36C(GsCOORDINATE2* arg0, u16 arg1, s32 arg2, s32 arg3)
{
    void**               scratch;
    u8*                  head;
    OverlaySparkScratch* block;
    POLY_FT4*            prim;
    s32                  ang;
    s32                  ang2;
    s32                  span;
    s32                  u0;
    s32                  v0;
    s32                  u1;
    s32                  v1;
    u16                  vz;
    u16                  tex;
    u16                  sel;
    s32                  sine;
    OverlaySparkScratch* tmp;

    scratch = (void**)G_SCRATCH_HEAD;
    SOFT_TOUCH_REG(arg2);
    head = *scratch;
    tmp  = (OverlaySparkScratch*)(head - 0x1C);
    SOFT_TOUCH_REG(tmp);
    block         = tmp;
    block->vec.vx = *(u16*)&arg0->workm.t[0];
    sel           = arg1 >> 12;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    tex           = arg1 & 0xFFF;
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((OverlaySparkScratch*)(head - 0x1C))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((OverlaySparkScratch*)(head - 0x1C))->sx);
    gte_stflg(&((OverlaySparkScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim->tpage = 0x2C;
        prim->code |= 3;
        if (sel) {
            prim->clut = 0x428F;
        } else {
            prim->clut = 0x43D0;
        }
        ang = (s16)arg3;
        u0  = (tex % 5) * 0x30;
        v0  = (tex / 5) * 0x30;
        u1  = u0 + 0x2F;
        v1  = v0 - 0x51;
        v0  = v0 - 0x80;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        sine      = rsin(ang);
        span      = (s16)arg2 * 0x2F;
        block->dx = ((span / block->otz) * sine) >> 12;
        block->dy = ((span / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((span / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((span / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a glowing disc around the point `arg0`, projected through
/// `Gfx_ViewWorldMtx`, unless the projection flags an error: four gouraud
/// wedges lit at the projected centre and black at the rim, of screen radius
/// `(s16)arg1 * 64 / otz`. `arg2` is the colour as three 4-bit channels
/// (0xRGB), brightened slightly on odd frames.
void func_dryfield_r08_8017E7C8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw31Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw31Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw31Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw31Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw31Scratch*)(head - 0x10))->otz;
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

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues an eight-wedge gouraud disc plus four
/// inner cross wedges around the projected centre. `arg1` is a
/// signed half-extent; on-screen radii are `(s16)arg1 * 64 / otz` (outer) and
/// `(s16)arg1 * 8 / otz` (inner). `arg2` packs the colour one nibble per
/// channel - bits 8..11 red, 4..7 green, 0..3 blue, each scaled to 8 bits -
/// with bits 12..15 giving the shift for a `gDisplayState.animFrame & 1`
/// flicker added to every channel. The outer disc uses the full colour and
/// the inner cross half of it.
void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDiscScratch* block;
    POLY_G4*         prim;
    s32              ang;
    s32              t;
    s32              t2;
    s32              ua;
    s32              ub;
    s32              uc;
    s32              frame;
    s32              packed;
    s32              blend;
    s32              r;
    s32              g;
    s32              b;
    s32              outer;
    s32              inner;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = (*scratch = (u8*)*scratch - 0x14);
        block   = (RoomDiscScratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        r   = (u8)r >> 1;
        g   = (u8)g >> 1;
        b   = (u8)b >> 1;
        ang = 0x200;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

void func_dryfield_r08_8017F334(s32 arg0)
{
    D_dryfield_r08_80180C24 = arg0;
}

/// Sets the skip-OT-link byte (`GpSprtCmd.field_4`) of command record
/// `arg0` + 1 in this room's sprite-table command list: non-zero leaves that
/// record's prims out of the ordering table. `arg0` is a view index below
/// 0xB; the record the table yields is larger than its `GpSprtRec` prefix,
/// so `[3].field_4` reaches the command list its tail holds there.
void func_dryfield_r08_8017F340(u8 arg0, u8 arg1)
{
    GpAreaKey* sess;
    GpSprtCmd* cmd;

    sess = &gGameSession->at4.loc;
    if ((u32)(arg0 & 0xFF) < 0xBU) {
        cmd = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1][3].field_4;
        if (arg1 & 0xFF) {
            cmd[arg0 + 1].field_4 = 1;
            return;
        }
        cmd[arg0 + 1].field_4 = 0;
    }
}

void func_dryfield_r08_8017F3B8(u8 arg0, u8 arg1)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    GpSprtCmd* cmd;

    sess = &gGameSession->at4.loc;
    if ((u32)(arg0 & 0xFF) < 3U) {
        rec = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
        if ((u32)(arg0 & 0xFF) == 0U) {
            cmd = rec[1].field_4;
        } else {
            cmd = rec[2].field_4;
        }
        if (arg1 & 0xFF) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}

/// Publishes one of the room's two data banks as the active one: bank 0 for a
/// zero argument, bank 1 otherwise.
void func_dryfield_r08_8017F438(s16 arg0)
{
    if (arg0 == 0) {
        D_dryfield_r08_8017F708 = D_dryfield_r08_801809C0;
        return;
    }
    D_dryfield_r08_8017F708 = D_dryfield_r08_80180B58;
}
