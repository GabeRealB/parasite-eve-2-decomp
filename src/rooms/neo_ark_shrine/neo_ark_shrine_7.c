#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_neo_ark_shrine_8018268C[];
extern SVECTOR D_neo_ark_shrine_80182694[];
extern SVECTOR D_neo_ark_shrine_8018269C[];
extern SVECTOR D_neo_ark_shrine_801826AC[];
extern SVECTOR D_neo_ark_shrine_801826C4[];
extern SVECTOR D_neo_ark_shrine_801826D4[];
extern SVECTOR D_neo_ark_shrine_80182704[];

void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2);

/// Tail every `NeoArkShrineFall` handler runs: clears the prop's root coordinate
/// flag, rebuilds its world matrix, and republishes the translation in
/// `func_800D7A9C`'s format, lowered by 0x320 so the prop draws on the floor.
void func_neo_ark_shrine_8017F86C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj        = task->extra;
    coord      = obj->coords;
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// On the task's first tick stores three ids (0x601DF, 0x601FB, 0x60217) into
/// the `D_80115758` / `D_8011572C` / `D_80115750` slots; then, every tick, runs
/// `func_neo_ark_shrine_8017FC14` over the positions the current camera view
/// shows, drawn from one of the room's `SVECTOR` arrays.
void func_neo_ark_shrine_8017F8DC(Task* task)
{
    if (task->state == 0) {
        D_80115758  = 0x601DF;
        D_8011572C  = 0x601FB;
        D_80115750  = 0x60217;
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_neo_ark_shrine_801826D4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[8], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[9], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[10], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 5:
        case 18: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_neo_ark_shrine_8018269C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_neo_ark_shrine_8018268C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 12: {
            SVECTOR* p = D_neo_ark_shrine_80182694;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 14: {
            SVECTOR* p = D_neo_ark_shrine_801826C4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 16: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 10:
        case 17: {
            SVECTOR* p = D_neo_ark_shrine_80182704;
            func_neo_ark_shrine_8017FC14(&p[0], 0, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 0, 0x300);
            break;
        }
    }
}

/// Projects the world-space point `pos` through `Gfx_ViewWorldMtx` and, when
/// the GTE flag is non-negative, queues one semi-transparent `POLY_FT4` sprite
/// centred on it (tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects
/// the 40-texel UV column `(s16)arg1 * 40` at v=0..0x27, and `arg2` is a signed
/// half-extent whose on-screen radius is `(s16)arg2 * 39 / otz`. All three RGB
/// channels take `0x20`, plus 0x10 on odd `animFrame` values, so the sprite
/// flickers frame to frame.
void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                idx;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                blend;
    s16                xy;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(pos);
    gte_rtps_real();
    ds    = &gDisplayState;
    blend = (((u8)ds->animFrame & 1) * 16) + 0x20;
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        sarg        = (s16)arg2;
        setRGB0(prim, blend, blend, blend);
        prim->u0                          = u0;
        prim->v0                          = 0;
        prim->u1                          = u1;
        prim->v1                          = 0;
        prim->u2                          = u0;
        prim->v2                          = 0x27;
        prim->u3                          = u1;
        prim->v3                          = 0x27;
        prim->code                       |= 2;
        ((RoomDraw13Scratch*)tmp)->radius = (sarg * 40 - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy                                = ((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2                          = xy;
        prim->x0                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3                          = xy;
        prim->x1                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1                          = xy;
        prim->y0                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3                          = xy;
        prim->y2                          = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x10;
}
