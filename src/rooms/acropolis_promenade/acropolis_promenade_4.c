#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "rooms/acropolis_promenade.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps` / `rtpt` / `mvmva`. The `inline_c.h` macros of those names assemble
/// to different words, so spell the instructions out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern s32 Gp_LcgState;

extern ApmGlowCorner D_acropolis_promenade_80181AE4[];

extern SVECTOR D_acropolis_promenade_80181AFC[];
extern SVECTOR D_acropolis_promenade_80181B0C[];
extern SVECTOR D_acropolis_promenade_80181B14[];
extern u16     D_acropolis_promenade_80181B74;
extern u16     D_acropolis_promenade_80181B76;
extern u16     D_acropolis_promenade_80181B78[];

/// Per-frame effect spawner for the promenade. `D_acropolis_promenade_80181B74`
/// / `_80181B76` and the twelve-entry mask table `_80181B78` are per-view bit
/// masks: bit `view - 1` of an entry says whether that emitter is visible from
/// the camera `Gp_GetViewIndex` reports, and the parallel twelve-entry
/// `_80181B14` array holds each emitter's offset from the room's coordinate
/// frame. View 7 spawns nothing.
void func_acropolis_promenade_8017E03C(Task* task)
{
    GsCOORDINATE2* coord;
    RoomEffWork*   work;
    u8             view;
    s32            i;
    s32            mask;
    s16            prev;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    view  = Gp_GetViewIndex();
    if (Gp_State1C->field_4 >= 4) {
        return;
    }
    work->field_22++;
    if (view == 7) {
        return;
    }
    mask = 1 << (view - 1);
    if (D_acropolis_promenade_80181B74 & mask) {
        Gp_SpawnEff(0x8006004B, coord, (s16)work->field_22, &D_acropolis_promenade_80181AFC[0]);
        Gp_SpawnEff(0x8006004B, coord, (s16)work->field_22, &D_acropolis_promenade_80181AFC[1]);
        Gp_SpawnEff(0x60057, coord, (s16)work->field_22, &D_acropolis_promenade_80181B0C[0]);
        Room_Draw21(&D_acropolis_promenade_80181AFC[-1], 0x100, 0x5C40);
    }
    for (i = 0; i < 3; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 0, &D_acropolis_promenade_80181B14[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i]);
        }
        if (D_acropolis_promenade_80181B78[i + 2] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 2]);
        }
        if (D_acropolis_promenade_80181B78[i + 4] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i + 4]);
        }
        if (D_acropolis_promenade_80181B78[i + 6] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 6]);
        }
    }
    if (D_acropolis_promenade_80181B78[11] & mask) {
        Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[11]);
    }
    if (D_acropolis_promenade_80181B76 & mask) {
        prev = work->field_24;
        if (prev != view) {
            for (i = 0; i < 0x28; i++) {
                Gp_SpawnEff(0x60056, coord, view, NULL);
            }
        } else {
            Gp_SpawnEff(0x60056, coord, prev, NULL);
            Gp_SpawnEff(0x60056, coord, prev, NULL);
        }
    }
    work->field_24 = view;
}

/// One falling water drip on the promenade, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole drip out of `Gp_LcgState`: `field_10.vx` is the column
/// (0..0xEF), `field_10.vy` the row it starts on (0xB0..0xEF), `field_24` the
/// lifetime in frames, `field_26` the width and `field_28` the number of frames
/// each row of fall takes. `Display_State.field_1f` picks the buffer half, and
/// the OT slot is the row scaled into the 0x500-deep range so a drip sorts
/// against the room behind it. The task releases itself once the camera turns
/// away, the lifetime runs out, or the drip falls off the bottom of the screen.
void func_acropolis_promenade_8017E394(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          onScreen;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = ((u32)Gp_LcgState >> 16) % 240;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = (((u32)Gp_LcgState >> 16) & 0x3F) + 0xB0;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y        = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x        = work->field_10.vx;
        depth    = 0x500 - (y - 0xB0) * 10;
        onScreen = y < 0xEF;
        if (onScreen) {
            rect.x         = x;
            rect.y         = y + bufferY;
            rect.w         = work->field_26;
            rect.h         = 1;
            mv             = (DR_MOVE*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(mv + 1);
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && onScreen) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// One frame of the promenade's twinkling star: two semi-transparent
/// `POLY_FT4`s stacked on the same screen point, centred on the task's own
/// coordinate frame. The frame's translation is projected through `GsWSMATRIX`
/// into a 0x18-byte `G_SCRATCH_HEAD` block, and both quads are dropped
/// entirely inside `otz` 0x11.
///
/// The lower quad is upright, of half-extent `0x1680 / otz`, and animates
/// through six 0x10x0x10 cells at v = 0 on tpage 0x2B by stepping `u` with
/// `work->field_22 % 6`; it is drawn `code |= 3`, so semi-transparent *and*
/// unshaded. The upper quad is the 0x27x0x27 flare at v = 0x10 with clut
/// 0x4381, drawn at `0x3A80 / otz` from the centre along the spin angle
/// `work->field_24` and its quarter-turn (`+ 0x400`), so it rotates a frame at
/// a time. Its colour is a fresh random grey (0x20..0x7F, equal on all three
/// channels) every frame, which is what makes the star flicker.
///
/// Like the promenade's other glows, the task is one-shot: the work block is
/// released as soon as both quads have been queued, so the room respawns it
/// every frame it wants the star.
void func_acropolis_promenade_8017E634(Task* task)
{
    GsCOORDINATE2*     coord;
    RoomEffWork*       work;
    void**             scratch;
    u8*                head;
    ApmTwinkleScratch* blk;
    s32*               otzp;
    POLY_FT4*          prim;
    s32                grey;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    work->field_22 = task->spawnArg1;
    scratch        = (void**)G_SCRATCH_HEAD;
    head           = *scratch;
    blk            = (ApmTwinkleScratch*)(head - 0x18);
    otzp           = &blk->otz;
    blk->pos.vx    = coord->workm.t[0];
    blk->pos.vy    = coord->workm.t[1];
    *scratch       = blk;
    blk->pos.vz    = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->pos);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(otzp);
    if (blk->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4380;
        prim->code |= 3;
        prim->u0    = ((s16)work->field_22 % 6) * 16;
        prim->v0    = 0;
        prim->u1    = ((s16)work->field_22 % 6) * 16 + 0xF;
        prim->v1    = 0;
        prim->u2    = ((s16)work->field_22 % 6) * 16;
        prim->v2    = 0xF;
        prim->u3    = ((s16)work->field_22 % 6) * 16 + 0xF;
        prim->v3    = 0xF;
        blk->dx     = 0x1680 / blk->otz;
        blk->dy     = 0x1680 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->dx;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->dx;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->dy;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->dy;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);

        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->clut  = 0x4381;
        prim->tpage = 0x2B;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        grey        = ((u32)Gp_LcgState >> 16) % 96 + 0x20;
        prim->u0    = 0;
        prim->v0    = 0x10;
        prim->u1    = 0x27;
        prim->v1    = 0x10;
        prim->u2    = 0;
        prim->v2    = 0x37;
        prim->u3    = 0x27;
        prim->v3    = 0x37;
        prim->code |= 2;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;

        work->field_24 = Display_State.field_8 + work->field_22;
        blk->dx        = ((0x3A80 / blk->otz) * rsin((s16)work->field_24)) >> 12;
        blk->dy        = ((0x3A80 / blk->otz) * rcos((s16)work->field_24)) >> 12;
        prim->x0       = blk->sxy.vx + blk->dx;
        prim->x3       = blk->sxy.vx - blk->dx;
        prim->y0       = blk->sxy.vy - blk->dy;
        prim->y3       = blk->sxy.vy + blk->dy;
        blk->dx        = ((0x3A80 / blk->otz) * rsin((s16)work->field_24 + 0x400)) >> 12;
        blk->dy        = ((0x3A80 / blk->otz) * rcos((s16)work->field_24 + 0x400)) >> 12;
        prim->x1       = blk->sxy.vx + blk->dx;
        prim->x2       = blk->sxy.vx - blk->dx;
        prim->y1       = blk->sxy.vy - blk->dy;
        prim->y2       = blk->sxy.vy + blk->dy;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    Gp_ReleaseState1CMem(work, task);
}

/// Draws one frame of the promenade's ground glow: a semi-transparent textured
/// quad lying flat under the task's coordinate frame. The four corner signs in
/// `D_acropolis_promenade_80181AE4` are scaled to +/-0x300 in `vx` / `vz` (with
/// `vy` left at zero, so the quad is horizontal), rotated by the task's own
/// `workm`, offset by that matrix's translation and then projected through
/// `GsWSMATRIX` into an `ApmGlowScratch` block taken from `G_SCRATCH_HEAD`. The
/// first corner goes through `rtps` and the other three through `rtpt`, the
/// same split the sanctuary's mosaic tiles use.
///
/// The depth is biased by 0x20 before the near-plane test, so the glow survives
/// a little closer to the camera than the 0x11 cutoff alone would allow. Its
/// colour is a fresh random grey (0..0xF, equal on all three channels) every
/// frame, which is what makes it flicker; the quad is drawn semi-transparent
/// (`code |= 2`) from the 0x27x0x27 patch at v = 0x10 on tpage 0x2B.
///
/// The task is one-shot: the work block is released as soon as the quad has
/// been queued, so the room respawns it each frame it wants the glow.
void func_acropolis_promenade_8017ED44(Task* task)
{
    GsCOORDINATE2*  coord;
    RoomEffWork*    work;
    void**          scratch;
    u8*             head;
    ApmGlowScratch* blk;
    POLY_FT4*       prim;
    SVECTOR*        sv;
    s32             i;
    s32             grey;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch        = (void**)G_SCRATCH_HEAD;
    head           = *scratch;
    work->field_22 = task->spawnArg1;
    *scratch       = head - 0x24;
    blk            = (ApmGlowScratch*)(head - 0x24);
    for (i = 0; i < 4; i++) {
        blk->v[i].vx = D_acropolis_promenade_80181AE4[i].x * 0x300;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(ApmGlowScratch, v));
        sv->vy = 0;
        sv->vz = D_acropolis_promenade_80181AE4[i].y * 0x300;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0_real();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx += coord->workm.t[0];
        sv->vy       += coord->workm.t[1];
        sv->vz       += coord->workm.t[2];
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    prim->u0 = 0;
    prim->v0 = 0x10;
    prim->u1 = 0x27;
    prim->v1 = 0x10;
    prim->u2 = 0;
    prim->v2 = 0x37;
    prim->u3 = 0x27;
    prim->v3 = 0x37;
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    blk->otz += 0x20;
    if (blk->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4381;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        grey        = ((u32)Gp_LcgState >> 16) & 0xF;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;
        prim->code |= 2;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x24;
    Gp_ReleaseState1CMem(work, task);
}
