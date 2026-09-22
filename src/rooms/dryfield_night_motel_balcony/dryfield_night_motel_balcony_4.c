#include "common.h"

#include <psyq/inline_c.h>

#include "rooms/room_common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

extern SVECTOR D_dryfield_night_motel_balcony_80182D20;

/// One row of the sprite table `func_dryfield_night_motel_balcony_8017FF78`
/// indexes by `Task::spawnArg1`: `tpageX` selects the texture page, `w` is the
/// frame width (the u step between frames and the billboard scale) and `v` the
/// frame row.
typedef struct {
    u16 tpageX;
    s16 w;
    u8  v;
    u8  pad5;
} _SpriteFrame;

extern _SpriteFrame D_dryfield_night_motel_balcony_80182DE0[];

/// A CLUT origin in VRAM, as `x` in pixels and `y` in rows, packed into a
/// `POLY_FT4` clut word by the caller.
typedef struct {
    s16 x;
    u16 y;
} _ClutOrigin;

extern _ClutOrigin D_dryfield_night_motel_balcony_80182DF4[];

#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")

void func_dryfield_night_motel_balcony_8017FF78(Task* task, u8* color, s32 arg);
void func_dryfield_night_motel_balcony_801819E0(Task* task, s32 arg);
void func_dryfield_night_motel_balcony_8018221C(Task* task, u8* color, s16 tick);

/// Draws one axis-aligned `POLY_FT4` panel of a 0x28-pixel sprite at the packed
/// screen position `arg0` (x in the low half, y in the high half). `arg1` is
/// the ordering-table index, `arg2` the panel width and `arg3` the animation
/// step, which walks frames 2..11 of `D_80111E48`. The quad is `2 * d` wide and
/// `4 * d` tall, anchored three quarters of the way down, and both `d` and the
/// rounded weight `3 * d` are the one reused local the ROM keeps for them.
void func_dryfield_night_motel_balcony_8017F6C8(s32 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    POLY_FT4* prim;
    GpEffUv8* rec;
    s16       idx;
    GpEffUv8* tbl;
    s32       d;
    s32       y;

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2F);
    prim->tpage = 0x29;

    idx        = arg3 % 10 + 2;
    tbl        = D_80111E48;
    rec        = &tbl[idx];
    prim->clut = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
    prim->u0   = rec->u;
    prim->v0   = rec->v;
    prim->u1   = rec->u + 0x27;
    prim->v1   = rec->v;
    prim->u2   = rec->u;
    prim->v2   = rec->v + 0x27;
    prim->u3   = rec->u + 0x27;
    prim->v3   = rec->v + 0x27;

    d        = (arg2 * 0x1F) >> 12;
    prim->x2 = arg0 - d;
    prim->x0 = arg0 - d;
    prim->x3 = arg0 + d;
    prim->x1 = arg0 + d;

    d        = (arg2 * 0x1F) >> 13;
    y        = arg0 >> 16;
    prim->y1 = y - d * 3;
    prim->y0 = y - d * 3;
    prim->y3 = y + d;
    prim->y2 = y + d;

    addPrim(&gGpuCurrentOt[arg1], prim);
}

/// Per-frame handler of a falling room effect task that bounces. The first
/// frame resets the model's rotation to identity, keeps the low twelve bits of
/// `Task::spawnArg1` in `field_18`, sets the speed `field_24` to 0xA0, and rolls
/// a frame period (0..7) into `field_1A`, a start frame into `field_20`, a value
/// into `field_1C` and its per-tick step into `field_28`. When the spawner left
/// no drift it rolls one (negative `spawnArg1`: about +-0x40 across and
/// 0x20..0x11F in y; otherwise about +-0x80 on every axis) and turns it into
/// `field_8`'s frame. `spawnArg1` is then replaced by two bits of its upper half.
/// Later frames step `field_1C`, advance `field_20` once per period and move the
/// model by the drift scaled to `field_24`. When `func_800DE7CC` reports a hit
/// along the view-space step, the move is undone, the drift is bent halfway
/// towards the vector it returns, speed and step are halved and the model moves
/// again; a hit within eight ticks of the previous one at a speed below 0x20
/// moves the task to state 2. Without a hit, `0xA000 / field_24` is added to the
/// drift's y. Both states draw through
/// `func_dryfield_night_motel_balcony_8017FF78`, fading over ticks 60..89 and
/// releasing the task at 90. Event states 2 and 3 suspend it, 4 and above
/// release it at once, and event state 1 freezes the tick and the motion.
void func_dryfield_night_motel_balcony_8017F84C(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    MATRIX*        m;
    s32            half;
    SVECTOR        delta;
    SVECTOR        dir;
    SVECTOR        pos;
    u8             color[3];

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    work->field_22++;

    switch (task->state) {
        case 0:
            m                  = &coord->coord;
            *(s32*)&m->m[0][0] = 0x1000;
            *(s32*)&m->m[0][2] = 0;
            *(s32*)&m->m[1][1] = 0x1000;
            *(s32*)&m->m[2][0] = 0;
            m->m[2][2]         = 0x1000;
            work->field_18     = (u16)task->spawnArg1 & 0xFFF;
            work->field_24     = 0xA0;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1A     = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_20     = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1C     = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_28     = 0x200 - (((u32)Gp_LcgState >> 16) & 0x3FF);
            if ((work->field_10.vx | work->field_10.vy | work->field_10.vz) == 0) {
                if (task->spawnArg1 < 0) {
                    half              = 0x40;
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = half - (((u32)Gp_LcgState >> 16) & 0x7F);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vy = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x20;
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = half - (((u32)Gp_LcgState >> 16) & 0x7F);
                } else {
                    half              = 0x80;
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vy = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                }
                gte_SetRotMatrix(&work->field_8->coord);
                gte_ldv0(&work->field_10);
                gte_rtv0_real();
                gte_stsv(&work->field_10);
            }
            VectorNormalSS(&work->field_10, &work->field_10);
            coord->flg      = 0;
            task->state     = 1;
            task->spawnArg1 = (s16)(task->spawnArg1 >> 16) & 3;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                work->field_1C += work->field_28;
                if ((s16)work->field_1A != 0 && (s16)work->field_22 % (s16)work->field_1A == 0) {
                    work->field_20++;
                }
                gte_lddp(work->field_24);
                gte_ldsv(&work->field_10);
                gte_gpf12_real();
                gte_stsv(&delta);
                coord->coord.t[0] += delta.vx;
                coord->coord.t[1] += delta.vy;
                coord->coord.t[2] += delta.vz;
                coord->flg         = 0;
                gte_SetRotMatrix(&Gfx_ViewWorldMtx);
                gte_ldv0(&delta);
                gte_rtv0_real();
                gte_stsv(&dir);
                pos.vx  = coord->workm.t[0];
                pos.vy  = coord->workm.t[1];
                pos.vz  = coord->workm.t[2];
                dir.vx += pos.vx;
                dir.vy += pos.vy;
                dir.vz += pos.vz;
                if (func_800DE7CC(&dir, &pos, &dir, &pos) == 1) {
                    coord->coord.t[0] -= delta.vx;
                    coord->coord.t[1] -= delta.vy;
                    coord->coord.t[2] -= delta.vz;
                    work->field_10.vx  = (pos.vx >> 1) + (work->field_10.vx >> 1);
                    work->field_10.vy  = pos.vy + (work->field_10.vy >> 1);
                    work->field_10.vz  = (pos.vz >> 1) + (work->field_10.vz >> 1);
                    VectorNormalSS(&work->field_10, &work->field_10);
                    work->field_24 = (s16)work->field_24 >> 1;
                    work->field_28 = (s16)work->field_28 >> 1;
                    gte_lddp(work->field_24);
                    gte_ldsv(&work->field_10);
                    gte_gpf12_real();
                    gte_stsv(&delta);
                    coord->coord.t[0] += delta.vx;
                    coord->coord.t[1] += delta.vy;
                    coord->coord.t[2] += delta.vz;
                    if ((s16)work->field_22 - (s16)work->field_2A < 8 && (s16)work->field_24 < 0x20) {
                        task->state = 2;
                    } else {
                        work->field_2A = work->field_22;
                    }
                } else if ((s16)work->field_24 > 0) {
                    work->field_10.vy += 0xA000 / (s16)work->field_24;
                }
            } else {
                work->field_22--;
            }
            if ((s16)work->field_22 < 60) {
                func_dryfield_night_motel_balcony_8017FF78(task, NULL, task->spawnArg1);
            } else if ((s16)work->field_22 < 90) {
                color[0] = color[1] = color[2] = (90 - (s16)work->field_22) * 4;
                func_dryfield_night_motel_balcony_8017FF78(task, color, task->spawnArg1);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
        case 2:
            if (Gp_State1C->eventState != 0) {
                work->field_22--;
            }
            if ((s16)work->field_22 < 60) {
                func_dryfield_night_motel_balcony_8017FF78(task, NULL, task->spawnArg1);
            } else if ((s16)work->field_22 < 90) {
                color[0] = color[1] = color[2] = (90 - (s16)work->field_22) * 4;
                func_dryfield_night_motel_balcony_8017FF78(task, color, task->spawnArg1);
            } else {
            release:
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws the task's model position as a rotated billboard `POLY_FT4`, taking
/// its texture frame from row `Task::spawnArg1` of the sprite table and column
/// `field_20 & 7`. The quad's half-extent is the frame width times `field_18`
/// divided by the projected depth, rotated by `field_1C`. A non-NULL `color`
/// tints the quad and makes it semi-transparent; NULL draws it raw. `arg` is
/// unused. The block pointer goes through an `asm` move for the same reason as
/// in `func_dryfield_night_motel_balcony_8018221C`.
void func_dryfield_night_motel_balcony_8017FF78(Task* task, u8* color, s32 arg)
{
    RoomEffWork*      work  = task->spawnArg2;
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               size;
    u16               vx;

    size                                       = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w - 1;
    head                                       = *(void**)G_SCRATCH_HEAD;
    vx                                         = *(u16*)&coord->workm.t[0];
    vecp                                       = (GpEffBeamScratch*)(head - 0x1C);
    *(void**)G_SCRATCH_HEAD                    = vecp;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
    __asm__("move %0,%1" : "=r"(block) : "r"(vecp));
    block->vec.vy = *(u16*)&coord->workm.t[1];
    block->vec.vz = *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = ((D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].tpageX & 0x3FF) >> 6) | 0x20;
        prim->clut  = getClut(task->spawnArg1 * 16, 0x10F);
        prim->u0    = (work->field_20 & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w;
        prim->v0    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v;
        prim->u1    = (work->field_20 & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w + size;
        prim->v1    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v;
        prim->u2    = (work->field_20 & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w;
        prim->v2    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v + size;
        prim->u3    = (work->field_20 & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w + size;
        prim->v3    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v + size;
        block->dx   = (((size * (s16)work->field_18) / block->otz) * rsin((s16)work->field_1C)) >> 12;
        block->dy   = (((size * (s16)work->field_18) / block->otz) * rcos((s16)work->field_1C)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((size * (s16)work->field_18) / block->otz) * rsin((s16)work->field_1C + 0x400)) >> 12;
        block->dy   = (((size * (s16)work->field_18) / block->otz) * rcos((s16)work->field_1C + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void func_dryfield_night_motel_balcony_80180580(Task* task)
{
    void*          work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            i;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }
    switch (task->state) {
        case 0:
            task->state = task->spawnArg1 * 2 + 1;
            break;
        case 1:
            for (i = 0; i < 8; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0xFF) | 0x100, NULL);
            }
            task->state = 2;
            break;
        case 2:
            for (i = 0; i < 4; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0xFF) | 0x10100, NULL);
                Gp_SpawnEff(0x60095, coord, 0x400, NULL);
            }
            task->state = 10;
            break;
        case 3:
            for (i = 0; i < 6; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x80, NULL);
            }
            task->state = 4;
            break;
        case 4:
            for (i = 0; i < 3; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x10080, NULL);
                Gp_SpawnEff(0x60095, coord, 0x400, NULL);
            }
            task->state = 10;
            break;
        case 5:
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(0x60095, coord, 0x40000300, NULL);
            }
            task->state = 6;
            break;
        case 6:
            for (i = 0; i < 4; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x80000080, NULL);
            }
            for (i = 0; i < 2; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x80010080, NULL);
            }
            task->state = 10;
            break;
        case 7:
            for (i = 0; i < 8; i++) {
                Gp_SpawnEff(0x60095, coord, 0x10400, NULL);
            }
            task->state = 8;
            break;
        case 8:
            for (i = 0; i < 8; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0xFF) | 0x100, NULL);
            }
            task->state = 10;
            break;
        case 9:
            for (i = 0; i < 8; i++) {
                Gp_SpawnEff(0x60095, coord, 0x10400, NULL);
            }
            task->state = 10;
            break;
        case 10:
        release:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_801809CC);

/// Projects the task model's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues one `POLY_FT4` billboard (tpage 0x2C, clut
/// 0x43C3) centred on it. `field_20 % 6` picks one of six 40-texel columns at
/// v 0x40..0x67, and the half-extent is `field_18 * 39 / otz` on both axes.
/// `color` modulates the texture and makes the quad semi-transparent; NULL
/// draws the texture raw and opaque. The block pointer goes through the same
/// `asm` move as `func_dryfield_night_motel_balcony_8018221C`, for the same
/// reason.
void func_dryfield_night_motel_balcony_80180C60(Task* task, u8* color)
{
    RoomEffWork*       work;
    GsCOORDINATE2*     coord;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    SVECTOR*           vec;
    s16                xy;
    u16                vz;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;

    head                                        = *(void**)G_SCRATCH_HEAD;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&coord->workm.t[0];
    vec                                         = (SVECTOR*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(vec));
    block->vec.vy           = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = block;
    block->vec.vz           = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage   = 0x2C;
        prim->clut    = 0x43C3;
        prim->u0      = (s16)work->field_20 % 6 * 40;
        prim->v0      = 0x40;
        prim->u1      = (s16)work->field_20 % 6 * 40 + 0x27;
        prim->v1      = 0x40;
        prim->u2      = (s16)work->field_20 % 6 * 40;
        prim->v2      = 0x67;
        prim->u3      = (s16)work->field_20 % 6 * 40 + 0x27;
        prim->v3      = 0x67;
        block->radius = (s16)work->field_18 * 39 / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Per-frame handler of an effect-spawning room task. Any non-zero event state
/// suspends it, and 4 or above releases it. In view 0x27 it makes three
/// independent LCG rolls each frame: 1 in 4 spawns effect 0x6003D and 1 in 3
/// spawns 0x60093, both with an offset of up to 0x100 on every axis, and 1 in 7
/// spawns 0x60095 with a horizontal offset of up to 0x80. In any other view it
/// counts `field_22` up to 150 frames and then releases itself. Until then it
/// makes two rolls that fire less often as the count grows (the count must be
/// below a draw modulo 150, then modulo 120), each followed by a 1-in-4 roll
/// that spawns 0x60095, first with a vertical offset of up to 0x7FF and then at
/// a fixed height of 0xC00.
void func_dryfield_night_motel_balcony_80181024(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            lo;
    s32            arg;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }
    if (gGameSession->at4.loc.view == 0x27) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            lo                = ((u32)Gp_LcgState >> 16) & 0x1FF;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            arg               = ((((u32)Gp_LcgState >> 16) % 3) << 16) + 0x80000100;
            Gp_SpawnEff(0x6003D, coord, lo + arg, &work->field_10);
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 3U) == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60093, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x100, &work->field_10);
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 7U) == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            work->field_10.vy = 0;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0xA0000400, &work->field_10);
        }
    } else {
        work->field_22++;
        if ((s16)work->field_22 < 150) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((s16)work->field_22 < (u16)(((u32)Gp_LcgState >> 16) % 150U)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vy = ((u32)Gp_LcgState >> 16) & 0x7FF;
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x80000400,
                                &work->field_10);
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((s16)work->field_22 < (u16)(((u32)Gp_LcgState >> 16) % 120U)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    work->field_10.vy = 0xC00;
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x20010400,
                                &work->field_10);
                }
            }
        } else {
        release:
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Per-frame handler of a drifting room effect task, a variant of
/// `func_dryfield_night_motel_balcony_80181E7C`. The first frame resets the
/// model's rotation to identity, keeps the low twelve bits of
/// `Task::spawnArg1` in `field_18`, rolls a frame period (1..4 ticks) into
/// `field_1A` and a value into `field_1C`, and, when the spawner left no drift,
/// rolls one whose ranges depend on `spawnArg1` (bit 30: +-0x80 on every axis;
/// negative: +-0x10 across and 0..-0xFF in y; otherwise +-0x80 across and
/// 0..15 in y) and turns it into `field_8`'s frame. The drift is normalised and scaled to `field_24`
/// (0x40 with bit 30 or bit 29, else 0x80), and `spawnArg1` is replaced by
/// two bits of its upper half. Later frames advance `field_20` once per period,
/// move the model by the drift, decrementing its y by one a tick, and hand
/// the task to `func_dryfield_night_motel_balcony_801819E0` until `field_20`
/// reaches 12, when it is released. Event states 2 and 3 suspend it, 4 and
/// above release it at once, and state 1 freezes the drift and the tick.
void func_dryfield_night_motel_balcony_8018158C(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    MATRIX*        m;
    s32            half; // default drift length and the centre of the wide drift rolls

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    half = 0x80;
    work->field_22++;

    switch (task->state) {
        case 0:
            m                  = &coord->coord;
            *(s32*)&m->m[0][0] = 0x1000;
            *(s32*)&m->m[0][2] = 0;
            *(s32*)&m->m[1][1] = 0x1000;
            *(s32*)&m->m[2][0] = 0;
            m->m[2][2]         = 0x1000;
            work->field_18     = (u16)task->spawnArg1 & 0xFFF;
            work->field_24     = half;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1A     = (((u32)Gp_LcgState >> 16) & 3) + 1;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1C     = ((u32)Gp_LcgState >> 16) & 0xFFF;
            work->field_20     = 0;
            if ((work->field_10.vx | work->field_10.vy | work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0x40000000) {
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vy = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    work->field_24    = 0x40;
                } else {
                    if (task->spawnArg1 < 0) {
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                    } else {
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = ((u32)Gp_LcgState >> 16) & 0xF;
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    }
                    if (task->spawnArg1 & 0x20000000) {
                        work->field_24 = 0x40;
                    }
                }
                gte_SetRotMatrix(&work->field_8->coord);
                gte_ldv0(&work->field_10);
                gte_rtv0_real();
                gte_stsv(&work->field_10);
            }
            VectorNormalSS(&work->field_10, &work->field_10);
            gte_lddp(work->field_24);
            gte_ldsv(&work->field_10);
            gte_gpf12_real();
            gte_stsv(&work->field_10);
            coord->flg      = 0;
            task->state     = 1;
            task->spawnArg1 = (s16)(task->spawnArg1 >> 16) & 3;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                if ((s16)work->field_22 % (s16)work->field_1A == 0) {
                    work->field_20++;
                }
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
                work->field_10.vy--;
            } else {
                work->field_22--;
            }
            if ((s16)work->field_20 < 12) {
                func_dryfield_night_motel_balcony_801819E0(task, task->spawnArg1);
            } else {
            release:
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Projects the task model's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues one semi-transparent `POLY_FT4` billboard
/// on tpage 0x2C centred on it. `field_20` is the animation frame: it picks a
/// 48-texel cell of a five-column sheet starting at v 0x68, and steps the CLUT
/// x by 16 per frame from the origin `arg` selects in
/// `D_dryfield_night_motel_balcony_80182DF4`. The half-extent is
/// `field_18 * 47 / (otz + 1)` on both axes.
void func_dryfield_night_motel_balcony_801819E0(Task* task, s32 arg)
{
    RoomEffWork*       work;
    GsCOORDINATE2*     coord;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    _ClutOrigin*       clut;
    SVECTOR*           vec;
    s16                xy;
    u16                vz;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;

    head                                        = *(void**)G_SCRATCH_HEAD;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD                     = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage   = 0x2C;
        clut          = &D_dryfield_night_motel_balcony_80182DF4[arg];
        prim->clut    = (clut->y << 6) | (((clut->x + (s16)work->field_20 * 16) >> 4) & 0x3F);
        prim->u0      = (s16)work->field_20 % 5 * 48;
        prim->v0      = (s16)work->field_20 / 5 * 48 + 0x68;
        prim->u1      = (s16)work->field_20 % 5 * 48 + 0x2F;
        prim->v1      = (s16)work->field_20 / 5 * 48 + 0x68;
        prim->u2      = (s16)work->field_20 % 5 * 48;
        prim->v2      = (s16)work->field_20 / 5 * 48 + 0x97;
        prim->u3      = (s16)work->field_20 % 5 * 48 + 0x2F;
        prim->v3      = (s16)work->field_20 / 5 * 48 + 0x97;
        block->radius = (s16)work->field_18 * 0x2F / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Per-frame handler of a drifting room effect task. The first frame resets the
/// model's rotation to identity, rolls a starting animation step (0..9) and a
/// lifetime (5..14 ticks), and, when the spawner left no drift, rolls one and
/// turns it into `field_8`'s frame. The drift is then normalised and scaled to a
/// length chosen by `Task::spawnArg1` (8 when negative, 0x80 with bit 30, 0x20
/// otherwise). Later frames move the model by the drift, bending it by one
/// unit a tick, and draw it through `func_dryfield_night_motel_balcony_8018221C`,
/// fading its colour over the last ten ticks before releasing the task. Event
/// states 2 and 3 suspend it; 4 and above release it at once, and any non-zero
/// state below that freezes the drift and the lifetime tick.
void func_dryfield_night_motel_balcony_80181E7C(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    MATRIX*        m;
    s32            seed;
    s16            tick;
    s16            end;
    u8             color[3];

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    work->field_22++;

    switch (task->state) {
        case 0:
            seed               = Gp_LcgState * 5 + 0x71357911;
            m                  = &coord->coord;
            *(s32*)&m->m[0][0] = 0x1000;
            *(s32*)&m->m[0][2] = 0;
            *(s32*)&m->m[1][1] = 0x1000;
            *(s32*)&m->m[2][0] = 0;
            m->m[2][2]         = 0x1000;
            work->field_18     = task->spawnArg1 & 0xFFF;
            Gp_LcgState        = seed;
            work->field_20     = ((u32)Gp_LcgState >> 16) % 10;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_26     = ((u32)Gp_LcgState >> 16) % 10 + 5;
            if ((work->field_10.vx | work->field_10.vy | work->field_10.vz) == 0) {
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                if (task->spawnArg1 < 0) {
                    work->field_24 = 8;
                } else if (task->spawnArg1 & 0x40000000) {
                    work->field_24 = 0x80;
                } else {
                    work->field_24 = 0x20;
                }
                gte_SetRotMatrix(&work->field_8->coord);
                gte_ldv0(&work->field_10);
                gte_rtv0_real();
                gte_stsv(&work->field_10);
            }
            VectorNormalSS(&work->field_10, &work->field_10);
            gte_lddp(work->field_24);
            gte_ldsv(&work->field_10);
            gte_gpf12_real();
            gte_stsv(&work->field_10);
            coord->flg  = 0;
            task->state = 1;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                work->field_20++;
                work->field_10.vy--;
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
            } else {
                work->field_22--;
            }
            tick = work->field_22;
            end  = work->field_26;
            if (tick < end - 10) {
                func_dryfield_night_motel_balcony_8018221C(task, NULL, tick);
            } else if (tick < end) {
                color[0] = color[1] = color[2] = (end - tick) * 12;
                func_dryfield_night_motel_balcony_8018221C(task, color, tick);
            } else {
            release:
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws a drifting effect task's sprite: projects its model's world position
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x2B). The animation frame is
/// `field_20 % 10`; it picks the CLUT column and one 48-texel cell of a 5x2
/// grid starting at v=0x28. The quad is centred on the projected point with a
/// half-width of `field_18 * 47 / otz` and extends three quarters above and one
/// quarter below. `color` is the RGB the texture is modulated by; NULL draws
/// the texture raw.
/// `tick` is unused. The block pointer is copied through an `asm` move because
/// the ROM keeps the scratch-block address in a temporary and copies it into the
/// pointer's own register, a copy no C spelling found here survives combine with.
void func_dryfield_night_motel_balcony_8018221C(Task* task, u8* color, s16 tick)
{
    RoomEffWork*       work = task->spawnArg2;
    GsCOORDINATE2*     coord;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    SVECTOR*           vec;
    s16                frame;
    s32                u0;
    s32                u1;
    s32                vTop;
    s32                vBottom;
    s16                xy;
    u16                vz;

    frame = (s16)work->field_20 % 10;
    coord = ((TmdObject*)task->extra)->coords;

    head                                        = *(void**)G_SCRATCH_HEAD;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&coord->workm.t[0];
    vec                                         = (SVECTOR*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(vec));
    block->vec.vy           = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = block;
    block->vec.vz           = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = 0x2B;
        prim->clut  = getClut(frame * 16 + 0x40, 0x10E);
        setSemiTrans(prim, 1);
        u0              = frame % 5 * 48;
        vTop            = frame / 5 * 48;
        u1              = u0 + 0x2F;
        vBottom         = vTop + 0x57;
        vTop            = vTop + 0x28;
        prim->u0        = u0;
        prim->v0        = vTop;
        prim->u1        = u1;
        prim->v1        = vTop;
        prim->u2        = u0;
        prim->v2        = vBottom;
        prim->u3        = u1;
        prim->v3        = vBottom;
        block->radius   = (s16)work->field_18 * 47 / block->otz;
        xy              = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2        = xy;
        prim->x0        = xy;
        xy              = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3        = xy;
        prim->x1        = xy;
        block->radius >>= 1;
        xy              = *(u16*)&block->sy - block->radius * 3;
        prim->y1        = xy;
        prim->y0        = xy;
        xy              = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3        = xy;
        prim->y2        = xy;
        ds              = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Spawns an 8-step burst of effect 0x6007E and then a 6-step burst of 0x60070
/// around part 3 of the model owned by the slot-4 task's child. Each step rolls
/// the room LCG four times (three for the second burst) and builds the offset
/// vector from the top byte of each draw; the first burst also carries the last
/// draw's low nine bits, biased by 0x300, in the spawn argument.
void func_dryfield_night_motel_balcony_8018257C(void)
{
    Task*          task;
    GsCOORDINATE2* coord;
    SVECTOR        sv;
    s32            i;

    task  = gameGetPtrSlot(4);
    coord = ((TmdObject*)task->firstChild->extra)->coords + 3;

    for (i = 0; i < 8; i++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vx       = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vy       = 0xFE80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vz       = 0x680 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x6007E, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x300, &sv);
    }

    for (i = 0; i < 6; i++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vx       = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vy       = 0xFE80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vz       = 0x680 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_SpawnEff(0x60070, coord, 0xC0033800, &sv);
    }
}

/// Rolls the room LCG (`Gp_LcgState`) once and, on a draw whose upper half is
/// a multiple of three, rolls it again and spawns effect 0x6007E at part 3 of
/// the model owned by the slot-4 task's child, carrying the second draw's low
/// nine bits in the upper half of the spawn argument.
void func_dryfield_night_motel_balcony_80182730(void)
{
    Task* task;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((u16)(((u32)Gp_LcgState >> 16) % 3U) == 0) {
        task        = gameGetPtrSlot(4);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x6007E, ((TmdObject*)task->firstChild->extra)->coords + 3,
                    (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x80000100,
                    &D_dryfield_night_motel_balcony_80182D20);
    }
}
