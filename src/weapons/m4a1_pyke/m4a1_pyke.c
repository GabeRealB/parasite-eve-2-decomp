#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/m4a1_pyke.h"

extern s32 Gp_LcgState;

/// Translation of the Pyke's effect coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x200, 0x40)`.
SVECTOR D_m4a1_pyke_8011E90C = { 0, 0x200, 0x40, 0 };

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix, no translation.
/// The `inline_c.h` macro of that name assembles to a different word, so spell
/// the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// `rtps`: project V0 through the current matrices. The `inline_c.h` macro of
/// that name assembles to a different word, so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// `rtpt`: project V0..V2 through the current matrices. The `inline_c.h` macro
/// of that name assembles to a different word, so spell the instruction out.
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// Teardown callback shared with the other weapon overlays; it unlinks
/// `Task::idMap` and releases the `Gp_State1C` work block.
void WeaponsShared8011e4ac(Task* task);

/// Per-frame beam task for the M4A1 Pyke. Nothing runs while the player model
/// is hidden (`field_C & 0x80`) or the room is fading out
/// (`Gp_State1C->field_4 >= 2`). State 0 hangs the task's own coordinate off
/// `field_8` at the fixed muzzle offset with an identity rotation; state 1 then
/// dispatches on `spawnArg1`:
///
/// - 1 draws the beam head at `workm.t` every frame and claims room-light slot
///   1 as a narrow (`0x80` / `0x400`) light aimed at a random angle in
///   `0x400..0xB00`, arming the flare width in `field_24`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x6017F`
///   as a child of this task, and re-claims the light with a much wider
///   (`0x400` / `0x4000`) falloff and a `0x800..0xF00` angle.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `Gp_State1C->field_4` is non-zero the two drawing sub-states wind
/// `field_22` back down instead of advancing.
void func_m4a1_pyke_8011D1F8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GsCOORDINATE2* light;
    GpMtxWords*    rot;
    GpEffWork*     eff;
    u32            ang;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    base  = &Gp_RoomCoords[1];
    light = &base->coord;
    slot  = (GpCoordTail*)light;
    if ((((GpActorWork*)Game_GetPtrSlot(3))->extra->field_C & 0x80) != 0) {
        return;
    }
    if (Gp_State1C->field_4 >= 2) {
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = work->field_8;
            rot->w0           = 0x1000;
            rot->w1           = 0;
            rot->w2           = 0x1000;
            rot->w3           = 0;
            rot->h4           = 0x1000;
            coord->coord.t[0] = D_m4a1_pyke_8011E90C.vx;
            coord->coord.t[1] = D_m4a1_pyke_8011E90C.vy;
            coord->coord.t[2] = D_m4a1_pyke_8011E90C.vz;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            task->state = 1;
            break;
        case 1:
            switch (task->spawnArg1) {
                case 0:
                    break;
                case 1:
                    if (Gp_State1C->field_4 != 0) {
                        work->field_22--;
                        func_m4a1_pyke_8011D548(
                            (VECTOR3*)&coord->workm.t, work->field_22, 0x80);
                        break;
                    }
                    Gp_UpdateCoord(coord);
                    func_m4a1_pyke_8011D548((VECTOR3*)&coord->workm.t, work->field_22, 0x80);
                    base->field_0  = 4;
                    slot->field_58 = 0x80;
                    slot->field_5C = 0x400;
                    ang            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState    = ang;
                    /* `field_52` reads the halfword back as unsigned and
                       `field_54` as signed, so the two shifts come off the same
                       register: a plain `ang >> 1` / `ang >> 2` pair would drop
                       the sign-extension the ROM keeps for `field_54`. */
                    slot->field_50 = ((ang >> 16) & 0x700) + 0x400;
                    slot->field_52 = (u16)slot->field_50 >> 1;
                    slot->field_54 = slot->field_50 >> 2;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                    light->flg     = 0;
                    work->field_24 = 0x40;
                    break;
                case 2:
                    if (Gp_State1C->field_4 != 0) {
                        work->field_22--;
                        break;
                    }
                    Gp_UpdateCoord(coord);
                    if (work->field_24 < 0x180) {
                        work->field_24 = (u16)work->field_24 + 0x40;
                    }
                    eff = Gp_SpawnEff(0x6017F, coord, work->field_24, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->field_0);
                    }
                    base->field_0  = 4;
                    slot->field_58 = 0x400;
                    slot->field_5C = 0x4000;
                    ang            = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState    = ang;
                    slot->field_50 = ((ang >> 16) & 0x700) + 0x800;
                    slot->field_52 = (u16)slot->field_50 >> 1;
                    slot->field_54 = slot->field_50 >> 2;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &light->coord);
                    light->flg = 0;
                    break;
                case 3:
                    task->spawnArg1 = 1;
                    break;
                case 4:
                    task->spawnArg1 = 0;
                    break;
                case 5:
                    Gp_ReleaseState1CMem(work, task);
                    break;
            }
            break;
    }
}

/// Draws one frame of the Pyke's beam head at the world point `pos`. The point
/// is projected through `GsWSMATRIX` by a single `RTPS` into a 0x18-byte
/// scratchpad block; the sprite is dropped whole if that `RTPS` sets its
/// `FLAG`. `frame` walks the six 0x20-wide sprite cells of the strip at
/// `(v = 0x98..0xB7)`, and `brightness` scales the on-screen half-extent, which
/// shrinks with distance as `brightness * 31 / otz`.
void func_m4a1_pyke_8011D548(VECTOR3* pos, u16 frame, s32 brightness)
{
    void**               scratch;
    u8*                  head;
    M4a1PykeBeamScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    s16                  x;
    s16                  y;
    u16                  uv;
    s32                  u0;
    s32                  u1;
    u16                  vz;

    scratch                                       = (void**)G_SCRATCH_HEAD;
    head                                          = *scratch;
    ((M4a1PykeBeamScratch*)(head - 0x18))->vec.vx = *(u16*)&pos->vx;
    block                                         = (M4a1PykeBeamScratch*)(head - 0x18);
    block->vec.vy                                 = *(u16*)&pos->vy;
    vz                                            = *(u16*)&pos->vz;
    *scratch                                      = block;
    block->vec.vz                                 = vz;
    vec                                           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&((M4a1PykeBeamScratch*)(head - 0x18))->sx);
    gte_stflg(&((M4a1PykeBeamScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((M4a1PykeBeamScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x430D;
        prim->v0    = 0x98;
        prim->v1    = 0x98;
        prim->v2    = 0xB7;
        prim->v3    = 0xB7;
        /* The remainder has to land in a `u16` of its own: writing it back to
           `frame` lets GCC fold the truncation into the shift, and taking the
           `u0` / `u1` pair straight off `frame` costs the `$a0` / `$a1`
           allocation the ROM has. */
        uv          = frame % 6;
        u0          = uv << 5;
        u1          = u0 + 0x1F;
        prim->u0    = u0;
        prim->u1    = u1;
        prim->u2    = u0;
        prim->u3    = u1;
        block->size = ((u16)brightness * 31) / block->otz;
        x           = *(u16*)&block->sx - *(u16*)&block->size;
        prim->x2    = x;
        prim->x0    = x;
        x           = *(u16*)&block->sx + *(u16*)&block->size;
        prim->x3    = x;
        prim->x1    = x;
        y           = *(u16*)&block->sy - *(u16*)&block->size;
        prim->y1    = y;
        prim->y0    = y;
        y           = *(u16*)&block->sy + *(u16*)&block->size;
        prim->y3    = y;
        prim->y2    = y;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Per-frame task for one dart the Pyke throws. `Task::spawnArg2` is the
/// `Gp_State1C` work block holding the dart's velocity (`field_10` / `field_12`
/// / `field_14`), its age (`field_22`), its flare width (`field_24`) and its
/// spin angle (`field_26`); `Task::extra` reaches the coordinate the dart flies
/// on. Everything stops once the room is fading out (`Gp_State1C->field_4 >=
/// 4`); while the fade is merely under way the dart is only redrawn.
///
/// - State 0 allocates the `M4a1PykeBeam` list node, aims the dart by rotating
///   `(0, spawnArg1 - rand(0..0x3F), 0)` through the coordinate's own matrix,
///   seeds the flare width and spin angle, links the node and falls through.
/// - State 1 flies the dart, redraws it, and every third frame traces the
///   ground under it for a splash. Hitting a wall (`Gp_CountRec18Hi`) or living
///   past 0x14 frames releases the block; hitting geometry (`func_800DE7CC`)
///   switches to state 2 with a fresh ricochet velocity.
/// - State 2 coasts on that velocity with a fast-widening flare until the dart
///   is 0x15 frames old.
void func_m4a1_pyke_8011D7D4(Task* task)
{
    GsCOORDINATE2  ground;
    SVECTOR        after;
    SVECTOR        before;
    GsCOORDINATE2* coord;
    GpEffWork*     work;
    M4a1PykeBeam*  beam;
    s32            fade;
    u32            ang0;
    u32            ang1;
    u32            ang2;
    u32            ang3;

    beam  = (M4a1PykeBeam*)task->idMap;
    work  = task->spawnArg2;
    fade  = Gp_State1C->field_4;
    coord = ((TmdObject*)task->extra)->field_8;
    if (fade >= 4) {
        if (task->state != 0) {
            Gp_UnlinkObj(&beam->obj);
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    if (fade != 0) {
        func_m4a1_pyke_8011DCEC((VECTOR3*)coord->workm.t,
                                ((s16)(u16)work->field_22 >> 1) + 1, work->field_24, work->field_26);
        return;
    }
    work->field_22 = (u16)work->field_22 + 1;
    switch (task->state) {
        case 0:
            beam = Mem_Calloc(sizeof(M4a1PykeBeam), 0);
            if (beam == NULL) {
                work->field_22 = 0;
                return;
            }
            task->exitCallback = WeaponsShared8011e4ac;
            /* The three halfwords are the SVECTOR `gte_rtv0_real` rotates in
               place, so `field_14` has to be cleared after the random pitch is
               written to `field_12`, not alongside `field_10`. */
            work->field_10 = 0;
            ang0           = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState    = ang0;
            work->field_12 = (u16)task->spawnArg1 - ((ang0 >> 16) & 0x3F);
            work->field_14 = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->field_10);
            gte_rtv0_real();
            gte_stsv(&work->field_10);
            work->field_24     = (u16)task->spawnArg1 + 0x180;
            ang1               = Gp_LcgState * 5 + 0x71357911;
            work->field_26     = (ang1 >> 16) & 0xFFF;
            task->state        = 1;
            task->idMap        = (TaskIdMap*)beam;
            beam->obj.field_8  = coord;
            beam->obj.field_C  = beam->rec;
            beam->obj.field_18 = 0x21C1E;
            beam->obj.field_1C = (s16)(u16)work->field_24 >> 1;
            Gp_LcgState        = ang1;
            beam->obj.flags    = 1;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].field_0 = 2;
            beam->obj.flags     |= 0x8000;
            /* fallthrough */
        case 1:
            work->field_24     = (u16)work->field_24 + 0x10;
            work->field_12     = (u16)work->field_12 + 8;
            before.vx          = coord->workm.t[0];
            before.vy          = coord->workm.t[1];
            before.vz          = coord->workm.t[2];
            coord->coord.t[0] += work->field_10;
            coord->coord.t[1] += work->field_12;
            coord->coord.t[2] += work->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            func_m4a1_pyke_8011DCEC((VECTOR3*)coord->workm.t,
                                    ((s16)(u16)work->field_22 >> 1) + 1, work->field_24,
                                    work->field_26);
            ang2        = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = ang2;
            if ((u16)((ang2 >> 16) % 3) == 0 && Gp_State1C->field_6 != 0 &&
                Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_m4a1_pyke_8011E168((VECTOR3*)ground.workm.t,
                                        (s16)((work->field_24 * 2) / 3));
            }
            if (Gp_CountRec18Hi(beam->obj.field_C, 0x30000) != 0) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                Gp_UnlinkObj(&beam->obj);
                task->state    = 2;
                work->field_10 = (u32)rcos(work->field_26) >> 8;
                work->field_12 = (u32)rsin(work->field_26) >> 8;
                ang3           = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState    = ang3;
                work->field_14 = (u32)rsin((ang3 >> 16) & 0xFFF) >> 8;
                return;
            }
            if (work->field_22 >= 0x15) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            Gp_ClearRec18Occupied(beam->rec);
            return;
        case 2:
            work->field_24     = (u16)work->field_24 + 0x40;
            coord->coord.t[0] += work->field_10;
            coord->coord.t[1] += work->field_12;
            coord->coord.t[2] += work->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_m4a1_pyke_8011DCEC((VECTOR3*)coord->workm.t,
                                    ((s16)(u16)work->field_22 >> 1) + 1, work->field_24,
                                    work->field_26);
            if (work->field_22 >= 0x15) {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one frame of the flying dart: a single semi-transparent, textured
/// `POLY_FT4` billboarded on the world point `pos`. `frame` walks the twelve
/// sprite frames of `D_80111E48`, `width` is the dart's flare width (divided
/// down by the projected depth) and `ang` its spin, so the quad is a square
/// rotated by `ang` rather than an axis-aligned sprite. `otz` is biased by one
/// before it is used as the divisor so a point on the near plane cannot divide
/// by zero.
void func_m4a1_pyke_8011DCEC(VECTOR3* pos, u16 frame, u16 width, s16 ang)
{
    void**               scratch;
    u8*                  head;
    M4a1PykeQuadScratch* block;
    M4a1PykeQuadScratch* vecp;
    POLY_FT4*            prim;
    GpEffUv8*            rec;
    u16                  idx;
    s32                  a;
    u16                  vz;

    scratch                                       = (void**)G_SCRATCH_HEAD;
    head                                          = *scratch;
    ((M4a1PykeQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&pos->vx;
    block                                         = (M4a1PykeQuadScratch*)(head - 0x1C);
    block->vec.vy                                 = *(u16*)&pos->vy;
    vz                                            = *(u16*)&pos->vz;
    *scratch                                      = block;
    block->vec.vz                                 = vz;
    vecp                                          = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    idx = frame % 12;
    gte_stsxy(&((M4a1PykeQuadScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((M4a1PykeQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((M4a1PykeQuadScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        rec         = &D_80111E48[idx];
        prim->clut  = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
        prim->u0    = rec->u;
        prim->v0    = rec->v;
        prim->u1    = rec->u + 0x27;
        prim->v1    = rec->v;
        prim->u2    = rec->u;
        prim->v2    = rec->v + 0x27;
        prim->u3    = rec->u + 0x27;
        prim->v3    = rec->v + 0x27;
        a           = ang;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        a           = a + 0x400;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((width * 0x27) / block->otz) * rsin(a)) >> 12;
        block->dy   = (((width * 0x27) / block->otz) * rcos(a)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Draws the dart's ground splash: the unit quad `D_80111E38` scaled to
/// `width` half-size, laid flat by `Gfx_ViewWorldMtx` and moved to the traced
/// ground point `pos`, then projected through `GsWSMATRIX` into a 0x30-byte
/// `G_SCRATCH_HEAD` block. The first corner goes through `rtps` and the other
/// three through one `rtpt`; a negative `gte_stflg` drops the quad.
void func_m4a1_pyke_8011E168(VECTOR3* pos, s32 width)
{
    void**                 scratch;
    u8*                    head;
    M4a1PykeSplashScratch* block;
    POLY_FT4*              prim;
    GpQuadCorner*          tbl;
    s32                    i;
    s32                    flag;
    s32                    otz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = (u8*)*scratch - 0x30;
    /* The ROM stores the freshly computed head and keeps a *copy* of it in the
       register the rest of the function walks; without the barrier GCC folds
       the two together and stores the copy instead. */
    SOFT_TOUCH_REG(head);
    *scratch = head;
    block    = (M4a1PykeSplashScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    tbl = D_80111E38;
    do {
        block->vec[i].vx = tbl[i].x * width;
        block->vec[i].vy = 0;
        block->vec[i].vz = tbl[i].y * width;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&block->vec[i]);
        gte_rtv0_real();
        gte_stsv(&block->vec[i]);
        *(u16*)&block->vec[i].vx = *(u16*)&block->vec[i].vx + *(u16*)&pos->vx;
        *(u16*)&block->vec[i].vy = *(u16*)&block->vec[i].vy + *(u16*)&pos->vy;
        *(u16*)&block->vec[i].vz = *(u16*)&block->vec[i].vz + *(u16*)&pos->vz;
        i++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy[0]);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy[1], &block->sxy[2], &block->sxy[3]);
    gte_stflg(&flag);
    if (flag >= 0) {
        /* Dead in the ROM too: `otz` is bumped before it is read back, so the
           increment lands on garbage and `gte_stszotz` immediately overwrites
           it. It still costs a `lw`/`addiu`/`sw` because the address escapes
           into the asm below. */
        otz++;
        gte_stszotz(&otz);
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x40, 0x40, 0x40);
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        setUV4(prim, 0xE0, 0xC8, 0xFF, 0xC8, 0xE0, 0xE7, 0xFF, 0xE7);
        prim->x0 = block->sxy[0].vx;
        prim->y0 = block->sxy[0].vy;
        prim->x1 = block->sxy[1].vx;
        prim->y1 = block->sxy[1].vy;
        prim->x2 = block->sxy[2].vx;
        prim->y2 = block->sxy[2].vy;
        prim->x3 = block->sxy[3].vx;
        prim->y3 = block->sxy[3].vy;
        addPrim((u_long*)(((((u32)otz << Display_State.field_128) >> 2) & 0xFFC) +
                          (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
}
