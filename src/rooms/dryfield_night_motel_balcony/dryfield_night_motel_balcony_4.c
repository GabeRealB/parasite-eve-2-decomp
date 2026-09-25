#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_night_motel_balcony_80182C60[];
extern SVECTOR D_dryfield_night_motel_balcony_80182C70;
extern SVECTOR D_dryfield_night_motel_balcony_80182C80;
extern SVECTOR D_dryfield_night_motel_balcony_80182C90;
extern SVECTOR D_dryfield_night_motel_balcony_80182CA0;
extern SVECTOR D_dryfield_night_motel_balcony_80182CF0;
extern SVECTOR D_dryfield_night_motel_balcony_80182D00;
extern SVECTOR D_dryfield_night_motel_balcony_80182D08;
extern SVECTOR D_dryfield_night_motel_balcony_80182D10;
extern SVECTOR D_dryfield_night_motel_balcony_80182D18;
extern SVECTOR D_dryfield_night_motel_balcony_80182D28;
extern SVECTOR D_dryfield_night_motel_balcony_80182D30;
extern SVECTOR D_dryfield_night_motel_balcony_80182D38;
extern s32     D_dryfield_night_motel_balcony_80182D40[2][20];
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

void func_dryfield_night_motel_balcony_8017EC58(SVECTOR* arg0, s32 arg1);
void func_dryfield_night_motel_balcony_8017F440(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_dryfield_night_motel_balcony_8017FF78(Task* task, u8* color, s32 arg);
void func_dryfield_night_motel_balcony_80180C60(Task* task, u8* color, s32 unused);
void func_dryfield_night_motel_balcony_801819E0(Task* task, s32 arg);
void func_dryfield_night_motel_balcony_8018221C(Task* task, u8* color, s16 tick);

/// The room's ambient effect task. Each tick it draws the glows whose bit for
/// the current view is set in the per-view mask table, turns two of them off
/// for good once flag nibble 0x7F is set (spawning effect 0x60094 the first
/// time), and runs the current view's timed effect bursts off the work
/// block's two counters.
void func_dryfield_night_motel_balcony_8017E554(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        hi;
    s32        mask;
    s32        i;
    s32        n;
    s16        cnt;
    SVECTOR    pos;
    SVECTOR    ofs;

    work                    = task->spawnArg2;
    coord                   = task->extra.tmd->coords;
    Gp_State1C->groundShade = 0xFF;
    hi                      = 0;
    if (gGameSession->at4.loc.view < 0x20) {
        mask = 1 << gGameSession->at4.loc.view;
    } else {
        mask = 1 << (gGameSession->at4.loc.view - 0x20);
        hi   = 1;
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][0]) {
        func_dryfield_night_motel_balcony_8017EC58(&D_dryfield_night_motel_balcony_80182C60[0], 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][2]) {
        func_dryfield_night_motel_balcony_8017EC58(&D_dryfield_night_motel_balcony_80182C70, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][4]) {
        func_dryfield_night_motel_balcony_8017EC58(&D_dryfield_night_motel_balcony_80182C80, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][6]) {
        func_dryfield_night_motel_balcony_8017EC58(&D_dryfield_night_motel_balcony_80182C90, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][8]) {
        func_dryfield_night_motel_balcony_8017EC58(&D_dryfield_night_motel_balcony_80182CA0, 0x180);
    }
    for (i = 10; i < 18; i++) {
        if (mask & D_dryfield_night_motel_balcony_80182D40[hi][i]) {
            func_dryfield_night_motel_balcony_8017F440(&D_dryfield_night_motel_balcony_80182C60[i], 1, 0x380);
        }
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][18]) {
        func_dryfield_night_motel_balcony_8017EC58(&D_dryfield_night_motel_balcony_80182CF0, 0x180);
    }
    if (GameFlag_GetNibble(0x7F) == 1) {
        D_dryfield_night_motel_balcony_80182D40[0][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[0][2] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][2] = 0;
        Gp_SpawnEff(0x60094, coord, 0, &D_dryfield_night_motel_balcony_80182C70);
        GameFlag_SetNibble(0x7F, 2);
    } else if (GameFlag_GetNibble(0x7F) == 2) {
        D_dryfield_night_motel_balcony_80182D40[0][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[0][2] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][2] = 0;
    }
    switch (gGameSession->at4.loc.view) {
        case 17:
            if (++work->angle == 0x5C) {
                Gp_SpawnEff(0x60095, coord, 0x40000300, &D_dryfield_night_motel_balcony_80182D28);
                Gp_SpawnEff(0x60095, coord, 0x40000300, &D_dryfield_night_motel_balcony_80182D28);
                for (i = 0; i < 3; i++) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6003D, coord, ((Gp_LcgState >> 16) & 0xFF) | 0x80010100,
                                &D_dryfield_night_motel_balcony_80182D28);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6003D, coord, ((Gp_LcgState >> 16) & 0x7F) | 0x80000080,
                                &D_dryfield_night_motel_balcony_80182D28);
                }
            }
            break;
        case 18:
            if (++work->scale == 0x3F) {
                Gp_SpawnEff(0x60050, coord, 3, &D_dryfield_night_motel_balcony_80182D08);
                work->angle = 0;
            }
            break;
        case 21:
            cnt = ++work->angle;
            if (cnt >= 0x47) {
                n = cnt - 0x46;
                if ((s16)(cnt % 6) == 0) {
                    memset(&ofs, 0, sizeof(ofs));
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    ofs.vx      = -((s32)(Gp_LcgState >> 16) % (n * 10));
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    ofs.vy      = (s32)(Gp_LcgState >> 16) % (n * 10);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    ofs.vz      = (s32)(Gp_LcgState >> 16) % (n * 10);
                    pos         = ofs;
                    pos.vx     += D_dryfield_night_motel_balcony_80182D30.vx;
                    pos.vy     += D_dryfield_night_motel_balcony_80182D30.vy;
                    pos.vz     += D_dryfield_night_motel_balcony_80182D30.vz;
                    Gp_SpawnEff(0x6007E, coord, n * 0x28 + 0x40000600, &pos);
                }
                work->scale = 0;
            }
            break;
        case 19:
            if (++work->scale == 0x2B) {
                Gp_SpawnEff(0x60050, coord, 4, &D_dryfield_night_motel_balcony_80182D10);
            }
            break;
        case 20:
            if (++work->scale == 0xC) {
                Gp_SpawnEff(0x60050, coord, 2, &D_dryfield_night_motel_balcony_80182D00);
            }
            break;
        case 23:
            if (++work->scale == 0xC) {
                Gp_SpawnEff(0x60050, coord, 2, &D_dryfield_night_motel_balcony_80182D38);
            }
            break;
        case 29:
            if (++work->scale == 0x41) {
                for (i = 0; i < 3; i++) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6003D, coord, ((Gp_LcgState >> 16) & 0x7F) | 0x80000080,
                                &D_dryfield_night_motel_balcony_80182D18);
                }
            }
            break;
        default:
            work->scale = 0;
            work->angle = 0;
            break;
    }
}

/// Projects the world-space points `arg0[0]` and `arg0[1]` through
/// `gGfxViewCoord.workm` and, when both project, joins them with a glowing
/// capsule of gouraud `POLY_G4`s: a wedge fan around each projected centre and
/// a strip between them, three quads per 0x400 step across half a turn
/// anchored to the screen-space angle between the two centres. `arg1` is a
/// half-extent scaled by depth (`arg1 * 64 / otz`); the lit vertices take the
/// grey `((animFrame & 1) * 16) | 0x20`, flickering with the frame counter,
/// and the rim is black.
void func_dryfield_night_motel_balcony_8017EC58(SVECTOR* arg0, s32 arg1)
{
    void**                   scratch;
    u8*                      head;
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = (((u8)ds->animFrame & 1) * 0x10) | 0x20;
            angEnd    = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
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

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
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
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the point `arg0` through `gGfxViewCoord.workm` and, when the GTE flag
/// is non-negative, queues one semi-transparent `POLY_FT4` sprite centred on
/// it: tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`, UV column `(s16)arg1 * 40`,
/// on-screen half-extent `(s16)arg2 * 39 / otz`, and a grey that alternates
/// between 0x20 and 0x30 with `animFrame`.
void func_dryfield_night_motel_balcony_8017F440(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex      = arg1;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = ((RoomDraw13Scratch*)tmp)->sx - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sx + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = ((RoomDraw13Scratch*)tmp)->sy + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

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
/// `Task::spawnArg1` in `pos.vx`, sets the speed `scale` to 0xA0, and rolls
/// a frame period (0..7) into `pos.vy`, a start frame into `index`, a value
/// into `pos.vz` and its per-tick step into `period`. When the spawner left
/// no drift it rolls one (negative `spawnArg1`: about +-0x40 across and
/// 0x20..0x11F in y; otherwise about +-0x80 on every axis) and turns it into
/// `parent`'s frame. `spawnArg1` is then replaced by two bits of its upper half.
/// Later frames step `pos.vz`, advance `index` once per period and move the
/// model by the drift scaled to `scale`. When `func_800DE7CC` reports a hit
/// along the view-space step, the move is undone, the drift is bent halfway
/// towards the vector it returns, speed and step are halved and the model moves
/// again; a hit within eight ticks of the previous one at a speed below 0x20
/// moves the task to state 2. Without a hit, `0xA000 / scale` is added to the
/// drift's y. Both states draw through
/// `func_dryfield_night_motel_balcony_8017FF78`, fading over ticks 60..89 and
/// releasing the task at 90. Event states 2 and 3 suspend it, 4 and above
/// release it at once, and event state 1 freezes the tick and the motion.
void func_dryfield_night_motel_balcony_8017F84C(Task* task)
{
    GpEffWork* work  = task->spawnArg2;
    GpCoord*   coord = task->extra.tmd->coords;
    MATRIX*    m;
    s32        half;
    SVECTOR    delta;
    SVECTOR    dir;
    SVECTOR    pos;
    u8         color[3];

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    work->age++;

    switch (task->state) {
        case 0:
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = (u16)task->spawnArg1 & 0xFFF;
            work->scale          = 0xA0;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->pos.vy         = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->index          = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->pos.vz         = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->period         = 0x200 - (((u32)Gp_LcgState >> 16) & 0x3FF);
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 < 0) {
                    half          = 0x40;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vx = half - (((u32)Gp_LcgState >> 16) & 0x7F);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vy = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x20;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vz = half - (((u32)Gp_LcgState >> 16) & 0x7F);
                } else {
                    half          = 0x80;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vy = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            coord->flg      = 0;
            task->state     = 1;
            task->spawnArg1 = (s16)(task->spawnArg1 >> 16) & 3;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                work->pos.vz += work->period;
                if (work->pos.vy != 0 && work->age % work->pos.vy == 0) {
                    work->index++;
                }
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&delta);
                coord->coord.t[0] += delta.vx;
                coord->coord.t[1] += delta.vy;
                coord->coord.t[2] += delta.vz;
                coord->flg         = 0;
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&delta);
                gte_rtv0();
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
                    work->move.vx      = (pos.vx >> 1) + (work->move.vx >> 1);
                    work->move.vy      = pos.vy + (work->move.vy >> 1);
                    work->move.vz      = (pos.vz >> 1) + (work->move.vz >> 1);
                    VectorNormalSS(&work->move, &work->move);
                    work->scale  = work->scale >> 1;
                    work->period = work->period >> 1;
                    gte_lddp(work->scale);
                    gte_ldsv(&work->move);
                    gte_gpf12();
                    gte_stsv(&delta);
                    coord->coord.t[0] += delta.vx;
                    coord->coord.t[1] += delta.vy;
                    coord->coord.t[2] += delta.vz;
                    if (work->age - work->step < 8 && work->scale < 0x20) {
                        task->state = 2;
                    } else {
                        work->step = work->age;
                    }
                } else if (work->scale > 0) {
                    work->move.vy += 0xA000 / work->scale;
                }
            } else {
                work->age--;
            }
            if (work->age < 60) {
                func_dryfield_night_motel_balcony_8017FF78(task, NULL, task->spawnArg1);
            } else if (work->age < 90) {
                color[0] = color[1] = color[2] = (90 - work->age) * 4;
                func_dryfield_night_motel_balcony_8017FF78(task, color, task->spawnArg1);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
        case 2:
            if (Gp_State1C->eventState != 0) {
                work->age--;
            }
            if (work->age < 60) {
                func_dryfield_night_motel_balcony_8017FF78(task, NULL, task->spawnArg1);
            } else if (work->age < 90) {
                color[0] = color[1] = color[2] = (90 - work->age) * 4;
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
/// `index & 7`. The quad's half-extent is the frame width times `pos.vx`
/// divided by the projected depth, rotated by `pos.vz`. A non-NULL `color`
/// tints the quad and makes it semi-transparent; NULL draws it raw. `arg` is
/// unused. The block pointer goes through an `asm` move for the same reason as
/// in `func_dryfield_night_motel_balcony_8018221C`.
void func_dryfield_night_motel_balcony_8017FF78(Task* task, u8* color, s32 arg)
{
    GpEffWork*       work  = task->spawnArg2;
    GpCoord*         coord = task->extra.tmd->coords;
    u8*              head;
    GpFxQuadScratch* block;
    GpFxQuadScratch* vecp;
    POLY_FT4*        prim;
    s16              size;
    u16              vx;

    size                                      = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w - 1;
    head                                      = SCRATCH_HEAD(void);
    vx                                        = (u16)coord->workm.t[0];
    vecp                                      = (GpFxQuadScratch*)(head - 0x1C);
    SCRATCH_HEAD(void)                        = vecp;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = vx;
    __asm__("move %0,%1" : "=r"(block) : "r"(vecp));
    block->vec.vy = (u16)coord->workm.t[1];
    block->vec.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
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
        prim->u0    = (work->index & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w;
        prim->v0    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v;
        prim->u1    = (work->index & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w + size;
        prim->v1    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v;
        prim->u2    = (work->index & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w;
        prim->v2    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v + size;
        prim->u3    = (work->index & 7) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].w + size;
        prim->v3    = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1].v + size;
        block->dx   = (((size * work->pos.vx) / block->otz) * rsin(work->pos.vz)) >> 12;
        block->dy   = (((size * work->pos.vx) / block->otz) * rcos(work->pos.vz)) >> 12;
        prim->x0    = block->sx + (u16)block->dx;
        prim->x3    = block->sx - (u16)block->dx;
        prim->y0    = block->sy - (u16)block->dy;
        prim->y3    = block->sy + (u16)block->dy;
        block->dx   = (((size * work->pos.vx) / block->otz) * rsin(work->pos.vz + 0x400)) >> 12;
        block->dy   = (((size * work->pos.vx) / block->otz) * rcos(work->pos.vz + 0x400)) >> 12;
        prim->x1    = block->sx + (u16)block->dx;
        prim->x2    = block->sx - (u16)block->dx;
        prim->y1    = block->sy - (u16)block->dy;
        prim->y2    = block->sy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_dryfield_night_motel_balcony_80180580(Task* task)
{
    void*    work  = task->spawnArg2;
    GpCoord* coord = task->extra.tmd->coords;
    s32      i;

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

void func_dryfield_night_motel_balcony_801809CC(Task* task)
{
    GpEffWork*  work;
    GpCoord*    coord;
    GpMtxWords* rot;
    s16         flag;
    u16         age;
    s16         t;
    u8          color[3];

    work  = task->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = task->extra.tmd->coords;
    if (flag >= 2) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    Gp_UpdateCoord(coord);
    age       = work->age;
    work->age = age + 1;
    switch (task->state) {
        case 0:
            rot           = (GpMtxWords*)&coord->coord;
            rot->m00_m01  = 0x1000;
            rot->m02_m10  = 0;
            rot->m11_m12  = 0x1000;
            rot->m20_m21  = 0;
            rot->m22      = 0x1000;
            work->pos.vx  = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            work->scale   = 0xA0;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->index   = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = ((u32)Gp_LcgState >> 16) & 0xFF;
            task->state   = 1;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                work->index++;
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                if (coord->coord.t[1] > 0) {
                    if (work->age < 0x1E) {
                        Gp_SpawnEff(0x60095, coord, work->pos.vx + 0x20010400, NULL);
                    }
                    task->state = 2;
                } else if (work->scale > 0) {
                    work->move.vy += 6;
                }
            } else {
                work->age = age;
            }
            t = work->age;
            if (t < 0x14) {
                func_dryfield_night_motel_balcony_80180C60(task, NULL, 0);
            } else if (t < 0x1E) {
                color[0] = color[1] = color[2] = (0x1E - t) * 0xC;
                func_dryfield_night_motel_balcony_80180C60(task, color, 0);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
        case 2:
            if (Gp_State1C->eventState != 0) {
                work->age = age;
            }
            t = work->age;
            if (t < 0x14) {
                func_dryfield_night_motel_balcony_80180C60(task, NULL, 0);
            } else if (t < 0x1E) {
                color[0] = color[1] = color[2] = (0x1E - t) * 0xC;
                func_dryfield_night_motel_balcony_80180C60(task, color, 0);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Projects the task model's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues one `POLY_FT4` billboard (tpage 0x2C, clut
/// 0x43C3) centred on it. `index % 6` picks one of six 40-texel columns at
/// v 0x40..0x67, and the half-extent is `pos.vx * 39 / otz` on both axes.
/// `color` modulates the texture and makes the quad semi-transparent; NULL
/// draws the texture raw and opaque. The block pointer goes through the same
/// `asm` move as `func_dryfield_night_motel_balcony_8018221C`, for the same
/// reason. The third argument is never read; every caller passes 0.
void func_dryfield_night_motel_balcony_80180C60(Task* task, u8* color, s32 unused)
{
    GpEffWork*     work;
    GpCoord*       coord;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    SVECTOR*       vec;
    s16            xy;
    u16            vz;

    coord = task->extra.tmd->coords;
    work  = task->spawnArg2;

    head                                    = SCRATCH_HEAD(void);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)coord->workm.t[0];
    vec                                     = (SVECTOR*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(vec));
    block->vec.vy      = (u16)coord->workm.t[1];
    vz                 = (u16)coord->workm.t[2];
    SCRATCH_HEAD(void) = block;
    block->vec.vz      = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
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
        prim->tpage = 0x2C;
        prim->clut  = 0x43C3;
        prim->u0    = work->index % 6 * 40;
        prim->v0    = 0x40;
        prim->u1    = work->index % 6 * 40 + 0x27;
        prim->v1    = 0x40;
        prim->u2    = work->index % 6 * 40;
        prim->v2    = 0x67;
        prim->u3    = work->index % 6 * 40 + 0x27;
        prim->v3    = 0x67;
        block->step = work->pos.vx * 39 / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame handler of an effect-spawning room task. Any non-zero event state
/// suspends it, and 4 or above releases it. In view 0x27 it makes three
/// independent LCG rolls each frame: 1 in 4 spawns effect 0x6003D and 1 in 3
/// spawns 0x60093, both with an offset of up to 0x100 on every axis, and 1 in 7
/// spawns 0x60095 with a horizontal offset of up to 0x80. In any other view it
/// counts `age` up to 150 frames and then releases itself. Until then it
/// makes two rolls that fire less often as the count grows (the count must be
/// below a draw modulo 150, then modulo 120), each followed by a 1-in-4 roll
/// that spawns 0x60095, first with a vertical offset of up to 0x7FF and then at
/// a fixed height of 0xC00.
void func_dryfield_night_motel_balcony_80181024(Task* task)
{
    GpEffWork* work  = task->spawnArg2;
    GpCoord*   coord = task->extra.tmd->coords;
    s32        lo;
    s32        arg;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }
    if (gGameSession->at4.loc.view == 0x27) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            lo            = ((u32)Gp_LcgState >> 16) & 0x1FF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            arg           = ((((u32)Gp_LcgState >> 16) % 3) << 16) + 0x80000100;
            Gp_SpawnEff(0x6003D, coord, lo + arg, &work->move);
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 3U) == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60093, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x100, &work->move);
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 7U) == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            work->move.vy = 0;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0xA0000400, &work->move);
        }
    } else {
        work->age++;
        if (work->age < 150) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (work->age < (u16)(((u32)Gp_LcgState >> 16) % 150U)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vy = ((u32)Gp_LcgState >> 16) & 0x7FF;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x80000400,
                                &work->move);
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (work->age < (u16)(((u32)Gp_LcgState >> 16) % 120U)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    work->move.vy = 0xC00;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x20010400,
                                &work->move);
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
/// `Task::spawnArg1` in `pos.vx`, rolls a frame period (1..4 ticks) into
/// `pos.vy` and a value into `pos.vz`, and, when the spawner left no drift,
/// rolls one whose ranges depend on `spawnArg1` (bit 30: +-0x80 on every axis;
/// negative: +-0x10 across and 0..-0xFF in y; otherwise +-0x80 across and
/// 0..15 in y) and turns it into `parent`'s frame. The drift is normalised and scaled to `scale`
/// (0x40 with bit 30 or bit 29, else 0x80), and `spawnArg1` is replaced by
/// two bits of its upper half. Later frames advance `index` once per period,
/// move the model by the drift, decrementing its y by one a tick, and hand
/// the task to `func_dryfield_night_motel_balcony_801819E0` until `index`
/// reaches 12, when it is released. Event states 2 and 3 suspend it, 4 and
/// above release it at once, and state 1 freezes the drift and the tick.
void func_dryfield_night_motel_balcony_8018158C(Task* task)
{
    GpEffWork* work  = task->spawnArg2;
    GpCoord*   coord = task->extra.tmd->coords;
    MATRIX*    m;
    s32        half; // default drift length and the centre of the wide drift rolls

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    half = 0x80;
    work->age++;

    switch (task->state) {
        case 0:
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = (u16)task->spawnArg1 & 0xFFF;
            work->scale          = half;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->pos.vy         = (((u32)Gp_LcgState >> 16) & 3) + 1;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->pos.vz         = ((u32)Gp_LcgState >> 16) & 0xFFF;
            work->index          = 0;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 & 0x40000000) {
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vy = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    work->move.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    work->scale   = 0x40;
                } else {
                    if (task->spawnArg1 < 0) {
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                    } else {
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = ((u32)Gp_LcgState >> 16) & 0xF;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    }
                    if (task->spawnArg1 & 0x20000000) {
                        work->scale = 0x40;
                    }
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&work->move);
            coord->flg      = 0;
            task->state     = 1;
            task->spawnArg1 = (s16)(task->spawnArg1 >> 16) & 3;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                if (work->age % work->pos.vy == 0) {
                    work->index++;
                }
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                work->move.vy--;
            } else {
                work->age--;
            }
            if (work->index < 12) {
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
/// on tpage 0x2C centred on it. `index` is the animation frame: it picks a
/// 48-texel cell of a five-column sheet starting at v 0x68, and steps the CLUT
/// x by 16 per frame from the origin `arg` selects in
/// `D_dryfield_night_motel_balcony_80182DF4`. The half-extent is
/// `pos.vx * 47 / (otz + 1)` on both axes.
void func_dryfield_night_motel_balcony_801819E0(Task* task, s32 arg)
{
    GpEffWork*     work;
    GpCoord*       coord;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    _ClutOrigin*   clut;
    SVECTOR*       vec;
    s16            xy;
    u16            vz;

    coord = task->extra.tmd->coords;
    work  = task->spawnArg2;

    head                                    = SCRATCH_HEAD(void);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)coord->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)coord->workm.t[1];
    vz                                      = (u16)coord->workm.t[2];
    SCRATCH_HEAD(void)                      = block;
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        clut        = &D_dryfield_night_motel_balcony_80182DF4[arg];
        prim->clut  = (clut->y << 6) | (((clut->x + work->index * 16) >> 4) & 0x3F);
        prim->u0    = work->index % 5 * 48;
        prim->v0    = work->index / 5 * 48 + 0x68;
        prim->u1    = work->index % 5 * 48 + 0x2F;
        prim->v1    = work->index / 5 * 48 + 0x68;
        prim->u2    = work->index % 5 * 48;
        prim->v2    = work->index / 5 * 48 + 0x97;
        prim->u3    = work->index % 5 * 48 + 0x2F;
        prim->v3    = work->index / 5 * 48 + 0x97;
        block->step = work->pos.vx * 0x2F / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame handler of a drifting room effect task. The first frame resets the
/// model's rotation to identity, rolls a starting animation step (0..9) and a
/// lifetime (5..14 ticks), and, when the spawner left no drift, rolls one and
/// turns it into `parent`'s frame. The drift is then normalised and scaled to a
/// length chosen by `Task::spawnArg1` (8 when negative, 0x80 with bit 30, 0x20
/// otherwise). Later frames move the model by the drift, bending it by one
/// unit a tick, and draw it through `func_dryfield_night_motel_balcony_8018221C`,
/// fading its colour over the last ten ticks before releasing the task. Event
/// states 2 and 3 suspend it; 4 and above release it at once, and any non-zero
/// state below that freezes the drift and the lifetime tick.
void func_dryfield_night_motel_balcony_80181E7C(Task* task)
{
    GpEffWork* work  = task->spawnArg2;
    GpCoord*   coord = task->extra.tmd->coords;
    MATRIX*    m;
    s32        seed;
    s16        tick;
    s16        end;
    u8         color[3];

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    work->age++;

    switch (task->state) {
        case 0:
            seed                 = Gp_LcgState * 5 + 0x71357911;
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = task->spawnArg1 & 0xFFF;
            Gp_LcgState          = seed;
            work->index          = ((u32)Gp_LcgState >> 16) % 10;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->angle          = ((u32)Gp_LcgState >> 16) % 10 + 5;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                if (task->spawnArg1 < 0) {
                    work->scale = 8;
                } else if (task->spawnArg1 & 0x40000000) {
                    work->scale = 0x80;
                } else {
                    work->scale = 0x20;
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&work->move);
            coord->flg  = 0;
            task->state = 1;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                work->index++;
                work->move.vy--;
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
            } else {
                work->age--;
            }
            tick = work->age;
            end  = work->angle;
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
/// `index % 10`; it picks the CLUT column and one 48-texel cell of a 5x2
/// grid starting at v=0x28. The quad is centred on the projected point with a
/// half-width of `pos.vx * 47 / otz` and extends three quarters above and one
/// quarter below. `color` is the RGB the texture is modulated by; NULL draws
/// the texture raw.
/// `tick` is unused. The block pointer is copied through an `asm` move because
/// the ROM keeps the scratch-block address in a temporary and copies it into the
/// pointer's own register, a copy no C spelling found here survives combine with.
void func_dryfield_night_motel_balcony_8018221C(Task* task, u8* color, s16 tick)
{
    GpEffWork*     work = task->spawnArg2;
    GpCoord*       coord;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    SVECTOR*       vec;
    s16            frame;
    s32            u0;
    s32            u1;
    s32            vTop;
    s32            vBottom;
    s16            xy;
    u16            vz;

    frame = work->index % 10;
    coord = task->extra.tmd->coords;

    head                                    = SCRATCH_HEAD(void);
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)coord->workm.t[0];
    vec                                     = (SVECTOR*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(vec));
    block->vec.vy      = (u16)coord->workm.t[1];
    vz                 = (u16)coord->workm.t[2];
    SCRATCH_HEAD(void) = block;
    block->vec.vz      = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
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
        u0            = frame % 5 * 48;
        vTop          = frame / 5 * 48;
        u1            = u0 + 0x2F;
        vBottom       = vTop + 0x57;
        vTop          = vTop + 0x28;
        prim->u0      = u0;
        prim->v0      = vTop;
        prim->u1      = u1;
        prim->v1      = vTop;
        prim->u2      = u0;
        prim->v2      = vBottom;
        prim->u3      = u1;
        prim->v3      = vBottom;
        block->step   = work->pos.vx * 47 / block->otz;
        xy            = (u16)block->sx - (u16)block->step;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = (u16)block->sx + (u16)block->step;
        prim->x3      = xy;
        prim->x1      = xy;
        block->step >>= 1;
        xy            = (u16)block->sy - block->step * 3;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = (u16)block->sy + (u16)block->step;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Spawns an 8-step burst of effect 0x6007E and then a 6-step burst of 0x60070
/// around part 3 of the model owned by the slot-4 task's child. Each step rolls
/// the room LCG four times (three for the second burst) and builds the offset
/// vector from the top byte of each draw; the first burst also carries the last
/// draw's low nine bits, biased by 0x300, in the spawn argument.
void func_dryfield_night_motel_balcony_8018257C(void)
{
    Task*    task;
    GpCoord* coord;
    SVECTOR  sv;
    s32      i;

    task  = gameGetPtrSlot(4);
    coord = task->firstChild->extra.tmd->coords + 3;

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
        Gp_SpawnEff(0x6007E, task->firstChild->extra.tmd->coords + 3,
                    (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x80000100,
                    &D_dryfield_night_motel_balcony_80182D20);
    }
}
