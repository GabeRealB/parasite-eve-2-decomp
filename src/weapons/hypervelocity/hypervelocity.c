#include "weapons/hypervelocity.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "hypervelocity_private.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
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
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"
/// Signed effect-age argument whose low bit selects one of the two flame cells.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"
#include "../../shared/jet_cone.h"
#include "../../shared/ground_glow.h"

/// 0x18-byte scratchpad block `func_hypervelocity_8011F724` reserves for one
/// frame of the barrel's recoil kick. `dir` receives the third column of the
/// weapon coordinate matrix from `Gfx_MatrixCol2`; each axis is then scaled by
/// the remaining recoil ticks over a per-tick divisor, negated, and added to
/// the coordinate's translation so the gun rides back along its own barrel.
typedef struct HyperRecoil {
    /* 0x00 */ s32     vx;
    /* 0x04 */ s32     vy;
    /* 0x08 */ s32     vz;
    /* 0x0C */ byte    pad_C[4];
    /* 0x10 */ SVECTOR dir;
} HyperRecoil;
STATIC_ASSERT_SIZEOF(HyperRecoil, 0x18);

/// 0x58-byte scratchpad block `func_hypervelocity_8011EC1C` reserves for the
/// discharge cone. `hub` is the square collar sitting on the round itself and
/// `rim` the flared mouth in front of it; both are the unit quad
/// `D_80111E38` scaled in the round's own frame, rotated by its `workm` and
/// shifted onto its world position. Two opposed walls are then projected a
/// wall at a time - `sxy0`..`sxy3` are the four screen corners of the current
/// wall, `flag` the `gte_stflg` that rejects a wall behind the eye and `otz`
/// its `gte_stszotz` depth, which also picks the OT bucket.
typedef struct HyperConeScratch {
    /* 0x00 */ SVECTOR rim[4];
    /* 0x20 */ SVECTOR hub[4];
    /* 0x40 */ s32     otz;
    /* 0x44 */ s32     flag;
    /* 0x48 */ DVECTOR sxy0;
    /* 0x4C */ DVECTOR sxy1;
    /* 0x50 */ DVECTOR sxy2;
    /* 0x54 */ DVECTOR sxy3;
} HyperConeScratch;
STATIC_ASSERT_SIZEOF(HyperConeScratch, 0x58);

/// 0x38 block the round's spawn state allocates with `memCalloc` and parks in
/// `Task::work`. It leads with the `WorldCollisionBody` list node `func_hypervelocity_8011F11C`
/// hands back to `Gp_UnlinkObj` on teardown; `rec` is the single-entry
/// `WorldCollisionContact` collision table `obj.context.contacts` points at, and its `flags` is set
/// to 2 (the last-element bit) instead of going through `Gp_InitRec18Table`.
typedef struct HyperBeam {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[1];
} HyperBeam;
STATIC_ASSERT_SIZEOF(HyperBeam, 0x38);

/// Translation of the round's own coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x240, 0x80)`.
static SVECTOR D_hypervelocity_8011FB74 = { 0, 0x240, 0x80, 0 };

static void func_hypervelocity_8011F11C(Task* task);
static void func_hypervelocity_8011F6A0(Task* task);

static void func_hypervelocity_8011EC1C(GfxCoord* coord, s16 age, s32 radius, u8* rgb);
static void func_hypervelocity_8011F374(Task* arg0);
static void func_hypervelocity_8011F570(Task* arg0);
static void func_hypervelocity_8011F694(Task* arg0);
static void func_hypervelocity_8011F724(Task* arg0);

/// Per-frame task for the muzzle flare the hypervelocity round leaves behind.
/// `Task::spawnArg2` is the `EffectWork` holding the flare's drift
/// (`move` / `move.vy` / `move.vz`), its age (`age`), the ring
/// brightness (`scale`), the ring radius (`angle`), the arc brightness
/// (`period`) and the per-frame brightness step (`step`);
/// `Task::extra` reaches the coordinate it hangs on and `Task::spawnArg1` is
/// the charge counter the firing code drives. Nonzero effect control
/// (`gRoomEffectState->effectControl`) freezes the task; cancellation at 4 or more restarts
/// it at state 1.
///
/// - State 0 hangs the coordinate off `EffectWork::parent` at the fixed muzzle
///   offset `D_hypervelocity_8011FB74` with an identity rotation, then falls
///   through to state 1, which waits for `spawnArg1` to reach 1 before arming
///   the charge at state 2.
/// - State 2 charges: it jitters the drift, sparks every other frame, and
///   refreshes transient light slot 1 with narrow (`0x100` / `0x1000`) falloff and
///   random blue intensity in `0x400..0xB00`. A negative `spawnArg1` cancels back to state
///   1; holding past frame 0x40 caps the charge at 0x18; once the charge is 2
///   or more it seeds the ring and moves to state 3 with the brightness step
///   scaled so the ring fills over `spawnArg1` frames.
/// - State 3 fires: the light widens to `0x400` / `0x4000`, the ring brightens
///   by `step` and grows by 8 a frame, both ring halves are drawn, and past
///   half brightness the arc is drawn too with a one-shot report. Running the
///   charge out spawns the discharge effect as a child task and moves to state
///   4; a negative charge cancels back to state 1 with the stop sound.
/// - State 4 fades the ring out 0x20 a frame while spawning smoke off a random
///   one of the player's two hand coordinates, and returns to state 1 once the
///   flare is 0x6F frames old or the charge goes negative.
void func_hypervelocity_8011D1E8(Task* task)
{
    u8                             rgb[3];
    GfxCoord*                      coord;
    GfxCoord*                      light;
    GfxCoord*                      player;
    EffectWork*                    work;
    EffectWork*                    eff;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    GfxRotationWords*              dstm;
    s32                            pan;

    work      = task->spawnArg2.pointer;
    lightSlot = &gWorldCoordTransientPointLights[1];
    light     = &lightSlot->light.head.transform.coord;
    slot      = &lightSlot->light;
    coord     = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            task->state = 1;
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
            coord->coord.t[0]   = D_hypervelocity_8011FB74.vx;
            coord->coord.t[1]   = D_hypervelocity_8011FB74.vy;
            coord->coord.t[2]   = D_hypervelocity_8011FB74.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
            /* fallthrough */
        case 1:
            if (task->spawnArg1.value == 1) {
                task->state = 2;
                work->age   = 0;
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            work->move.vy = -((work->age & 0xF) << 5);
            if (work->age & 1) {
                Gp_SpawnEff(0x600E1, coord, 0x180, &work->move);
            }
            lightSlot->framesLeft = 4;
            slot->inner           = 0x100;
            slot->outer           = 0x1000;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            slot->head.color.b    = ((gRandomLcgState >> 16) & 0x700) + 0x400;
            slot->head.color.r    = (u16)slot->head.color.b >> 1;
            slot->head.color.g    = slot->head.color.b >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
            light->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->spawnArg1.value < 0) {
                task->spawnArg1.value = 0;
                task->state           = 1;
                return;
            }
            if (work->age >= 0x41) {
                task->spawnArg1.value = 0x18;
            }
            if (task->spawnArg1.value >= 2) {
                work->scale  = 0;
                work->angle  = 0x40;
                work->period = 0;
                work->step   = 0x100 / task->spawnArg1.value;
                task->state  = 3;
            }
            return;
        case 3:
            Gp_UpdateCoord(coord);
            work->move.vy = -((work->age & 0xF) << 6);
            Gp_SpawnEff(0x600E0, coord, 0x180, &work->move);
            lightSlot->framesLeft = 4;
            slot->inner           = 0x400;
            slot->outer           = 0x4000;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            slot->head.color.b    = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            slot->head.color.r    = (u16)slot->head.color.b >> 1;
            slot->head.color.g    = slot->head.color.b >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
            light->composeStamp = GRAPHICS_COORD_DIRTY;
            work->scale        += work->step;
            if (work->scale >= 0x100) {
                work->scale = 0xFF;
            }
            work->angle += 8;
            if (work->angle >= 0x201) {
                work->angle = 0x200;
            }
            rgb[0] = work->scale >> 1;
            rgb[1] = work->scale >> 1;
            rgb[2] = work->scale;
            Gp_DrawRing(coord, work->angle, rgb);
            Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
            if (work->scale >= 0x81) {
                if (work->period == 0) {
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(0x20160006, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->period += (u16)work->step * 2;
                if (work->period >= 0x100) {
                    work->period = 0xFF;
                }
                rgb[0] = work->period >> 1;
                rgb[1] = work->period >> 1;
                rgb[2] = work->period;
                Gp_DrawArc(coord, (s16)((u16)task->spawnArg1.value * 128), 0x60, rgb);
            }
            if (task->spawnArg1.value < 0) {
                SndEvt_EnqueueType7(0x20160006, 1);
                task->spawnArg1.value = 0;
                task->state           = 1;
                return;
            }
            task->spawnArg1.value = task->spawnArg1.value - 1;
            if (task->spawnArg1.value == 0) {
                task->state = 4;
                eff         = Gp_SpawnEff(0x6000C, coord, 0, NULL);
                if (eff != NULL) {
                    taskReparent(task, eff->task);
                }
                work->scale = 0xFF;
            }
            return;
        case 4:
            Gp_UpdateCoord(coord);
            work->move.vy = -((work->age & 0xF) << 6);
            Gp_SpawnEff(0x600E1, coord, 0x180, &work->move);
            if (work->angle > 0) {
                rgb[0] = work->scale >> 1;
                rgb[1] = work->scale >> 1;
                rgb[2] = work->scale;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawFadeQuad(rgb, 1);
                work->scale = work->scale - 0x20;
                work->angle = work->angle - 0x20;
            }
            player          = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(0x60054, &player[(((gRandomLcgState >> 16) & 1) * 3) + 15], 0x2300, NULL);
            if (work->age >= 0x6F || task->spawnArg1.value < 0) {
                task->state = 1;
            }
            return;
    }
}

/// Per-frame task for the hypervelocity round in flight. `Task::spawnArg2` is
/// the `EffectWork` holding the round's velocity (`move` /
/// `move.vy` / `move.vz`), its age (`age`), the trail brightness
/// (`scale`), the ring spin (`angle`) and the ring's start angle
/// (`period`); `Task::extra` reaches the coordinate it flies on. Nonzero effect control
/// (`gRoomEffectState->effectControl`) winds the age back down instead of advancing, and
/// tears the round down at the cancellation threshold of 4.
///
/// - State 0 allocates the `HyperBeam` list node, copies the player's rotation
///   onto the round's own frame, rotates the fixed `(0, 0, 0x400)` muzzle
///   velocity through it, re-rolls the 16 trail jitters and the ring angle,
///   links the node, spawns the launch effect as a child task and claims room
///   -light slot 0.
/// - State 1 flies the round, draws the ring plus both trail halves, traces the
///   ground under it for a splash, and until frame 0x15 keeps spawning sparks.
///   It then re-aims the room light and asks `func_800DE7CC` whether the step
///   crossed geometry: a hit unlinks the node and switches to state 2, and
///   living past frame 0x15 releases the pool block.
/// - State 2 shrinks the ring by 0x40 a frame, spawning one more spark burst
///   per frame until the ring falls under 0x80.
void func_hypervelocity_8011D830(Task* task)
{
    GfxCoord                       ground;
    SVECTOR                        after;
    SVECTOR                        before;
    u8                             rgb[3];
    GfxCoord*                      coord;
    GfxCoord*                      player;
    GfxCoord*                      light;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    EffectWork*                    work;
    EffectWork*                    eff;
    HyperBeam*                     beam;
    GfxRotationWords*              destinationRotation;
    GfxRotationWords*              sourceRotation;
    u32                            ang;
    s32                            i;

    beam      = (HyperBeam*)task->work;
    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[0];
    light     = &lightSlot->light.head.transform.coord;
    slot      = &lightSlot->light;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        work->age = work->age - 1;
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state != 0) {
                Gp_UnlinkObj(&beam->obj);
            }
            effectKillTask(work, task);
        }
        return;
    }

    work->age = work->age + 1;
    switch (task->state) {
        case 0:
            beam = memCalloc(sizeof(HyperBeam), 0);
            if (beam == NULL) {
                work->age = 0;
                return;
            }
            task->exitCallback          = func_hypervelocity_8011F11C;
            player                      = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            destinationRotation         = (GfxRotationWords*)&coord->coord;
            sourceRotation              = (GfxRotationWords*)&player->coord;
            destinationRotation->m00M01 = sourceRotation->m00M01;
            destinationRotation->m02M10 = sourceRotation->m02M10;
            destinationRotation->m11M12 = sourceRotation->m11M12;
            destinationRotation->m20M21 = sourceRotation->m20M21;
            destinationRotation->m22    = sourceRotation->m22;
            coord->composeStamp         = GRAPHICS_COORD_DIRTY;
            gGfxViewCoord.composeStamp  = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            work->move.vx = 0;
            work->move.vy = 0;
            work->move.vz = 0x400;
            gte_SetRotMatrix(&player->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            for (i = 0; i < 0x10; i++) {
                gRandomLcgState             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_hypervelocity_8012EF0C[i] = (gRandomLcgState >> 16) & 0xFF;
            }
            work->scale                = 0xC0;
            work->angle                = 0x500;
            gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period               = (gRandomLcgState >> 16) & 0xFFF;
            task->work                 = beam;
            beam->obj.context.contacts = beam->rec;
            beam->obj.radius           = 0x800;
            beam->obj.coord            = coord;
            beam->obj.key              = 0x2161A;
            beam->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].flags = 2;
            beam->obj.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            eff                = Gp_SpawnEff(0x6000D, coord, 0, NULL);
            if (eff != NULL) {
                taskReparent(task, eff->task);
            }
            task->state           = 1;
            lightSlot->framesLeft = 4;
            slot->inner           = (work->index << 9) + 0x200;
            slot->outer           = slot->inner * 16;
            ang                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            slot->head.color.b    = ((ang >> 16) & 0x700) + 0x800;
            slot->head.color.r    = (u16)slot->head.color.b >> 1;
            slot->head.color.g    = slot->head.color.b >> 1;
            light->coord.t[0]     = coord->coord.t[0];
            light->coord.t[1]     = coord->coord.t[1];
            light->coord.t[2]     = coord->coord.t[2];
            light->composeStamp   = GRAPHICS_COORD_DIRTY;
            rgb[0]                = work->scale >> 2;
            rgb[1]                = work->scale >> 2;
            rgb[2]                = work->scale >> 1;
            gRandomLcgState       = ang;
            spriteQuadDraw(coord, work->age, work->angle, work->period);
            Gp_DrawRing(coord, work->angle, rgb);
            return;
        case 1:
            Gp_UpdateCoord(coord);
            before.vx                  = coord->workm.t[0];
            before.vy                  = coord->workm.t[1];
            before.vz                  = coord->workm.t[2];
            coord->coord.t[0]         += work->move.vx;
            coord->coord.t[1]         += work->move.vy;
            coord->coord.t[2]         += work->move.vz;
            coord->composeStamp        = GRAPHICS_COORD_DIRTY;
            gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            rgb[0]   = work->scale >> 2;
            rgb[1]   = work->scale >> 2;
            rgb[2]   = work->scale >> 1;
            spriteQuadDraw(coord, work->age, work->angle, work->period);
            Gp_DrawRing(coord, work->angle, rgb);
            jetConeDraw(coord, work->age, work->angle, 0);
            jetConeDraw(coord, work->age, work->angle, 1);
            if (gRoomEffectState->groundTraceEnabled != 0 && Gp_TraceGroundCoord(coord, &ground) == 1) {
                groundGlowDraw(&ground, work->angle);
            }
            if (work->age < 0x15) {
                Gp_SpawnEff(0x600E0, coord, 0x400, NULL);
                eff = Gp_SpawnEff(0x6000B, coord, 0, NULL);
                if (eff != NULL) {
                    taskReparent(task, eff->task);
                }
            }
            light->coord.t[0]     = coord->coord.t[0];
            light->coord.t[1]     = coord->coord.t[1];
            light->coord.t[2]     = coord->coord.t[2];
            light->composeStamp   = GRAPHICS_COORD_DIRTY;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            slot->head.color.b    = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            slot->head.color.r    = (u16)slot->head.color.b >> 1;
            lightSlot->framesLeft = 4;
            slot->head.color.g    = slot->head.color.b >> 1;
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                Gp_UnlinkObj(&beam->obj);
                task->state = 2;
                return;
            }
            if (work->age >= 0x15) {
                Gp_UnlinkObj(&beam->obj);
                effectKillTask(work, task);
                return;
            }
            Gp_ClearRec18Occupied(beam->rec);
            return;
        case 2:
            Gp_UpdateCoord(coord);
            work->angle = work->angle - 0x40;
            rgb[0]      = work->scale >> 2;
            rgb[1]      = work->scale >> 2;
            rgb[2]      = work->scale >> 1;
            spriteQuadDraw(coord, work->age, work->angle, work->period);
            Gp_DrawRing(coord, work->angle, rgb);
            if (work->angle < 0x80) {
                effectKillTask(work, task);
                return;
            }
            Gp_SpawnEff(0x600E0, coord, 0x400, NULL);
            return;
    }
}

#define JET_CONE_CLUT         0x42C1
#define JET_CONE_RIM_SHORT    0x600
#define JET_CONE_RIM_LONG     0x800
#define JET_CONE_FRAME_JITTER D_hypervelocity_8012EF0C
#include "../../shared/jet_cone_draw.inc.c"

#define SPRITE_QUAD_TPAGE     0x29
#define SPRITE_QUAD_CLUT      0x428B
#define SPRITE_QUAD_CELL_W    0x38
#define SPRITE_QUAD_CELL_MASK 1
#define SPRITE_QUAD_U_BASE    0x70
#define SPRITE_QUAD_V0        0xC8
#define SPRITE_QUAD_V1        0xFF
#define SPRITE_QUAD_SCALE     55
#include "../../shared/sprite_quad_draw.inc.c"

#define GROUND_GLOW_R    0x30
#define GROUND_GLOW_G    0x30
#define GROUND_GLOW_B    0x30
#define GROUND_GLOW_CLUT 0x428B
#include "../../shared/ground_glow_draw.inc.c"

/// Draws the discharge cone `func_hypervelocity_8011F270` leaves behind: two
/// opposed `POLY_FT4` walls flaring out of `coord`, built in the scratchpad as
/// a `HyperConeScratch`. The collar sits at y `0x700` and is `radius` wide,
/// the mouth rises to `0x600 - age * 256 / 2` and flares to
/// `radius + age * 128 + 0x200`, so the cone climbs and opens as the puff
/// ages; both rings are `radius` deep in z. Wall `i` is therefore the quad
/// `rim[i]`, `rim[i + 2]`, `hub[i]`, `hub[i + 2]` - the -x pair, then the +x
/// pair. Each wall is a frame of the same six-frame strip at tpage 0x2A the
/// trail uses, picked by the stored jitter `D_hypervelocity_8012EF0C[i]` plus
/// `age`, tinted by `rgb` and linked into the OT bucket its own projected
/// depth names. Walls the GTE flags as behind the eye are dropped.
static void func_hypervelocity_8011EC1C(GfxCoord* coord, s16 age, s32 radius, u8* rgb)
{
    HyperConeScratch* sc;
    POLY_FT4*         prim;
    GpQuadCorner*     tbl;
    SVECTOR*          vert;
    MATRIX*           rot;
    s32               i;
    s32               rise;
    s32               top;
    u16               flare;
    s32               half;
    s32               u0;

    /* `rise` is built in two steps and then walked in place, and `half` is a
       second spelling of `radius`, because the ROM keeps both copies the
       folded forms would have coalesced away. `flare` is 16-bit on purpose:
       it only ever feeds a halfword store, and widening it moves the whole
       prologue's register assignment. `vert` reaches `hub[i]` through
       `rim[i]` so the `gte_ldv0` / `gte_stsv` address stays a register of its
       own instead of being shared with the field stores. */
    sc    = SCRATCH_STACK_RESERVE_BLOCK(HyperConeScratch);
    rise  = age;
    rise  = rise << 7;
    top   = 0x600 - rise;
    rise  = rise + 0x200;
    flare = radius + rise;
    half  = radius;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    rot = &coord->workm;
    tbl = D_80111E38;
    do {
        sc->rim[i].vx = tbl[i].x * flare;
        sc->rim[i].vy = top;
        sc->rim[i].vz = tbl[i].y * half;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->rim[i]);
        gte_rtv0();
        gte_stsv(&sc->rim[i]);
        (u16) sc->rim[i].vx = (u16)sc->rim[i].vx + (u16)coord->workm.t[0];
        (u16) sc->rim[i].vy = (u16)sc->rim[i].vy + (u16)coord->workm.t[1];
        (u16) sc->rim[i].vz = (u16)sc->rim[i].vz + (u16)coord->workm.t[2];
        vert                = &sc->rim[i] + 4;
        vert->vx            = tbl[i].x * radius;
        vert->vy            = 0x700;
        vert->vz            = tbl[i].y * half;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->hub[i]);
        gte_rtv0();
        gte_stsv(&sc->hub[i]);
        (u16) vert->vx = (u16)vert->vx + (u16)coord->workm.t[0];
        i++;
        (u16) vert->vy = (u16)vert->vy + (u16)coord->workm.t[1];
        (u16) vert->vz = (u16)vert->vz + (u16)coord->workm.t[2];
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        gte_ldv0(&sc->rim[i]);
        gte_rtps();
        gte_stsxy(&sc->sxy0);
        gte_ldv3(&sc->rim[i + 2], &sc->hub[i], &sc->hub[i + 2]);
        gte_rtpt();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&sc->flag);
        if (sc->flag >= 0) {
            gte_stszotz(&sc->otz);
            sc->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setRGB0(prim, rgb[0], rgb[1], rgb[2]);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u0          = (s16)((D_hypervelocity_8012EF0C[i] + age) % 6) * 40;
            prim->u0    = u0;
            prim->v0    = 0x60;
            prim->u1    = u0 + 0x27;
            prim->v1    = 0x60;
            prim->u2    = u0;
            prim->u3    = u0 + 0x27;
            prim->v2    = 0x87;
            prim->v3    = 0x87;
            setXY4(prim, sc->sxy0.vx, sc->sxy0.vy, sc->sxy1.vx, sc->sxy1.vy, sc->sxy2.vx, sc->sxy2.vy, sc->sxy3.vx,
                   sc->sxy3.vy);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        }
        i++;
    } while (i < 2);
    SCRATCH_STACK_RELEASE_BLOCK(HyperConeScratch);
}

/// Exit callback: unlinks the collision node leading `Task::work`, if one was
/// linked, and releases the `EffectWork` in `Task::spawnArg2`. M4A1 Pyke
/// carries an identical copy.
static void func_hypervelocity_8011F11C(Task* task)
{
    WorldCollisionBody* obj = task->work;
    void*               mem = task->spawnArg2.pointer;

    if (obj != NULL) {
        Gp_UnlinkObj(obj);
    }
    effectKillTask(mem, task);
}

void func_hypervelocity_8011F168(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         val;
    u8          rgb[3];

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

    Gp_UpdateCoord(coord);
    mem->age++;
    if (arg0->state == 0) {
        mem->scale  = 0xF0;
        mem->angle  = 0x100;
        arg0->state = 1;
    }
    rgb[0] = mem->scale >> 1;
    rgb[1] = mem->scale >> 1;
    rgb[2] = mem->scale;
    Gp_DrawBand(coord, mem->angle, rgb);
    mem->angle += 0x40;
    val         = mem->scale - 0x10;
    mem->scale  = val;
    if (val < 0x10) {
        effectKillTask(mem, arg0);
    }
}

void func_hypervelocity_8011F270(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         val;
    u8          rgb[3];

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

    Gp_UpdateCoord(coord);
    mem->age++;
    if (arg0->state == 0) {
        mem->scale  = 0x80;
        mem->angle  = 0x200;
        arg0->state = 1;
    }
    rgb[0] = mem->scale;
    rgb[1] = mem->scale;
    rgb[2] = mem->scale;
    func_hypervelocity_8011EC1C(coord, mem->age, mem->angle, rgb);
    mem->angle += 0x60;
    val         = mem->scale - 8;
    mem->scale  = val;
    if (val < 6) {
        effectKillTask(mem, arg0);
    }
}

static void func_hypervelocity_8011F374(Task* arg0)
{
    Task*             parent;
    TmdObject*        extra;
    TmdObject*        playerExtra;
    GfxCoord*         coord;
    Task*             work;
    GfxRotationWords* mat;
    s16               count;

    parent      = arg0->parent;
    work        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    extra       = arg0->extra.tmd;
    playerExtra = work->extra.tmd;
    coord       = extra->coords;

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = playerExtra->flags;
    extra->colorMtx     = playerExtra->colorMtx;
    extra->lightMtx     = playerExtra->lightMtx;

    SCRATCH_STACK_RESERVE_BYTES(0x10);
    switch (arg0->spawnArg1.value & 0xF) {
        case 0:
            if (*(u32*)&((GameActor*)work->work)->mode != 0x40000) {
                arg0->spawnArg1.value = 0;
            }
            break;
        case 1:
            if (parent->spawnArg1.value & 0x10) {
                if (arg0->killCountdown < 0x3C) {
                    arg0->killCountdown = arg0->killCountdown + 1;
                }
            } else if (arg0->killCountdown > 0) {
                count               = arg0->killCountdown - 1;
                arg0->killCountdown = count;
                if (count == 0) {
                    SndEvt_EnqueueType7(0x20160004, 1);
                }
            }
            coord->coord.t[0] = 0;
            coord->coord.t[1] = -arg0->killCountdown * 4;
            coord->coord.t[2] = -0x16;
            break;
        case 2:
            if (parent->spawnArg1.value & 0x20) {
                if (coord->param.rot.vx >= -0x3FF) {
                    coord->param.rot.vx = coord->param.rot.vx - 0x110;
                }
            } else if (coord->param.rot.vx < 0) {
                coord->param.rot.vx = coord->param.rot.vx + 0x110;
            }
            coord->coord.t[0] = -0x14;
            coord->coord.t[1] = -0x15C;
            coord->coord.t[2] = 0xA8;

            mat         = (GfxRotationWords*)&coord->coord;
            mat->m00M01 = ONE;
            mat->m02M10 = 0;
            mat->m11M12 = ONE;
            mat->m20M21 = 0;
            mat->m22    = ONE;
            RotMatrixX(coord->param.rot.vx, &coord->coord);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_hypervelocity_8011F570(Task* arg0)
{
    Task*      child;
    TmdObject* childExtra;
    TmdObject* extra;
    GfxCoord*  coord;

    extra               = arg0->extra.tmd;
    coord               = extra->coords;
    arg0->state        += 1;
    arg0->exitCallback  = func_hypervelocity_8011F6A0;
    arg0->killCountdown = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    if (!(arg0->spawnArg1.value & 0xF)) {
        child = Task_Spawn(7, 0x70, 1, 0);
        if (child != NULL) {
            child->extra.tmd->coords->parent = coord;
            childExtra                       = child->extra.tmd;
            childExtra->colorMtx             = extra->colorMtx;
            childExtra->lightMtx             = extra->lightMtx;
            taskReparent(arg0, child);
        }
        child = Task_Spawn(7, 0x74, 2, 0);
        if (child != NULL) {
            child->extra.tmd->coords->parent = coord;
            childExtra                       = child->extra.tmd;
            childExtra->colorMtx             = extra->colorMtx;
            childExtra->lightMtx             = extra->lightMtx;
            taskReparent(arg0, child);
            coord->coord.t[0] = -6;
            coord->coord.t[1] = -0x3C;
            coord->coord.t[2] = -0x16;
        }
    }
}

static void func_hypervelocity_8011F694(Task* arg0)
{
    arg0->state = 3;
}

/// Exit callback: kills the task.
static void func_hypervelocity_8011F6A0(Task* task)
{
    taskKill(task);
}

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_hypervelocity_8011F6C0(Task* arg0)
{
    TaskFunc states[4] = {
        func_hypervelocity_8011F570,
        func_hypervelocity_8011F374,
        func_hypervelocity_8011F694,
        func_hypervelocity_8011F6A0,
    };

    states[arg0->state](arg0);
}

/// Per-frame state machine for the hypervelocity's charge-up shot. Case 0 arms
/// the charge: it resets the weapon slots, wakes the muzzle-glow task
/// (`field_914`), sets the charge bit on the barrel effect task (`field_91C`)
/// and starts the wind-up animation. Case 1 runs the charge while the fire
/// button is still held (`field_962 & 0xA`): the charge ticks up, crosses a
/// half-way mark at 60 that adds the second glow stage, and completes at 90 by
/// consuming a round and firing. Releasing the button early jumps straight to
/// case 3 and cancels both loops. Case 2 is the 0x15-tick recoil: for the last
/// 18 ticks the third column of the weapon coordinate is scaled by the
/// remaining ticks over 378 (or 244 on the first tick) and subtracted from the
/// coordinate's translation, kicking the gun back along its own barrel.
static void func_hypervelocity_8011F724(Task* arg0)
{
    u8*          head;
    HyperRecoil* rec;
    GameActor*   actor;
    GfxCoord*    coord;
    Task*        eff;
    s32          div;
    s32          count;
    s32          step;

    head                              = SCRATCH_STACK_CURSOR(u8);
    rec                               = (HyperRecoil*)(head - 0x18);
    SCRATCH_STACK_CURSOR(HyperRecoil) = rec;
    actor                             = arg0->work;
    eff                               = actor->equipmentTasks[1];
    switch (actor->statePhase) {
        case 0:
            actor->mode                              = GAME_ACTOR_MODE_NORMAL;
            actor->state                             = 4;
            actor->movementMode                      = 0;
            actor->turnRateIndex                     = 0;
            actor->animationState                    = 0;
            actor->statePhase                        = 1;
            actor->weaponEffectTask->spawnArg1.value = 1;
            actor->stateTimer                        = 0;
            eff->spawnArg1.value                    |= 0x10;
            Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160003, 0);
            Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160005, 0);
            Gp_AnimPlayChildSlotsEx(arg0, 0xE, 0, 3);
            /* fallthrough */
        case 1:
            if (actor->padHeld & 0xA) {
                count             = actor->stateTimer + 1;
                actor->stateTimer = count;
                if (count >= 0x5A) {
                    actor->rumblePosted = 0;
                    actor->statePhase++;
                    eff->spawnArg1.value = 0;
                    actor->stateTimer    = 0x15;
                    Gp_ConsumeSlotQty(0x95, 1);
                    SndEvt_EnqueueType7(0x20160005, 1);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160007, 1);
                    Gp_AnimResetChildSlots(arg0, 0xB);
                } else if (count == 0x3C) {
                    eff->spawnArg1.value |= 0x20;
                    SndEvt_EnqueueType7(0x20160003, 1);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160002, 0);
                }
                SndEvt_EnqueueType7(0x20160004, 1);
            } else {
                actor->statePhase                        = 3;
                actor->weaponEffectTask->spawnArg1.value = -1;
                eff->spawnArg1.value                     = 0;
                SndEvt_EnqueueType7(0x20160003, 1);
                SndEvt_EnqueueType7(0x20160005, 1);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160004, 0);
                Gp_AnimPlayChildSlotsEx(arg0, 0xF, 0, 3);
            }
            break;
        case 2:
            step              = actor->stateTimer - 1;
            actor->stateTimer = step;
            if (step != 0) {
                if (step < 0x13) {
                    coord = arg0->extra.tmd->coords;
                    div   = 0x17A;
                    if (step == 0x12) {
                        div = 0xF4;
                    }
                    actor->movementSign = -1;
                    Gfx_MatrixCol2(&coord->coord, (SVECTOR*)(head - 8));
                    rec->vx            = -(rec->dir.vx * actor->stateTimer / div);
                    rec->vy            = -(rec->dir.vy * actor->stateTimer / div);
                    rec->vz            = -(rec->dir.vz * actor->stateTimer / div);
                    coord->coord.t[0] += rec->vx;
                    coord->coord.t[1] += rec->vy;
                    coord->coord.t[2] += rec->vz;
                }
            } else {
                actor->statePhase++;
            }
            /* fallthrough */
        case 3:
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
