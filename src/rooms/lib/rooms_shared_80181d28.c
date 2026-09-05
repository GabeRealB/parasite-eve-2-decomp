#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80181d28.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// One frame of a room glow sprite: a camera-facing, semi-transparent
/// `POLY_FT4` centred on the task's own coordinate frame. The frame's
/// translation is projected through `GsWSMATRIX` into a 0x14-byte
/// `G_SCRATCH_HEAD` block, and the quad is drawn as a square of half-extent
/// `0x6180 / otz` around the resulting screen point, so it shrinks with
/// distance and is dropped entirely inside `otz` 0x11.
///
/// `Task::spawnArg1` picks one of three lamps: it selects the 0x27x0x27 texture
/// cell at `u = (arg + 1) * 0x28`, `v = 0x10` on tpage 0x2B, the clut
/// `0x4380 | ((arg + 2) & 0x3F)`, and the pair of grey levels the sprite
/// flickers between - a base of 0x20 / 0x60 / 0x20 plus 0x08 / 0x10 / 0x0C on
/// the frames where `Display_State.field_8` is odd.
///
/// The task is one-shot: the work block is released
/// as soon as the quad has been queued, so the room respawns it every frame.
void RoomsShared80181d28(Task* task)
{
    GsCOORDINATE2*              coord;
    RoomEffWork*                work;
    void**                      scratch;
    u8*                         head;
    RoomsShared80181d28Scratch* blk;
    // The `gte_stszotz` operand is a second register holding the same pointer;
    // reload only emits that copy for a hard-register local (see
    // DECOMPILATION_LEARNINGS.md, "A `move` between two registers holding the
    // same pointer is a pin").
    register RoomsShared80181d28Scratch* p asm("a0");
    POLY_FT4*                            prim;
    s32                                  grey;
    s32                                  clut;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (RoomsShared80181d28Scratch*)(head - 0x14);
    blk->pos.vx = coord->workm.t[0];
    blk->pos.vy = coord->workm.t[1];
    *scratch    = blk;
    p           = blk;
    blk->pos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->pos);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(&p->otz);
    if (blk->otz >= 0x11) {
        u8 base[3] = { 0x20, 0x60, 0x20 };
        u8 step[3] = { 0x08, 0x10, 0x0C };

        grey        = base[task->spawnArg1] + (Display_State.field_8 & 1) * step[task->spawnArg1];
        prim->code |= 2;
        prim->tpage = 0x2B;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;
        // Assigning through an `s32` keeps the load of `spawnArg1` in SImode;
        // storing the expression straight into the `u16` field lets the front
        // end shorten the whole chain and the load becomes an `lhu`.
        clut       = ((task->spawnArg1 + 2) & 0x3F) | 0x4380;
        prim->clut = clut;
        prim->u0   = (task->spawnArg1 + 1) * 0x28;
        prim->v0   = 0x10;
        prim->u1   = (task->spawnArg1 + 1) * 0x28 + 0x27;
        prim->v1   = 0x10;
        prim->u2   = (task->spawnArg1 + 1) * 0x28;
        prim->v2   = 0x37;
        prim->u3   = (task->spawnArg1 + 1) * 0x28 + 0x27;
        prim->v3   = 0x37;
        blk->half  = 0x6180 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->half;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->half;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->half;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->half;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    Gp_ReleaseState1CMem(work, task);
}
