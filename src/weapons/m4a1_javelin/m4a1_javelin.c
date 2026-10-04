#include "weapons/m4a1_javelin.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "m4a1_javelin_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
/// Read-only XYZ translation words from the launch sprite's `GfxCoord::workm.t`.
#define SPRITE_QUAD_POSITION_SOURCE_TYPE const long
#define SPRITE_QUAD_POS(p, i)            ((p)[i])
/// Signed texture-frame index for the eight-frame launch sprite.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// Scratch-stack block for projecting the two ends of one flare line.
///
/// The ends are world points, transformed one after the other through
/// `GsWSMATRIX` with one perspective transform each. Every transform writes
/// that end's screen position as one screen-XY word and replaces `flag` with
/// the GTE flag word; a negative flag word means that transform reported an
/// error, and the line is dropped. Otherwise that end's depth, SZ3 / 4, is
/// stored and raised by one. The line is sorted and blended at the first
/// end's depth alone; the second end's depth is stored but not read back.
///
/// Reserve one complete block and release it in scratch-stack order after
/// drawing; no pointer into it survives release.
typedef struct {
    s32     otz0; // Ordering-table depth of the first end, plus one; where the line is sorted and blended
    s32     otz1; // Ordering-table depth of the second end, plus one; never read back
    s32     flag; // GTE flag word of the latest transform; negative means that transform failed
    DVECTOR sxy0; // Projected screen position of the first end, in pixels; one GTE screen-XY word
    DVECTOR sxy1; // Projected screen position of the second end, in pixels; one GTE screen-XY word
} _M4a1JavelinLineScratch;
STATIC_ASSERT_SIZEOF(_M4a1JavelinLineScratch, 0x14);

static void func_m4a1_javelin_8011DAB0(SVECTOR* p0, SVECTOR* p1, u16 flags, u16 color);
static void func_m4a1_javelin_8011E4A8(SVECTOR* p0, SVECTOR* p1, u16 flags, u16 color);
static void func_m4a1_javelin_8011EE78(SVECTOR* p0, SVECTOR* p1, u16 brightness);

/// Fixed local offset the guide beam's coordinate hangs at.
static SVECTOR D_m4a1_javelin_8011FA90 = { 0, 0x200, 0x20, 0 };

/// `(0, 0x800, 0)`: the probe offset `func_800DE7CC` traces each beam segment
/// against, rotated into world space by `gGfxViewCoord.workm` first.
static SVECTOR D_m4a1_javelin_8011FA98 = { 0, 0x800, 0, 0 };

/// Per-segment `flags` for `func_m4a1_javelin_8011DAB0`, walked from the far
/// end (`[5]`, bit 1: retake the beam angle) to the muzzle (`[0]`, bit 0: cap
/// the near end).
static u16 D_m4a1_javelin_8011FAA0[6] = { 1, 0, 0, 0, 0, 2 };

/// The four RGB444 beam colours `EffectWork::step` fades through.
static u16 D_m4a1_javelin_8011FAAC[4] = { 0x12, 0x124, 0x248, 0x36C };

static void func_m4a1_javelin_8011F4A4(const long* arg0);
void        func_m4a1_javelin_8011F5D4(Task* arg0);

/// Per-frame task for the javelin's guide beam. `Task::spawnArg2` is the
/// `EffectWork` and `Task::extra` reaches the coordinate the beam
/// hangs on. Cancellation (`gRoomEffectState->effectControl` >=
/// `ROOM_EFFECT_CONTROL_CANCEL_MIN`) tears the effect down; pause or hide
/// freezes it.
///
/// - State 0 hangs the coordinate off `EffectWork::parent` at the fixed offset
///   `D_m4a1_javelin_8011FA90` with an identity rotation, seeds the beam
///   parameters and falls through to state 1.
/// - State 1 is the muzzle flare: it refreshes transient light slot 1 with
///   (`0x100` / `0x1000`) falloff and red intensity that halves every frame, then draws
///   eight `func_m4a1_javelin_8011EE78` tracers around a ring that widens by
///   `0x20` a frame until `scale` reaches `0xC0`, which moves it to state 2.
/// - State 2 is the beam itself. The far end is either the cached
///   `D_m4a1_javelin_8012EB68` impact point or `EffectWork::move` rotated
///   into world space, and `pos` is a sixth of the way back towards the
///   muzzle. Six segments are drawn with
///   `func_m4a1_javelin_8011DAB0`; while `gRoomEffectState->groundTraceEnabled` is set each
///   segment also probes `D_m4a1_javelin_8011FA98` (0x800 along +Y) with
///   `func_800DE7CC` and skins the ground contact with
///   `func_m4a1_javelin_8011E4A8` as long as the probe keeps hitting. The beam
///   fades one `D_m4a1_javelin_8011FAAC` colour step every 0x20 of `age`
///   and releases the work block when the last step runs out.
void func_m4a1_javelin_8011D1E4(Task* task)
{
    EffectWork*                    work;
    GfxCoord*                      coord;
    GameActor*                     actor;
    WorldCoordTransientPointLight* lightSlot;
    GfxCoord*                      light;
    WorldCoordPointLight*          slot;
    GfxRotationWords*              dstm;
    SVECTOR                        pa;
    SVECTOR                        pb;
    SVECTOR                        qa;
    SVECTOR                        qb;
    s32                            i;
    s32                            lim;
    s32                            t;
    u16                            rnd;

    actor     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    lightSlot = &gWorldCoordTransientPointLights[1];
    slot      = &lightSlot->light;
    light     = &lightSlot->light.head.transform.coord;
    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    work->age = work->age + 1;
    switch (task->state) {
        case 0:
            dstm                = (GfxRotationWords*)&coord->coord;
            coord->parent       = work->parent;
            dstm->m00M01        = ONE;
            dstm->m02M10        = 0;
            dstm->m11M12        = ONE;
            dstm->m20M21        = 0;
            dstm->m22           = ONE;
            coord->coord.t[0]   = D_m4a1_javelin_8011FA90.vx;
            coord->coord.t[1]   = D_m4a1_javelin_8011FA90.vy;
            coord->coord.t[2]   = D_m4a1_javelin_8011FA90.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            task->state                = 1;
            work->move.vy              = 0x1F40;
            work->move.vx              = 0;
            work->move.vz              = 0;
            D_m4a1_javelin_8012EB68.vx = 0;
            D_m4a1_javelin_8012EB68.vy = 0;
            D_m4a1_javelin_8012EB68.vz = 0;
            D_m4a1_javelin_8012EB70    = 0;
            work->period               = 0x600;
            work->step                 = 3;
            D_m4a1_javelin_8012EB62    = 0;
            D_m4a1_javelin_8012EB60    = 0;
            /* fallthrough */
        case 1:
            Gp_UpdateCoord(coord);
            lightSlot->framesLeft = 4;
            slot->inner           = 0x100;
            slot->outer           = 0x1000;
            t                     = slot->head.color.r >> 1;
            slot->head.color.r    = t;
            slot->head.color.g    = t >> 2;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            slot->head.color.b    = ((gRandomLcgState >> 16) & 0x700) + 0x400;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
            light->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->scale == 0xC0) {
                task->state = 2;
            } else {
                work->scale  = work->scale + 0x20;
                work->angle  = work->angle + 0xC0;
                work->period = work->period - 0xF0;
            }
            pa.vx = coord->workm.t[0];
            pa.vy = coord->workm.t[1];
            pa.vz = coord->workm.t[2];
            for (i = 0; i < 0x1000; i += 0x200) {
                pb.vx = (work->period * rsin(i)) >> 12;
                pb.vy = work->angle;
                pb.vz = (work->period * rcos(i)) >> 12;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&pb);
                gte_rtv0();
                gte_stsv(&pb);
                pb.vx = (u16)pb.vx + (u16)pa.vx;
                pb.vy = (u16)pb.vy + (u16)pa.vy;
                pb.vz = (u16)pb.vz + (u16)pa.vz;
                func_m4a1_javelin_8011EE78(&pa, &pb, work->scale);
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            lightSlot->framesLeft = 4;
            slot->inner           = 0x400;
            slot->outer           = 0x4000;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd                   = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            slot->head.color.b    = rnd;
            slot->head.color.r    = rnd >> 1;
            slot->head.color.g    = rnd >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
            D_m4a1_javelin_8012EB64 = 0;
            light->composeStamp     = GRAPHICS_COORD_DIRTY;
            D_m4a1_javelin_8012EB66 = 0;
            if (D_m4a1_javelin_8012EB70 != 0) {
                pa.vx = coord->workm.t[0];
                pa.vy = coord->workm.t[1];
                pa.vz = coord->workm.t[2];
                pb.vx = D_m4a1_javelin_8012EB68.vx;
                pb.vy = D_m4a1_javelin_8012EB68.vy;
                pb.vz = D_m4a1_javelin_8012EB68.vz;
            } else {
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&pb);
                pa.vx = coord->workm.t[0];
                pa.vy = coord->workm.t[1];
                pa.vz = coord->workm.t[2];
                pb.vx = (u16)pb.vx + (u16)pa.vx;
                pb.vy = (u16)pb.vy + (u16)pa.vy;
                pb.vz = (u16)pb.vz + (u16)pa.vz;
            }
            work->pos.vx = (pa.vx - pb.vx) / 6;
            work->pos.vy = (pa.vy - pb.vy) / 6;
            work->pos.vz = (pa.vz - pb.vz) / 6;
            pa.vx        = (u16)pb.vx;
            pa.vy        = (u16)pb.vy;
            pa.vz        = (u16)pb.vz;
            if (gRoomEffectState->groundTraceEnabled != 0) {
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&D_m4a1_javelin_8011FA98);
                gte_rtv0();
                gte_stsv(&qb);
                qb.vx = (u16)qb.vx + (u16)pb.vx;
                qb.vy = (u16)qb.vy + (u16)pb.vy;
                qb.vz = (u16)qb.vz + (u16)pb.vz;
                pb.vy = (u16)pb.vy - 0x100;
                if (func_800DE7CC(&qb, &pb, &qb, NULL) == 1) {
                    lim = 6;
                } else {
                    lim = 5;
                }
                pb.vy = (u16)pb.vy + 0x100;
                for (i = 5; i >= 0; i--) {
                    pa.vx = (u16)pa.vx + work->pos.vx;
                    pa.vy = (u16)pa.vy + work->pos.vy;
                    pa.vz = (u16)pa.vz + work->pos.vz;
                    func_m4a1_javelin_8011DAB0(&pa, &pb, D_m4a1_javelin_8011FAA0[i],
                                               D_m4a1_javelin_8011FAAC[work->step]);
                    gte_SetRotMatrix(&gGfxViewCoord.workm);
                    gte_ldv0(&D_m4a1_javelin_8011FA98);
                    gte_rtv0();
                    gte_stsv(&qa);
                    qa.vx = (u16)qa.vx + (u16)pa.vx;
                    qa.vy = (u16)qa.vy + (u16)pa.vy;
                    qa.vz = (u16)qa.vz + (u16)pa.vz;
                    pa.vy = (u16)pa.vy - 0x100;
                    if (func_800DE7CC(&qa, &pa, &qa, NULL) == 1) {
                        if (i < lim) {
                            func_m4a1_javelin_8011E4A8(&qa, &qb, D_m4a1_javelin_8011FAA0[i],
                                                       D_m4a1_javelin_8011FAAC[work->step >> 1]);
                        }
                        lim = i;
                    } else {
                        lim = i - 1;
                    }
                    pa.vy = (u16)pa.vy + 0x100;
                    qb.vx = (u16)qa.vx;
                    qb.vy = (u16)qa.vy;
                    qb.vz = (u16)qa.vz;
                    pb.vx = (u16)pa.vx;
                    pb.vy = (u16)pa.vy;
                    pb.vz = (u16)pa.vz;
                }
            } else {
                for (i = 5; i >= 0; i--) {
                    pa.vx = (u16)pa.vx + work->pos.vx;
                    pa.vy = (u16)pa.vy + work->pos.vy;
                    pa.vz = (u16)pa.vz + work->pos.vz;
                    func_m4a1_javelin_8011DAB0(&pa, &pb, D_m4a1_javelin_8011FAA0[i], 0x36C);
                    pb.vx = (u16)pa.vx;
                    pb.vy = (u16)pa.vy;
                    pb.vz = (u16)pa.vz;
                }
            }
            if (work->age >= 0x21) {
                work->step = work->step - 1;
                if (work->step < 0) {
                    effectKillTask(work, task);
                }
            } else if (*(s32*)&actor->mode != 0x40000) {
                work->age = work->age + 0x20;
            }
            break;
    }
}

/// Draws the javelin's aiming guide: a flat `LINE_F2` from `p0` to `p1` plus
/// three fans of `POLY_G4` segments, all dropped if either endpoint fails its
/// `RTPS` `FLAG` check (which also arms `D_m4a1_javelin_8012EB64` so the beam
/// angle is recomputed on the next visible frame). `color` is an RGB444 word
/// widened a nibble at a time, brightened by `gDisplayState.animFrame`'s low bit
/// so the beam flickers every other frame; the fans use two thirds of that.
/// Bit 1 of `flags` forces the `ratan2` of the on-screen beam direction to be
/// taken again, otherwise the cached `D_m4a1_javelin_8012EB60` is reused. The
/// first fan caps the far end, the second (bit 0 of `flags`) caps the near end
/// and the third sweeps at double rate to skin the beam between the two.
static void func_m4a1_javelin_8011DAB0(SVECTOR* p0, SVECTOR* p1, u16 flags, u16 color)
{
    u8*                      head;
    OverlayPointPairScratch* sc;
    LINE_F2*                 line;
    POLY_G4*                 prim;
    s32                      i;
    s32                      tipAng;
    s32                      baseAng;
    s32                      bodyAng;
    s32                      tint;
    u32                      rgb;
    u8                       r;
    u8                       g;
    u8                       b;
    u16                      angle;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(OverlayPointPairScratch);
    sc                       = (OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch));

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(p0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch)))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch)))->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&((OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch)))->otz0);
        sc->otz0++;
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch)))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch)))->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch)))->otz1);
            rgb = color;
            r   = (rgb >> 4) & 0xF0;
            g   = rgb & 0xF0;
            b   = (color & 0xF) * 0x10;
            sc->otz1++;
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineF2(line);
            tint = ((u8)gDisplayState.animFrame & 1) * 0x10;
            r    = r + tint;
            g    = g + tint;
            b    = b + tint;
            setRGB0(line, r, g, b);
            line->x0 = sc->sx0;
            line->y0 = sc->sy0;
            line->x1 = sc->sx1;
            line->y1 = sc->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, sc->otz0);
            sc->radius0 = 0x4000 / sc->otz0;
            sc->radius1 = 0x4000 / sc->otz1;
            r           = r * 2 / 3;
            g           = g * 2 / 3;
            b           = b * 2 / 3;
            if ((flags & 2) || D_m4a1_javelin_8012EB64 != 0) {
                angle                   = ratan2(line->y1 - line->y0, line->x0 - line->x1);
                D_m4a1_javelin_8012EB60 = angle;
                D_m4a1_javelin_8012EB64 = 0;
                for (i = (s16)angle; i < (s16)angle + 0x800; i += 0x400) {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    tipAng   = i + 0x800;
                    prim->x0 = (u16)line->x1 + ((sc->radius1 * rsin(tipAng)) >> 12);
                    prim->y0 = (u16)line->y1 + ((sc->radius1 * rcos(tipAng)) >> 12);
                    tipAng   = i + 0xA00;
                    prim->x1 = (u16)line->x1 + ((sc->radius1 * rsin(tipAng)) >> 12);
                    prim->y1 = (u16)line->y1 + ((sc->radius1 * rcos(tipAng)) >> 12);
                    prim->x2 = (u16)line->x1;
                    prim->y2 = (u16)line->y1;
                    tipAng   = i + 0xC00;
                    prim->x3 = (u16)line->x1 + ((sc->radius1 * rsin(tipAng)) >> 12);
                    prim->y3 = (u16)line->y1 + ((sc->radius1 * rcos(tipAng)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, sc->otz1);
                }
            } else {
                angle = D_m4a1_javelin_8012EB60;
            }
            if (flags & 1) {
                for (i = (s16)angle; i < (s16)angle + 0x800; i += 0x400) {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = (u16)line->x0 + ((sc->radius0 * rsin(i)) >> 12);
                    prim->y0 = (u16)line->y0 + ((sc->radius0 * rcos(i)) >> 12);
                    baseAng  = i + 0x200;
                    prim->x1 = (u16)line->x0 + ((sc->radius0 * rsin(baseAng)) >> 12);
                    prim->y1 = (u16)line->y0 + ((sc->radius0 * rcos(baseAng)) >> 12);
                    prim->x2 = (u16)line->x0;
                    prim->y2 = (u16)line->y0;
                    baseAng  = i + 0x400;
                    prim->x3 = (u16)line->x0 + ((sc->radius0 * rsin(baseAng)) >> 12);
                    prim->y3 = (u16)line->y0 + ((sc->radius0 * rcos(baseAng)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, sc->otz0);
                }
            }
            for (i = (s16)angle; i < (s16)angle + 0x800; i += 0x400) {
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                bodyAng        = (s16)angle + ((i - (s16)angle) * 2);
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, r, g, b);
                setRGB3(prim, r, g, b);
                prim->x0 = (u16)line->x0 + ((sc->radius0 * rsin(bodyAng)) >> 12);
                prim->y0 = (u16)line->y0 + ((sc->radius0 * rcos(bodyAng)) >> 12);
                prim->x1 = (u16)line->x1 + ((sc->radius1 * rsin(bodyAng)) >> 12);
                prim->y1 = (u16)line->y1 + ((sc->radius1 * rcos(bodyAng)) >> 12);
                prim->x2 = (u16)line->x0;
                prim->y2 = (u16)line->y0;
                prim->x3 = (u16)line->x1;
                prim->y3 = (u16)line->y1;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, sc->otz0);
            }
        } else {
            D_m4a1_javelin_8012EB64 = 1;
        }
    } else {
        D_m4a1_javelin_8012EB64 = 1;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlayPointPairScratch));
}

/// Draws the javelin launcher's targeting reticle: a `LINE_F2` between the two
/// world-space points `p0` and `p1` plus three fans of `POLY_G4` wedges, all
/// dropped if either endpoint fails its `RTPS` `FLAG` check (which also arms
/// `D_m4a1_javelin_8012EB66` so the next frame re-measures the angle). Bit 1 of
/// `flags` forces that re-measurement: `ratan2` of the screen-space delta gives
/// the reticle's roll, which is cached in `D_m4a1_javelin_8012EB62` and reused
/// on the frames that do not. Bit 0 adds the near-end fan. `color` is a packed
/// `0x0RGB` nibble triple; each nibble is widened to a byte, biased by the
/// 8-unit dither of `gDisplayState.animFrame` and halved. Each fan is four
/// quarter-turn wedges of radius `0x4000 / otz`, so the reticle keeps a
/// constant on-screen size as the target moves away.
static void func_m4a1_javelin_8011E4A8(SVECTOR* p0, SVECTOR* p1, u16 flags, u16 color)
{
    u8*                      head;
    OverlayPointPairScratch* sc;
    LINE_F2*                 line;
    POLY_G4*                 poly;
    u32                      dither;
    u32                      c;
    u8                       r;
    u8                       g;
    u8                       b;
    u16                      ang;
    s32                      i;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(OverlayPointPairScratch);
    sc                       = (OverlayPointPairScratch*)(head - sizeof(OverlayPointPairScratch));

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(p0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)head)[-1].sx0);
    gte_stflg(&((OverlayPointPairScratch*)head)[-1].flag);
    if (sc->flag < 0) {
        goto fail;
    }
    gte_stszotz(&((OverlayPointPairScratch*)head)[-1].otz0);
    ((OverlayPointPairScratch*)head)[-1].otz0++;
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)head)[-1].sx1);
    gte_stflg(&((OverlayPointPairScratch*)head)[-1].flag);
    if (sc->flag < 0) {
        goto fail;
    }
    gte_stszotz(&((OverlayPointPairScratch*)head)[-1].otz1);

    sc->otz1++;
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    dither = (gDisplayState.animFrame & 1) * 8;
    c      = color & 0xFFFF;
    r      = (((c >> 4) & 0xF0) + dither) >> 1;
    g      = ((c & 0xF0) + dither) >> 1;
    b      = (((color & 0xF) << 4) + dither) >> 1;
    setRGB0(line, r, g, b);
    line->x0 = sc->sx0;
    line->y0 = sc->sy0;
    line->x1 = sc->sx1;
    line->y1 = sc->sy1;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((OverlayPointPairScratch*)head)[-1].otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            line);
    gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, ((OverlayPointPairScratch*)head)[-1].otz0);
    sc->radius0 = 0x4000 / ((OverlayPointPairScratch*)head)[-1].otz0;
    sc->radius1 = 0x4000 / sc->otz1;

    if ((flags & 2) || D_m4a1_javelin_8012EB66 != 0) {
        ang                     = ratan2(line->y1 - line->y0, line->x0 - line->x1);
        D_m4a1_javelin_8012EB62 = ang;
        D_m4a1_javelin_8012EB66 = 0;
        for (i = (s16)ang; i < (s16)ang + 0x800; i += 0x400) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setPolyG4(poly);
            setRGB0(poly, 0, 0, 0);
            setRGB1(poly, 0, 0, 0);
            setRGB2(poly, r, g, b);
            setRGB3(poly, 0, 0, 0);
            poly->x0 = (u16)line->x1 + ((sc->radius1 * rsin(i + 0x800)) >> 12);
            poly->y0 = (u16)line->y1 + ((sc->radius1 * rcos(i + 0x800)) >> 12);
            poly->x1 = (u16)line->x1 + ((sc->radius1 * rsin(i + 0xA00)) >> 12);
            poly->y1 = (u16)line->y1 + ((sc->radius1 * rcos(i + 0xA00)) >> 12);
            poly->x2 = line->x1;
            poly->y2 = line->y1;
            poly->x3 = (u16)line->x1 + ((sc->radius1 * rsin(i + 0xC00)) >> 12);
            poly->y3 = (u16)line->y1 + ((sc->radius1 * rcos(i + 0xC00)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), poly);
            gpuSetPrimitiveBlendMode(poly, GPU_BLEND_ADD, sc->otz1);
        }
    } else {
        ang = D_m4a1_javelin_8012EB62;
    }

    if (flags & 1) {
        for (i = (s16)ang; i < (s16)ang + 0x800; i += 0x400) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setPolyG4(poly);
            setRGB0(poly, 0, 0, 0);
            setRGB1(poly, 0, 0, 0);
            setRGB2(poly, r, g, b);
            setRGB3(poly, 0, 0, 0);
            poly->x0 = (u16)line->x0 + ((sc->radius0 * rsin(i)) >> 12);
            poly->y0 = (u16)line->y0 + ((sc->radius0 * rcos(i)) >> 12);
            poly->x1 = (u16)line->x0 + ((sc->radius0 * rsin(i + 0x200)) >> 12);
            poly->y1 = (u16)line->y0 + ((sc->radius0 * rcos(i + 0x200)) >> 12);
            poly->x2 = line->x0;
            poly->y2 = line->y0;
            poly->x3 = (u16)line->x0 + ((sc->radius0 * rsin(i + 0x400)) >> 12);
            poly->y3 = (u16)line->y0 + ((sc->radius0 * rcos(i + 0x400)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), poly);
            gpuSetPrimitiveBlendMode(poly, GPU_BLEND_ADD, sc->otz0);
        }
    }

    for (i = (s16)ang; i < (s16)ang + 0x800; i += 0x400) {
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setPolyG4(poly);
        setRGB0(poly, 0, 0, 0);
        setRGB1(poly, 0, 0, 0);
        setRGB2(poly, r, g, b);
        setRGB3(poly, r, g, b);
        poly->x0 = (u16)line->x0 + ((sc->radius0 * rsin((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->y0 = (u16)line->y0 + ((sc->radius0 * rcos((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->x1 = (u16)line->x1 + ((sc->radius1 * rsin((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->y1 = (u16)line->y1 + ((sc->radius1 * rcos((s16)ang + ((i - (s16)ang) * 2))) >> 12);
        poly->x2 = line->x0;
        poly->y2 = line->y0;
        poly->x3 = line->x1;
        poly->y3 = line->y1;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), poly);
        gpuSetPrimitiveBlendMode(poly, GPU_BLEND_ADD, sc->otz0);
    }
    goto done;

fail:
    D_m4a1_javelin_8012EB66 = 1;
done:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlayPointPairScratch));
}

/* `otz0` is taken before the branch on purpose: the address is the same one
   already held for `sc`, so CSE turns it into the copy the ROM keeps, which a
   `&sc->otz0` inside the `if` would fold away. */
/// Links one Gouraud `LINE_G2` between the world-space points `p0` and `p1`
/// into `gGpuCurrentOt`, dropped entirely if either endpoint fails its `RTPS`
/// `FLAG` check. Only the first vertex is lit: `brightness` goes into blue,
/// half of it into green and a quarter into red, so the tracer fades from a
/// blue-white head to black.
static void func_m4a1_javelin_8011EE78(SVECTOR* p0, SVECTOR* p1, u16 brightness)
{
    u8*                      head;
    _M4a1JavelinLineScratch* sc;
    LINE_G2*                 line;
    s32*                     otz0;

    head                                          = SCRATCH_STACK_CURSOR(u8);
    sc                                            = (_M4a1JavelinLineScratch*)(head - sizeof(_M4a1JavelinLineScratch));
    SCRATCH_STACK_CURSOR(_M4a1JavelinLineScratch) = sc;
    otz0                                          = &sc->otz0;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(p0);
    gte_rtps();
    gte_stsxy(&sc->sxy0);
    gte_stflg(&sc->flag);
    if (sc->flag >= 0) {
        gte_stszotz(otz0);
        sc->otz0++;
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&sc->sxy1);
        gte_stflg(&sc->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&sc->otz1);
            sc->otz1++;
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG2(line);
            setRGB0(line, brightness >> 2, brightness >> 1, brightness);
            setRGB1(line, 0, 0, 0);
            line->x0 = sc->sxy0.vx;
            line->y0 = sc->sxy0.vy;
            line->x1 = sc->sxy1.vx;
            line->y1 = sc->sxy1.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, sc->otz0);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_M4a1JavelinLineScratch);
}

/// Contact-flash palette: VRAM X=48 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT getClut(48, 267)
/// Texel width and horizontal stride of each of the launch flash's eight texture frames.
#define SPRITE_QUAD_CELL_WIDTH 32
/// Inclusive top texel row of the launch-flash strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x18
#define SPRITE_QUAD_V1    0x37
/// Perspective-sizing multiplier for Javelin's launch flash.
///
/// Uses the cell's inclusive 31-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

static void func_m4a1_javelin_8011F4A4(const long* arg0)
{
    if (arg0 == NULL) {
        D_m4a1_javelin_8012EB70 = 0;
        return;
    }
    D_m4a1_javelin_8012EB68.vx = arg0[0];
    D_m4a1_javelin_8012EB68.vy = arg0[1];
    D_m4a1_javelin_8012EB70    = 1;
    D_m4a1_javelin_8012EB68.vz = arg0[2];
}

void func_m4a1_javelin_8011F4E8(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
        return;
    }

    mem->age++;
    if (arg0->state == 0) {
        mem->scale      = 0x200;
        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
        arg0->state     = 1;
    }
    spriteQuadDraw(coord->workm.t, mem->age - 1, mem->scale, mem->angle);
    if (mem->age == 8) {
        effectKillTask(mem, arg0);
    }
}

/// Per-frame firing state machine for the M4A1 javelin launcher, the sibling of
/// `func_m4a1_grenade_8011D1EC`. State 0 arms the shot and raises the weapon
/// (clip 8 instead of 1 when it was already up), state 1 waits for that clip.
/// State 2 branches on `field_97F`: a held trigger (bit 0) drops into the
/// three-round burst of state 3, a tap (bit 1) fires the single 0x101 javelin
/// of state 5, and anything else falls straight into the burst. State 3 counts
/// `field_934` down to each round, spending one javelin, playing `0x201D0004`
/// and spawning the muzzle flash; on the frame `field_934` reaches 2 it drops
/// the aim lock and spawns the 0x6003B impact marker, which state 4 also does
/// before parking in state 7. States 5 and 6 run the flight timer and feed the
/// tracked point to `func_m4a1_javelin_8011F4A4` (or clear it when nothing is
/// in range) so the guide line is drawn. State 7 runs the recoil timer down and
/// hands back to `func_80106550` once `func_80105894` is done or the timer has
/// run out.
///
/// `gPlayerStatus.weaponSlotItem` is the low byte `func_801061F0` packs into
/// `GameActor::collisionBodies[GAME_ACTOR_BODY_WEAPON].key`. Reading it through the struct rather than as a bare
/// `extern u8` at 0x80073BAA is what keeps GCC from hoisting the `lbu` above
/// the `actor->` stores: a scalar global and a struct field do not alias, so
/// the scheduler is free to move the load, and the block comes out reordered.
void func_m4a1_javelin_8011F5D4(Task* arg0)
{
    GameActor*  actor;
    GfxCoord*   coord;
    GfxCoord*   spot;
    EffectWork* eff;
    s32         anim;
    s32         delay;
    s32         tick;
    u16         count;

    SCRATCH_STACK_RESERVE_BYTES(0x58);
    coord        = arg0->extra.tmd->coords;
    actor        = arg0->work;
    spot         = SCRATCH_STACK_CURSOR(GfxCoord);
    spot->parent = NULL;

    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->rumblePosted = 0;
            if (actor->attackButton & 1) {
                actor->statePhase                                     = 3;
                actor->attackCancelTicks                              = 9;
                actor->turnRateIndex                                  = 0;
                actor->stateTimer                                     = 0;
                actor->actionValue                                    = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | 0x21D00;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
                func_80106238(arg0, 0, 1);
            } else if (actor->attackButton & 2) {
                actor->statePhase                                     = 5;
                actor->turnRateIndex                                  = 2;
                actor->attackCancelTicks                              = 0x1C;
                actor->stateTimer                                     = 6;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = 0x21D1F;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                func_80106238(arg0, 0, 0);
                Gp_ConsumeSlotQty(0x9C, 0x101);
                eff = Gp_SpawnEff(EFFECT_JAVELIN_GUIDE_BEAM,
                                  actor->equipmentTasks[1]->extra.tmd->coords,
                                  0x1D, NULL);
                if (eff != NULL) {
                    taskReparent(actor->equipmentTasks[1], eff->task);
                }
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201D0005, 1);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 3);
                break;
            }
            /* fallthrough */
        case 3:
            count = actor->actionValue;
            if (actor->actionValue != 0) {
                delay = actor->stateTimer;
                if (delay == 0) {
                    actor->actionValue                                    = count - 1;
                    actor->stateTimer                                     = 3;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(0x9C, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201D0004, 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x1D, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                } else {
                    delay--;
                    actor->stateTimer = delay;
                    if (delay == 2) {
                        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                        if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                            Gp_SpawnEff(EFFECT_IMPACT_SPARK, spot, 0, NULL);
                            Gp_PlayObjSfx(spot, 0x17, 1);
                        }
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 7;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_SpawnEff(EFFECT_IMPACT_SPARK, spot, 0, NULL);
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            break;
        case 5:
        case 6:
            tick              = actor->stateTimer - 1;
            actor->stateTimer = tick;
            if (tick == 0) {
                if (actor->statePhase == 5) {
                    actor->statePhase                                     = 6;
                    actor->stateTimer                                     = 0x1C;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    actor->statePhase                                     = 7;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
            }
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                func_m4a1_javelin_8011F4A4(spot->workm.t);
                eff = Gp_SpawnEff(EFFECT_M4A1_JAVELIN_CONTACT_FLASH, spot, 0, NULL);
                if (eff != NULL) {
                    taskReparent(actor->equipmentTasks[1], eff->task);
                }
            } else {
                func_m4a1_javelin_8011F4A4(NULL);
            }
            break;
        case 7:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x58);
}
