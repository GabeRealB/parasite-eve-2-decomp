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
#include "gameplay/animation.h"
#include "types.h"
/// Signed effect-age argument whose low bit selects one of the two flame cells.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"
#include "../../shared/jet_cone.h"
#include "../../shared/ground_glow.h"

/// Scratch-stack workspace for the recoil that pushes the player back after a
/// hypervelocity shot.
///
/// On each frame of the push the player model's forward axis is read from its
/// root coordinate's local matrix, without normalization. `recoil` is each
/// component of it multiplied by the frames the recoil has left, divided by
/// the frame's divisor and negated, so the step points backwards and shortens
/// as the recoil runs out. It is added to the root coordinate's local
/// translation. Neither vector's `pad` is written.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order before returning. Pointers into the block must not survive its
/// release.
typedef struct {
    VECTOR  recoil;  // This frame's displacement of the model's root coordinate, in coordinate units
    SVECTOR forward; // Model's forward axis: the Z column of its root coordinate's local matrix, 4096 per unit
} _HypervelocityRecoilScratch;
STATIC_ASSERT_SIZEOF(_HypervelocityRecoilScratch, 0x18);

/// Vertices in each of the discharge cone's two squares, one per corner of
/// the unit quad both are scaled from. The collar's vertices follow the
/// mouth's at this distance in `_HypervelocityDischargeConeScratch::vertices`.
#define HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT 4
/// Walls drawn for one discharge cone: the local -X side, then the +X side.
#define HYPERVELOCITY_DISCHARGE_CONE_WALL_COUNT 2

/// Scratch-stack workspace for drawing the hypervelocity discharge cone.
///
/// The cone is spanned by two squares in an effect coordinate's local frame:
/// a mouth, and a narrower collar at a fixed local height. A drawer stages
/// each square's vertices in unit-quad corner order, rotates them by the
/// coordinate's world matrix, adds its translation and stores the result back
/// as a world position narrowed to 16 bits. `vertices` is one run of both
/// squares, the mouth's four vertices followed by the collar's: the collar
/// vertex of a corner is reached from its mouth vertex by stepping
/// `HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT` elements on.
///
/// Only two opposed walls are drawn, one at a time. Wall `i` takes its
/// vertices 0 and 1 from mouth vertices `i` and `i + 2`, the pair sharing a
/// local X sign, and its vertices 2 and 3 from the collar vertices of the same
/// corners. The remaining fields are refilled for each wall.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order after drawing. Pointers into the block must not survive its release.
typedef struct {
    SVECTOR vertices[2 * HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT]; // World-space vertices: the mouth square's, then the collar square's
    s32     otz;                                                            // Ordering-table depth of the current wall: SZ3 / 4 of its last vertex, plus 1
    s32     projectionFlags;                                                // GTE FLAG word after the wall's RTPT; bit 31 makes it negative and drops the wall
    DVECTOR sxy0;                                                           // Screen position of the current wall's vertex 0
    DVECTOR sxy1;                                                           // Screen position of vertex 1
    DVECTOR sxy2;                                                           // Screen position of vertex 2
    DVECTOR sxy3;                                                           // Screen position of vertex 3
} _HypervelocityDischargeConeScratch;
STATIC_ASSERT_SIZEOF(_HypervelocityDischargeConeScratch, 0x58);
// The drawer stages one mouth and one collar vertex per unit-quad corner.
STATIC_ASSERT(ARRAY_SIZE(D_80111E38) == HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT, hypervelocity_discharge_cone_square_vertex_count);

/// Packed collision key of the round in flight.
///
/// The high halfword is contact category 2, the category of the player's
/// weapon bodies. The identity follows the packing of the player's weapon
/// capsule key: the Hypervelocity's weapon index 0x16 in bits 8..15 and 0x1A
/// in the byte where that key carries the weapon-slot item. What 0x1A selects
/// for the bodies the round touches is unproven.
#define HYPERVELOCITY_ROUND_COLLISION_KEY 0x2161A

/// Collision block of one hypervelocity round in flight, allocated zeroed on
/// its first tick and kept at `Task::work`.
///
/// The sphere is linked on collision list 1 for as long as the round flies.
/// Its radius is a fixed 0x800 game-coordinate units, its packed key is
/// `HYPERVELOCITY_ROUND_COLLISION_KEY`, and its centre stays the origin of the
/// coordinate the round flies on. The sphere takes pair tests only, so it is
/// what the bodies it passes through record as a contact. The round itself
/// never reads `contacts`: a contact does not stop it, and its flight ends
/// only on room geometry, a separate segment test along each step, or on age.
/// The task's exit callback unlinks `Task::work` as a `WorldCollisionBody`,
/// which addresses this block while `body` remains its first member.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 1; pair tests are enabled after the link
    WorldCollisionContact contacts[1]; // One-entry table `body` borrows. The entry is marked LAST; occupied contacts are cleared each flight frame and never read
} _HypervelocityRoundBody;
STATIC_ASSERT_SIZEOF(_HypervelocityRoundBody, 0x38);

/// Translation of the round's own coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x240, 0x80)`.
static SVECTOR D_hypervelocity_8011FB74 = { 0, 0x240, 0x80, 0 };

static void func_hypervelocity_8011F11C(Task* task);
static void func_hypervelocity_8011F6A0(Task* task);

static void func_hypervelocity_8011EC1C(GfxCoord* coord, s16 age, s32 radius, u8* rgb);
static void func_hypervelocity_8011F374(Task* arg0);
static void func_hypervelocity_8011F570(Task* arg0);
static void func_hypervelocity_8011F694(Task* arg0);
void        func_hypervelocity_8011F724(Task* arg0);

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
            actorRenderComposeCoord(coord);
            work->move.vy = -((work->age & 0xF) << 5);
            if (work->age & 1) {
                Gp_SpawnEff(EFFECT_SPARK_FADE, coord, 0x180, &work->move);
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
            actorRenderComposeCoord(coord);
            work->move.vy = -((work->age & 0xF) << 6);
            Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x180, &work->move);
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
            effectDrawGouraudDisc(coord, work->angle, rgb);
            effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
            if (work->scale >= 0x81) {
                if (work->period == 0) {
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(SOUND_HYPERVELOCITY_DISCHARGE, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->period += (u16)work->step * 2;
                if (work->period >= 0x100) {
                    work->period = 0xFF;
                }
                rgb[0] = work->period >> 1;
                rgb[1] = work->period >> 1;
                rgb[2] = work->period;
                effectDrawOuterGlowBand(coord, (s16)((u16)task->spawnArg1.value * 128), 0x60, rgb);
            }
            if (task->spawnArg1.value < 0) {
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_DISCHARGE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                task->spawnArg1.value = 0;
                task->state           = 1;
                return;
            }
            task->spawnArg1.value = task->spawnArg1.value - 1;
            if (task->spawnArg1.value == 0) {
                task->state = 4;
                eff         = Gp_SpawnEff(EFFECT_HYPERVELOCITY_ROUND, coord, 0, NULL);
                if (eff != NULL) {
                    taskReparent(task, eff->task);
                }
                work->scale = 0xFF;
            }
            return;
        case 4:
            actorRenderComposeCoord(coord);
            work->move.vy = -((work->age & 0xF) << 6);
            Gp_SpawnEff(EFFECT_SPARK_FADE, coord, 0x180, &work->move);
            if (work->angle > 0) {
                rgb[0] = work->scale >> 1;
                rgb[1] = work->scale >> 1;
                rgb[2] = work->scale;
                effectDrawGouraudDisc(coord, work->angle, rgb);
                effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                work->scale = work->scale - 0x20;
                work->angle = work->angle - 0x20;
            }
            player          = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_DUST_PUFF, &player[(((gRandomLcgState >> 16) & 1) * 3) + 15], 0x2300, NULL);
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
/// - State 0 allocates the `_HypervelocityRoundBody`, copies the player's
///   rotation onto the round's own frame, rotates the fixed `(0, 0, 0x400)`
///   muzzle velocity through it, re-rolls the 16 trail jitters and the ring
///   angle, links the body, spawns the launch effect as a child task and
///   claims room-light slot 0.
/// - State 1 flies the round, draws the ring plus both trail halves, traces the
///   ground under it for a splash, and until frame 0x15 keeps spawning sparks.
///   It then re-aims the room light and asks `worldCollisionProbeGridSegment` whether the step
///   crossed geometry: a hit unlinks the body and switches to state 2, and
///   living past frame 0x15 unlinks it and releases the pool block. Otherwise
///   the body's occupied contacts are cleared unread, so the round is not
///   stopped by what it touches.
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
    _HypervelocityRoundBody*       roundBody;
    GfxRotationWords*              destinationRotation;
    GfxRotationWords*              sourceRotation;
    u32                            ang;
    s32                            i;

    roundBody = task->work;
    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[0];
    light     = &lightSlot->light.head.transform.coord;
    slot      = &lightSlot->light;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        work->age = work->age - 1;
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state != 0) {
                worldCollisionUnlinkBody(&roundBody->body);
            }
            effectKillTask(work, task);
        }
        return;
    }

    work->age = work->age + 1;
    switch (task->state) {
        case 0:
            roundBody = memCalloc(sizeof(_HypervelocityRoundBody), 0);
            if (roundBody == NULL) {
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
            actorRenderComposeCoord(coord);
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
            work->scale                      = 0xC0;
            work->angle                      = 0x500;
            gRandomLcgState                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period                     = (gRandomLcgState >> 16) & 0xFFF;
            task->work                       = roundBody;
            roundBody->body.context.contacts = roundBody->contacts;
            roundBody->body.radius           = 0x800;
            roundBody->body.coord            = coord;
            roundBody->body.key              = HYPERVELOCITY_ROUND_COLLISION_KEY;
            roundBody->body.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &roundBody->body);
            // The allocation already zeroed the entry; LAST terminates the table.
            roundBody->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
            roundBody->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            eff                          = Gp_SpawnEff(EFFECT_HYPERVELOCITY_SHOCK_RING, coord, 0, NULL);
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
            effectDrawGouraudDisc(coord, work->angle, rgb);
            return;
        case 1:
            actorRenderComposeCoord(coord);
            before.vx                  = coord->workm.t[0];
            before.vy                  = coord->workm.t[1];
            before.vz                  = coord->workm.t[2];
            coord->coord.t[0]         += work->move.vx;
            coord->coord.t[1]         += work->move.vy;
            coord->coord.t[2]         += work->move.vz;
            coord->composeStamp        = GRAPHICS_COORD_DIRTY;
            gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            rgb[0]   = work->scale >> 2;
            rgb[1]   = work->scale >> 2;
            rgb[2]   = work->scale >> 1;
            spriteQuadDraw(coord, work->age, work->angle, work->period);
            effectDrawGouraudDisc(coord, work->angle, rgb);
            jetConeDraw(coord, work->age, work->angle, 0);
            jetConeDraw(coord, work->age, work->angle, 1);
            if (gRoomEffectState->groundTraceEnabled != 0 && worldCollisionProjectGroundCoord(coord, &ground) == 1) {
                groundGlowDraw(&ground, work->angle);
            }
            if (work->age < 0x15) {
                Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x400, NULL);
                eff = Gp_SpawnEff(EFFECT_HYPERVELOCITY_DISCHARGE_CONE, coord, 0, NULL);
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
            if (worldCollisionProbeGridSegment(&after, &before, NULL, NULL) == 1) {
                worldCollisionUnlinkBody(&roundBody->body);
                task->state = 2;
                return;
            }
            if (work->age >= 0x15) {
                worldCollisionUnlinkBody(&roundBody->body);
                effectKillTask(work, task);
                return;
            }
            worldCollisionClearContacts(roundBody->contacts);
            return;
        case 2:
            actorRenderComposeCoord(coord);
            work->angle = work->angle - 0x40;
            rgb[0]      = work->scale >> 2;
            rgb[1]      = work->scale >> 2;
            rgb[2]      = work->scale >> 1;
            spriteQuadDraw(coord, work->age, work->angle, work->period);
            effectDrawGouraudDisc(coord, work->angle, rgb);
            if (work->angle < 0x80) {
                effectKillTask(work, task);
                return;
            }
            Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x400, NULL);
            return;
    }
}

#define JET_CONE_CLUT         0x42C1
#define JET_CONE_RIM_SHORT    0x600
#define JET_CONE_RIM_LONG     0x800
#define JET_CONE_FRAME_JITTER D_hypervelocity_8012EF0C
#include "../../shared/jet_cone_draw.inc.c"

/// Packed additive beam-flame page: 4-bit indexed texels at VRAM X=576 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 576, 0)
/// Beam-flame palette: VRAM X=176 words, Y=266 scanlines, the core look in `SPRITE_QUAD_CORE_CELL`.
#define SPRITE_QUAD_CLUT getClut(176, 266)
/// Texel width and horizontal stride of the two beam-flame cells selected by age parity.
#define SPRITE_QUAD_CELL_WIDTH 56
#define SPRITE_QUAD_CELL_MASK  1
/// First flame-cell U coordinate in texels relative to the selected texture page.
///
/// The effect age's low bit selects U = 0x70..0xA7 or 0xA8..0xDF.
/// This integer constant configures the next drawer inclusion, which undefines it.
#define SPRITE_QUAD_U_BASE 0x70
/// Inclusive top texel row of both beam-flame cells, relative to their texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0xC8
#define SPRITE_QUAD_V1    0xFF
/// Perspective-sizing multiplier for the hypervelocity flame sprite.
///
/// Uses the cell's inclusive 55-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

#define GROUND_GLOW_R    0x30
#define GROUND_GLOW_G    0x30
#define GROUND_GLOW_B    0x30
#define GROUND_GLOW_CLUT 0x428B
#include "../../shared/ground_glow_draw.inc.c"

/// Draws the discharge cone `func_hypervelocity_8011F270` leaves behind: two
/// opposed `POLY_FT4` walls flaring out of `coord`, built in the scratchpad as
/// a `_HypervelocityDischargeConeScratch`. The collar sits at y `0x700` and
/// is `radius` wide, the mouth rises to `0x600 - age * 256 / 2` and flares to
/// `radius + age * 128 + 0x200`, so the cone climbs and opens as the puff
/// ages; both squares are `radius` deep in z. Wall `i` is therefore the quad
/// of mouth vertices `i` and `i + 2` and collar vertices `i` and `i + 2` -
/// the -x pair, then the +x pair. Each wall is a frame of the same six-frame
/// strip at tpage 0x2A the trail uses, picked by the stored jitter
/// `D_hypervelocity_8012EF0C[i]` plus `age`, tinted by `rgb` and linked into
/// the OT bucket its own projected depth names. Walls the GTE flags as behind
/// the eye are dropped.
static void func_hypervelocity_8011EC1C(GfxCoord* coord, s16 age, s32 radius, u8* rgb)
{
    _HypervelocityDischargeConeScratch* sc;
    POLY_FT4*                           prim;
    EffectUnitQuadCorner*               corners;
    SVECTOR*                            collarVertex;
    MATRIX*                             rot;
    s32                                 i;
    s32                                 rise;
    s32                                 top;
    u16                                 flare;
    s32                                 half;
    s32                                 u0;

    /* `rise` is built in two steps and then walked in place, and `half` is a
       second spelling of `radius`, because the ROM keeps both copies the
       folded forms would have coalesced away. `flare` is 16-bit on purpose:
       it only ever feeds a halfword store, and widening it moves the whole
       prologue's register assignment. `collarVertex` is stepped from the
       mouth vertex of the same corner rather than indexed off `sc`, so the
       `gte_ldv0` / `gte_stsv` address stays a register of its own instead of
       being shared with the field stores. */
    sc    = SCRATCH_STACK_RESERVE_BLOCK(_HypervelocityDischargeConeScratch);
    rise  = age;
    rise  = rise << 7;
    top   = 0x600 - rise;
    rise  = rise + 0x200;
    flare = radius + rise;
    half  = radius;
    gte_SetTransMatrix(&GsWSMATRIX);
    i       = 0;
    rot     = &coord->workm;
    corners = D_80111E38;
    // Stage each corner's mouth and collar vertex and move both to world space.
    do {
        sc->vertices[i].vx = (u16)corners[i].axis0Sign * flare;
        sc->vertices[i].vy = top;
        sc->vertices[i].vz = (u16)corners[i].axis1Sign * half;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->vertices[i]);
        gte_rtv0();
        gte_stsv(&sc->vertices[i]);
        sc->vertices[i].vx += (u16)coord->workm.t[0];
        sc->vertices[i].vy += (u16)coord->workm.t[1];
        sc->vertices[i].vz += (u16)coord->workm.t[2];
        collarVertex        = &sc->vertices[i] + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT;
        collarVertex->vx    = (u16)corners[i].axis0Sign * radius;
        collarVertex->vy    = 0x700;
        collarVertex->vz    = (u16)corners[i].axis1Sign * half;
        gte_SetRotMatrix(rot);
        gte_ldv0(&sc->vertices[i + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT]);
        gte_rtv0();
        gte_stsv(&sc->vertices[i + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT]);
        collarVertex->vx += (u16)coord->workm.t[0];
        i++;
        collarVertex->vy += (u16)coord->workm.t[1];
        collarVertex->vz += (u16)coord->workm.t[2];
    } while (i < ARRAY_SIZE(D_80111E38));

    // Project and draw one wall at a time: the corners sharing a local X sign.
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        gte_ldv0(&sc->vertices[i]);
        gte_rtps();
        gte_stsxy(&sc->sxy0);
        gte_ldv3(&sc->vertices[i + 2],
                 &sc->vertices[i + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT],
                 &sc->vertices[i + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT + 2]);
        gte_rtpt();
        gte_stsxy3(&sc->sxy1, &sc->sxy2, &sc->sxy3);
        gte_stflg(&sc->projectionFlags);
        if (sc->projectionFlags >= 0) {
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
    } while (i < HYPERVELOCITY_DISCHARGE_CONE_WALL_COUNT);
    SCRATCH_STACK_RELEASE_BLOCK(_HypervelocityDischargeConeScratch);
}

/// Exit callback: unlinks the `_HypervelocityRoundBody` at `Task::work`, if
/// one was allocated, and releases the `EffectWork` in `Task::spawnArg2`.
/// `body` is the block's first member, so the pointer is that
/// `WorldCollisionBody`. M4A1 Pyke carries an identical copy.
static void func_hypervelocity_8011F11C(Task* task)
{
    WorldCollisionBody* obj = task->work;
    void*               mem = task->spawnArg2.pointer;

    if (obj != NULL) {
        worldCollisionUnlinkBody(obj);
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

    actorRenderComposeCoord(coord);
    mem->age++;
    if (arg0->state == 0) {
        mem->scale  = 0xF0;
        mem->angle  = 0x100;
        arg0->state = 1;
    }
    rgb[0] = mem->scale >> 1;
    rgb[1] = mem->scale >> 1;
    rgb[2] = mem->scale;
    effectDrawRaisedGlowBand(coord, mem->angle, rgb);
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

    actorRenderComposeCoord(coord);
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
                    sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_CANCEL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
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
/// 18 ticks the forward axis of the player model's root coordinate is scaled
/// by the remaining ticks over 378 (or 244 on the first tick) and subtracted
/// from the coordinate's translation, pushing the player straight back.
void func_hypervelocity_8011F724(Task* arg0)
{
    _HypervelocityRecoilScratch* scratch;
    GameActor*                   actor;
    GfxCoord*                    coord;
    Task*                        eff;
    s32                          div;
    s32                          count;
    s32                          step;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_HypervelocityRecoilScratch);
    actor   = arg0->work;
    eff     = actor->equipmentTasks[1];
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
            playerActorPlayChildSlotsWithBlend(arg0, 0xE, 0, 3);
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
                    sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160007, 1);
                    Gp_AnimResetChildSlots(arg0, 0xB);
                } else if (count == 0x3C) {
                    eff->spawnArg1.value |= 0x20;
                    sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_START, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160002, 0);
                }
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_CANCEL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            } else {
                actor->statePhase                        = 3;
                actor->weaponEffectTask->spawnArg1.value = -1;
                eff->spawnArg1.value                     = 0;
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_START, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20160004, 0);
                playerActorPlayChildSlotsWithBlend(arg0, 0xF, 0, 3);
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
                    gfxReadMatrixZAxis(&coord->coord, &scratch->forward);
                    scratch->recoil.vx = -(scratch->forward.vx * actor->stateTimer / div);
                    scratch->recoil.vy = -(scratch->forward.vy * actor->stateTimer / div);
                    scratch->recoil.vz = -(scratch->forward.vz * actor->stateTimer / div);
                    coord->coord.t[0] += scratch->recoil.vx;
                    coord->coord.t[1] += scratch->recoil.vy;
                    coord->coord.t[2] += scratch->recoil.vz;
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
    SCRATCH_STACK_RELEASE_BLOCK(_HypervelocityRecoilScratch);
}

static TmdBone _gHypervelocityModel02C9CSkeleton[1] = {
#include "assets/hypervelocity_model_02C9C_skeleton.inc"
};

static u32 _gHypervelocityModel02C9CPartVerts[1] = {
#include "assets/hypervelocity_model_02C9C_partVerts.inc"
};

static SVECTOR _gHypervelocityModel02C9CVerts[50] = {
#include "assets/hypervelocity_model_02C9C_verts.inc"
};

static SVECTOR _gHypervelocityModel02C9CNormals[37] = {
#include "assets/hypervelocity_model_02C9C_normals.inc"
};

static u32 _gHypervelocityModel02C9CStream[295] = {
#include "assets/hypervelocity_model_02C9C_stream.inc"
};

TmdSource D_hypervelocity_801202F8 = {
    0,
    2096,
    0,
    1,
    _gHypervelocityModel02C9CPartVerts,
    _gHypervelocityModel02C9CVerts,
    _gHypervelocityModel02C9CNormals,
    _gHypervelocityModel02C9CSkeleton,
    _gHypervelocityModel02C9CStream,
};

static TmdBone _gHypervelocityModel03284Skeleton[1] = {
#include "assets/hypervelocity_model_03284_skeleton.inc"
};

static u32 _gHypervelocityModel03284PartVerts[1] = {
#include "assets/hypervelocity_model_03284_partVerts.inc"
};

static SVECTOR _gHypervelocityModel03284Verts[16] = {
#include "assets/hypervelocity_model_03284_verts.inc"
};

static SVECTOR _gHypervelocityModel03284Normals[16] = {
#include "assets/hypervelocity_model_03284_normals.inc"
};

static u32 _gHypervelocityModel03284Stream[104] = {
#include "assets/hypervelocity_model_03284_stream.inc"
};

TmdSource D_hypervelocity_801205E4 = {
    0,
    728,
    0,
    1,
    _gHypervelocityModel03284PartVerts,
    _gHypervelocityModel03284Verts,
    _gHypervelocityModel03284Normals,
    _gHypervelocityModel03284Skeleton,
    _gHypervelocityModel03284Stream,
};

static TmdBone _gHypervelocityModel03550Skeleton[1] = {
#include "assets/hypervelocity_model_03550_skeleton.inc"
};

static u32 _gHypervelocityModel03550PartVerts[1] = {
#include "assets/hypervelocity_model_03550_partVerts.inc"
};

static SVECTOR _gHypervelocityModel03550Verts[12] = {
#include "assets/hypervelocity_model_03550_verts.inc"
};

static SVECTOR _gHypervelocityModel03550Normals[16] = {
#include "assets/hypervelocity_model_03550_normals.inc"
};

static u32 _gHypervelocityModel03550Stream[84] = {
#include "assets/hypervelocity_model_03550_stream.inc"
};

TmdSource D_hypervelocity_80120860 = {
    0,
    560,
    0,
    1,
    _gHypervelocityModel03550PartVerts,
    _gHypervelocityModel03550Verts,
    _gHypervelocityModel03550Normals,
    _gHypervelocityModel03550Skeleton,
    _gHypervelocityModel03550Stream,
};

static AnimationPackedPose _gHypervelocityAnimation03854Bank1[2] = {
#include "assets/hypervelocity_animation_03854_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation03854Bank4[8] = {
#include "assets/hypervelocity_animation_03854_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation03854Records[76] = {
#include "assets/hypervelocity_animation_03854_records.inc"
};

static u16 _gHypervelocityAnimation03854Indices[20] = {
#include "assets/hypervelocity_animation_03854_indices.inc"
};

static AnimationSet _gHypervelocityAnimation03854 = {
    _gHypervelocityAnimation03854Records,
    _gHypervelocityAnimation03854Indices,
    { NULL, _gHypervelocityAnimation03854Bank1, NULL, NULL, _gHypervelocityAnimation03854Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation03EF8Bank1[12] = {
#include "assets/hypervelocity_animation_03EF8_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation03EF8Bank4[151] = {
#include "assets/hypervelocity_animation_03EF8_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation03EF8Records[218] = {
#include "assets/hypervelocity_animation_03EF8_records.inc"
};

static u16 _gHypervelocityAnimation03EF8Indices[20] = {
#include "assets/hypervelocity_animation_03EF8_indices.inc"
};

static AnimationSet _gHypervelocityAnimation03EF8 = {
    _gHypervelocityAnimation03EF8Records,
    _gHypervelocityAnimation03EF8Indices,
    { NULL, _gHypervelocityAnimation03EF8Bank1, NULL, NULL, _gHypervelocityAnimation03EF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation047D0Bank1[19] = {
#include "assets/hypervelocity_animation_047D0_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation047D0Bank4[193] = {
#include "assets/hypervelocity_animation_047D0_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation047D0Records[296] = {
#include "assets/hypervelocity_animation_047D0_records.inc"
};

static u16 _gHypervelocityAnimation047D0Indices[20] = {
#include "assets/hypervelocity_animation_047D0_indices.inc"
};

static AnimationSet _gHypervelocityAnimation047D0 = {
    _gHypervelocityAnimation047D0Records,
    _gHypervelocityAnimation047D0Indices,
    { NULL, _gHypervelocityAnimation047D0Bank1, NULL, NULL, _gHypervelocityAnimation047D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation05030Bank1[19] = {
#include "assets/hypervelocity_animation_05030_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation05030Bank4[169] = {
#include "assets/hypervelocity_animation_05030_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation05030Records[290] = {
#include "assets/hypervelocity_animation_05030_records.inc"
};

static u16 _gHypervelocityAnimation05030Indices[20] = {
#include "assets/hypervelocity_animation_05030_indices.inc"
};

static AnimationSet _gHypervelocityAnimation05030 = {
    _gHypervelocityAnimation05030Records,
    _gHypervelocityAnimation05030Indices,
    { NULL, _gHypervelocityAnimation05030Bank1, NULL, NULL, _gHypervelocityAnimation05030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation05894Bank1[19] = {
#include "assets/hypervelocity_animation_05894_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation05894Bank4[170] = {
#include "assets/hypervelocity_animation_05894_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation05894Records[290] = {
#include "assets/hypervelocity_animation_05894_records.inc"
};

static u16 _gHypervelocityAnimation05894Indices[20] = {
#include "assets/hypervelocity_animation_05894_indices.inc"
};

static AnimationSet _gHypervelocityAnimation05894 = {
    _gHypervelocityAnimation05894Records,
    _gHypervelocityAnimation05894Indices,
    { NULL, _gHypervelocityAnimation05894Bank1, NULL, NULL, _gHypervelocityAnimation05894Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation05BA8Bank1[3] = {
#include "assets/hypervelocity_animation_05BA8_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation05BA8Bank4[69] = {
#include "assets/hypervelocity_animation_05BA8_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation05BA8Records[99] = {
#include "assets/hypervelocity_animation_05BA8_records.inc"
};

static u16 _gHypervelocityAnimation05BA8Indices[20] = {
#include "assets/hypervelocity_animation_05BA8_indices.inc"
};

static AnimationSet _gHypervelocityAnimation05BA8 = {
    _gHypervelocityAnimation05BA8Records,
    _gHypervelocityAnimation05BA8Indices,
    { NULL, _gHypervelocityAnimation05BA8Bank1, NULL, NULL, _gHypervelocityAnimation05BA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation06304Bank1[14] = {
#include "assets/hypervelocity_animation_06304_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation06304Bank4[156] = {
#include "assets/hypervelocity_animation_06304_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation06304Records[253] = {
#include "assets/hypervelocity_animation_06304_records.inc"
};

static u16 _gHypervelocityAnimation06304Indices[20] = {
#include "assets/hypervelocity_animation_06304_indices.inc"
};

static AnimationSet _gHypervelocityAnimation06304 = {
    _gHypervelocityAnimation06304Records,
    _gHypervelocityAnimation06304Indices,
    { NULL, _gHypervelocityAnimation06304Bank1, NULL, NULL, _gHypervelocityAnimation06304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation06A8CBank1[16] = {
#include "assets/hypervelocity_animation_06A8C_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation06A8CBank4[167] = {
#include "assets/hypervelocity_animation_06A8C_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation06A8CRecords[247] = {
#include "assets/hypervelocity_animation_06A8C_records.inc"
};

static u16 _gHypervelocityAnimation06A8CIndices[20] = {
#include "assets/hypervelocity_animation_06A8C_indices.inc"
};

static AnimationSet _gHypervelocityAnimation06A8C = {
    _gHypervelocityAnimation06A8CRecords,
    _gHypervelocityAnimation06A8CIndices,
    { NULL, _gHypervelocityAnimation06A8CBank1, NULL, NULL, _gHypervelocityAnimation06A8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation06D60Bank1[6] = {
#include "assets/hypervelocity_animation_06D60_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation06D60Bank4[52] = {
#include "assets/hypervelocity_animation_06D60_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation06D60Records[91] = {
#include "assets/hypervelocity_animation_06D60_records.inc"
};

static u16 _gHypervelocityAnimation06D60Indices[20] = {
#include "assets/hypervelocity_animation_06D60_indices.inc"
};

static AnimationSet _gHypervelocityAnimation06D60 = {
    _gHypervelocityAnimation06D60Records,
    _gHypervelocityAnimation06D60Indices,
    { NULL, _gHypervelocityAnimation06D60Bank1, NULL, NULL, _gHypervelocityAnimation06D60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation070F0Bank1[7] = {
#include "assets/hypervelocity_animation_070F0_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation070F0Bank4[73] = {
#include "assets/hypervelocity_animation_070F0_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation070F0Records[114] = {
#include "assets/hypervelocity_animation_070F0_records.inc"
};

static u16 _gHypervelocityAnimation070F0Indices[20] = {
#include "assets/hypervelocity_animation_070F0_indices.inc"
};

static AnimationSet _gHypervelocityAnimation070F0 = {
    _gHypervelocityAnimation070F0Records,
    _gHypervelocityAnimation070F0Indices,
    { NULL, _gHypervelocityAnimation070F0Bank1, NULL, NULL, _gHypervelocityAnimation070F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation07578Bank1[9] = {
#include "assets/hypervelocity_animation_07578_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation07578Bank4[104] = {
#include "assets/hypervelocity_animation_07578_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation07578Records[139] = {
#include "assets/hypervelocity_animation_07578_records.inc"
};

static u16 _gHypervelocityAnimation07578Indices[20] = {
#include "assets/hypervelocity_animation_07578_indices.inc"
};

static AnimationSet _gHypervelocityAnimation07578 = {
    _gHypervelocityAnimation07578Records,
    _gHypervelocityAnimation07578Indices,
    { NULL, _gHypervelocityAnimation07578Bank1, NULL, NULL, _gHypervelocityAnimation07578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation07774Bank1[3] = {
#include "assets/hypervelocity_animation_07774_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation07774Bank4[22] = {
#include "assets/hypervelocity_animation_07774_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation07774Records[76] = {
#include "assets/hypervelocity_animation_07774_records.inc"
};

static u16 _gHypervelocityAnimation07774Indices[20] = {
#include "assets/hypervelocity_animation_07774_indices.inc"
};

static AnimationSet _gHypervelocityAnimation07774 = {
    _gHypervelocityAnimation07774Records,
    _gHypervelocityAnimation07774Indices,
    { NULL, _gHypervelocityAnimation07774Bank1, NULL, NULL, _gHypervelocityAnimation07774Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation07A4CBank1[6] = {
#include "assets/hypervelocity_animation_07A4C_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation07A4CBank4[57] = {
#include "assets/hypervelocity_animation_07A4C_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation07A4CRecords[87] = {
#include "assets/hypervelocity_animation_07A4C_records.inc"
};

static u16 _gHypervelocityAnimation07A4CIndices[20] = {
#include "assets/hypervelocity_animation_07A4C_indices.inc"
};

static AnimationSet _gHypervelocityAnimation07A4C = {
    _gHypervelocityAnimation07A4CRecords,
    _gHypervelocityAnimation07A4CIndices,
    { NULL, _gHypervelocityAnimation07A4CBank1, NULL, NULL, _gHypervelocityAnimation07A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation07CF0Bank1[4] = {
#include "assets/hypervelocity_animation_07CF0_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation07CF0Bank4[55] = {
#include "assets/hypervelocity_animation_07CF0_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation07CF0Records[82] = {
#include "assets/hypervelocity_animation_07CF0_records.inc"
};

static u16 _gHypervelocityAnimation07CF0Indices[20] = {
#include "assets/hypervelocity_animation_07CF0_indices.inc"
};

static AnimationSet _gHypervelocityAnimation07CF0 = {
    _gHypervelocityAnimation07CF0Records,
    _gHypervelocityAnimation07CF0Indices,
    { NULL, _gHypervelocityAnimation07CF0Bank1, NULL, NULL, _gHypervelocityAnimation07CF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation07EF0Bank1[3] = {
#include "assets/hypervelocity_animation_07EF0_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation07EF0Bank4[23] = {
#include "assets/hypervelocity_animation_07EF0_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation07EF0Records[76] = {
#include "assets/hypervelocity_animation_07EF0_records.inc"
};

static u16 _gHypervelocityAnimation07EF0Indices[20] = {
#include "assets/hypervelocity_animation_07EF0_indices.inc"
};

static AnimationSet _gHypervelocityAnimation07EF0 = {
    _gHypervelocityAnimation07EF0Records,
    _gHypervelocityAnimation07EF0Indices,
    { NULL, _gHypervelocityAnimation07EF0Bank1, NULL, NULL, _gHypervelocityAnimation07EF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation08244Bank1[8] = {
#include "assets/hypervelocity_animation_08244_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation08244Bank4[68] = {
#include "assets/hypervelocity_animation_08244_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation08244Records[101] = {
#include "assets/hypervelocity_animation_08244_records.inc"
};

static u16 _gHypervelocityAnimation08244Indices[20] = {
#include "assets/hypervelocity_animation_08244_indices.inc"
};

static AnimationSet _gHypervelocityAnimation08244 = {
    _gHypervelocityAnimation08244Records,
    _gHypervelocityAnimation08244Indices,
    { NULL, _gHypervelocityAnimation08244Bank1, NULL, NULL, _gHypervelocityAnimation08244Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation084F8Bank1[5] = {
#include "assets/hypervelocity_animation_084F8_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation084F8Bank4[55] = {
#include "assets/hypervelocity_animation_084F8_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation084F8Records[83] = {
#include "assets/hypervelocity_animation_084F8_records.inc"
};

static u16 _gHypervelocityAnimation084F8Indices[20] = {
#include "assets/hypervelocity_animation_084F8_indices.inc"
};

static AnimationSet _gHypervelocityAnimation084F8 = {
    _gHypervelocityAnimation084F8Records,
    _gHypervelocityAnimation084F8Indices,
    { NULL, _gHypervelocityAnimation084F8Bank1, NULL, NULL, _gHypervelocityAnimation084F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation08818Bank1[6] = {
#include "assets/hypervelocity_animation_08818_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation08818Bank4[66] = {
#include "assets/hypervelocity_animation_08818_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation08818Records[96] = {
#include "assets/hypervelocity_animation_08818_records.inc"
};

static u16 _gHypervelocityAnimation08818Indices[20] = {
#include "assets/hypervelocity_animation_08818_indices.inc"
};

static AnimationSet _gHypervelocityAnimation08818 = {
    _gHypervelocityAnimation08818Records,
    _gHypervelocityAnimation08818Indices,
    { NULL, _gHypervelocityAnimation08818Bank1, NULL, NULL, _gHypervelocityAnimation08818Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation08FDCBank1[18] = {
#include "assets/hypervelocity_animation_08FDC_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation08FDCBank4[184] = {
#include "assets/hypervelocity_animation_08FDC_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation08FDCRecords[239] = {
#include "assets/hypervelocity_animation_08FDC_records.inc"
};

static u16 _gHypervelocityAnimation08FDCIndices[20] = {
#include "assets/hypervelocity_animation_08FDC_indices.inc"
};

static AnimationSet _gHypervelocityAnimation08FDC = {
    _gHypervelocityAnimation08FDCRecords,
    _gHypervelocityAnimation08FDCIndices,
    { NULL, _gHypervelocityAnimation08FDCBank1, NULL, NULL, _gHypervelocityAnimation08FDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0A274Bank1[29] = {
#include "assets/hypervelocity_animation_0A274_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0A274Bank4[450] = {
#include "assets/hypervelocity_animation_0A274_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0A274Records[633] = {
#include "assets/hypervelocity_animation_0A274_records.inc"
};

static u16 _gHypervelocityAnimation0A274Indices[20] = {
#include "assets/hypervelocity_animation_0A274_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0A274 = {
    _gHypervelocityAnimation0A274Records,
    _gHypervelocityAnimation0A274Indices,
    { NULL, _gHypervelocityAnimation0A274Bank1, NULL, NULL, _gHypervelocityAnimation0A274Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0ADECBank1[12] = {
#include "assets/hypervelocity_animation_0ADEC_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0ADECBank4[266] = {
#include "assets/hypervelocity_animation_0ADEC_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0ADECRecords[412] = {
#include "assets/hypervelocity_animation_0ADEC_records.inc"
};

static u16 _gHypervelocityAnimation0ADECIndices[20] = {
#include "assets/hypervelocity_animation_0ADEC_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0ADEC = {
    _gHypervelocityAnimation0ADECRecords,
    _gHypervelocityAnimation0ADECIndices,
    { NULL, _gHypervelocityAnimation0ADECBank1, NULL, NULL, _gHypervelocityAnimation0ADECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0B508Bank1[9] = {
#include "assets/hypervelocity_animation_0B508_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0B508Bank4[144] = {
#include "assets/hypervelocity_animation_0B508_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0B508Records[264] = {
#include "assets/hypervelocity_animation_0B508_records.inc"
};

static u16 _gHypervelocityAnimation0B508Indices[20] = {
#include "assets/hypervelocity_animation_0B508_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0B508 = {
    _gHypervelocityAnimation0B508Records,
    _gHypervelocityAnimation0B508Indices,
    { NULL, _gHypervelocityAnimation0B508Bank1, NULL, NULL, _gHypervelocityAnimation0B508Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0B988Bank1[6] = {
#include "assets/hypervelocity_animation_0B988_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0B988Bank4[107] = {
#include "assets/hypervelocity_animation_0B988_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0B988Records[143] = {
#include "assets/hypervelocity_animation_0B988_records.inc"
};

static u16 _gHypervelocityAnimation0B988Indices[20] = {
#include "assets/hypervelocity_animation_0B988_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0B988 = {
    _gHypervelocityAnimation0B988Records,
    _gHypervelocityAnimation0B988Indices,
    { NULL, _gHypervelocityAnimation0B988Bank1, NULL, NULL, _gHypervelocityAnimation0B988Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0BB60Bank1[3] = {
#include "assets/hypervelocity_animation_0BB60_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0BB60Bank4[32] = {
#include "assets/hypervelocity_animation_0BB60_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0BB60Records[57] = {
#include "assets/hypervelocity_animation_0BB60_records.inc"
};

static u16 _gHypervelocityAnimation0BB60Indices[20] = {
#include "assets/hypervelocity_animation_0BB60_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0BB60 = {
    _gHypervelocityAnimation0BB60Records,
    _gHypervelocityAnimation0BB60Indices,
    { NULL, _gHypervelocityAnimation0BB60Bank1, NULL, NULL, _gHypervelocityAnimation0BB60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0C0C4Bank1[11] = {
#include "assets/hypervelocity_animation_0C0C4_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0C0C4Bank4[125] = {
#include "assets/hypervelocity_animation_0C0C4_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0C0C4Records[167] = {
#include "assets/hypervelocity_animation_0C0C4_records.inc"
};

static u16 _gHypervelocityAnimation0C0C4Indices[20] = {
#include "assets/hypervelocity_animation_0C0C4_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0C0C4 = {
    _gHypervelocityAnimation0C0C4Records,
    _gHypervelocityAnimation0C0C4Indices,
    { NULL, _gHypervelocityAnimation0C0C4Bank1, NULL, NULL, _gHypervelocityAnimation0C0C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0C2B8Bank1[3] = {
#include "assets/hypervelocity_animation_0C2B8_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0C2B8Bank4[20] = {
#include "assets/hypervelocity_animation_0C2B8_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0C2B8Records[76] = {
#include "assets/hypervelocity_animation_0C2B8_records.inc"
};

static u16 _gHypervelocityAnimation0C2B8Indices[20] = {
#include "assets/hypervelocity_animation_0C2B8_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0C2B8 = {
    _gHypervelocityAnimation0C2B8Records,
    _gHypervelocityAnimation0C2B8Indices,
    { NULL, _gHypervelocityAnimation0C2B8Bank1, NULL, NULL, _gHypervelocityAnimation0C2B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0C738Bank1[8] = {
#include "assets/hypervelocity_animation_0C738_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0C738Bank4[105] = {
#include "assets/hypervelocity_animation_0C738_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0C738Records[139] = {
#include "assets/hypervelocity_animation_0C738_records.inc"
};

static u16 _gHypervelocityAnimation0C738Indices[20] = {
#include "assets/hypervelocity_animation_0C738_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0C738 = {
    _gHypervelocityAnimation0C738Records,
    _gHypervelocityAnimation0C738Indices,
    { NULL, _gHypervelocityAnimation0C738Bank1, NULL, NULL, _gHypervelocityAnimation0C738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0C914Bank1[2] = {
#include "assets/hypervelocity_animation_0C914_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0C914Bank4[17] = {
#include "assets/hypervelocity_animation_0C914_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0C914Records[76] = {
#include "assets/hypervelocity_animation_0C914_records.inc"
};

static u16 _gHypervelocityAnimation0C914Indices[20] = {
#include "assets/hypervelocity_animation_0C914_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0C914 = {
    _gHypervelocityAnimation0C914Records,
    _gHypervelocityAnimation0C914Indices,
    { NULL, _gHypervelocityAnimation0C914Bank1, NULL, NULL, _gHypervelocityAnimation0C914Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0CDB4Bank1[11] = {
#include "assets/hypervelocity_animation_0CDB4_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0CDB4Bank4[84] = {
#include "assets/hypervelocity_animation_0CDB4_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0CDB4Records[159] = {
#include "assets/hypervelocity_animation_0CDB4_records.inc"
};

static u16 _gHypervelocityAnimation0CDB4Indices[20] = {
#include "assets/hypervelocity_animation_0CDB4_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0CDB4 = {
    _gHypervelocityAnimation0CDB4Records,
    _gHypervelocityAnimation0CDB4Indices,
    { NULL, _gHypervelocityAnimation0CDB4Bank1, NULL, NULL, _gHypervelocityAnimation0CDB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0D5C8Bank1[15] = {
#include "assets/hypervelocity_animation_0D5C8_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0D5C8Bank4[195] = {
#include "assets/hypervelocity_animation_0D5C8_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0D5C8Records[257] = {
#include "assets/hypervelocity_animation_0D5C8_records.inc"
};

static u16 _gHypervelocityAnimation0D5C8Indices[20] = {
#include "assets/hypervelocity_animation_0D5C8_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0D5C8 = {
    _gHypervelocityAnimation0D5C8Records,
    _gHypervelocityAnimation0D5C8Indices,
    { NULL, _gHypervelocityAnimation0D5C8Bank1, NULL, NULL, _gHypervelocityAnimation0D5C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0DB10Bank1[11] = {
#include "assets/hypervelocity_animation_0DB10_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0DB10Bank4[124] = {
#include "assets/hypervelocity_animation_0DB10_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0DB10Records[161] = {
#include "assets/hypervelocity_animation_0DB10_records.inc"
};

static u16 _gHypervelocityAnimation0DB10Indices[20] = {
#include "assets/hypervelocity_animation_0DB10_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0DB10 = {
    _gHypervelocityAnimation0DB10Records,
    _gHypervelocityAnimation0DB10Indices,
    { NULL, _gHypervelocityAnimation0DB10Bank1, NULL, NULL, _gHypervelocityAnimation0DB10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0E2F0Bank1[17] = {
#include "assets/hypervelocity_animation_0E2F0_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0E2F0Bank4[192] = {
#include "assets/hypervelocity_animation_0E2F0_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0E2F0Records[241] = {
#include "assets/hypervelocity_animation_0E2F0_records.inc"
};

static u16 _gHypervelocityAnimation0E2F0Indices[20] = {
#include "assets/hypervelocity_animation_0E2F0_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0E2F0 = {
    _gHypervelocityAnimation0E2F0Records,
    _gHypervelocityAnimation0E2F0Indices,
    { NULL, _gHypervelocityAnimation0E2F0Bank1, NULL, NULL, _gHypervelocityAnimation0E2F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0E838Bank1[12] = {
#include "assets/hypervelocity_animation_0E838_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0E838Bank4[119] = {
#include "assets/hypervelocity_animation_0E838_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0E838Records[163] = {
#include "assets/hypervelocity_animation_0E838_records.inc"
};

static u16 _gHypervelocityAnimation0E838Indices[20] = {
#include "assets/hypervelocity_animation_0E838_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0E838 = {
    _gHypervelocityAnimation0E838Records,
    _gHypervelocityAnimation0E838Indices,
    { NULL, _gHypervelocityAnimation0E838Bank1, NULL, NULL, _gHypervelocityAnimation0E838Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0ED7CBank1[12] = {
#include "assets/hypervelocity_animation_0ED7C_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0ED7CBank4[118] = {
#include "assets/hypervelocity_animation_0ED7C_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0ED7CRecords[163] = {
#include "assets/hypervelocity_animation_0ED7C_records.inc"
};

static u16 _gHypervelocityAnimation0ED7CIndices[20] = {
#include "assets/hypervelocity_animation_0ED7C_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0ED7C = {
    _gHypervelocityAnimation0ED7CRecords,
    _gHypervelocityAnimation0ED7CIndices,
    { NULL, _gHypervelocityAnimation0ED7CBank1, NULL, NULL, _gHypervelocityAnimation0ED7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation0F964Bank1[23] = {
#include "assets/hypervelocity_animation_0F964_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation0F964Bank4[305] = {
#include "assets/hypervelocity_animation_0F964_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation0F964Records[368] = {
#include "assets/hypervelocity_animation_0F964_records.inc"
};

static u16 _gHypervelocityAnimation0F964Indices[20] = {
#include "assets/hypervelocity_animation_0F964_indices.inc"
};

static AnimationSet _gHypervelocityAnimation0F964 = {
    _gHypervelocityAnimation0F964Records,
    _gHypervelocityAnimation0F964Indices,
    { NULL, _gHypervelocityAnimation0F964Bank1, NULL, NULL, _gHypervelocityAnimation0F964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation10198Bank1[13] = {
#include "assets/hypervelocity_animation_10198_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation10198Bank4[207] = {
#include "assets/hypervelocity_animation_10198_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation10198Records[259] = {
#include "assets/hypervelocity_animation_10198_records.inc"
};

static u16 _gHypervelocityAnimation10198Indices[20] = {
#include "assets/hypervelocity_animation_10198_indices.inc"
};

static AnimationSet _gHypervelocityAnimation10198 = {
    _gHypervelocityAnimation10198Records,
    _gHypervelocityAnimation10198Indices,
    { NULL, _gHypervelocityAnimation10198Bank1, NULL, NULL, _gHypervelocityAnimation10198Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation10A50Bank1[18] = {
#include "assets/hypervelocity_animation_10A50_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation10A50Bank4[201] = {
#include "assets/hypervelocity_animation_10A50_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation10A50Records[283] = {
#include "assets/hypervelocity_animation_10A50_records.inc"
};

static u16 _gHypervelocityAnimation10A50Indices[20] = {
#include "assets/hypervelocity_animation_10A50_indices.inc"
};

static AnimationSet _gHypervelocityAnimation10A50 = {
    _gHypervelocityAnimation10A50Records,
    _gHypervelocityAnimation10A50Indices,
    { NULL, _gHypervelocityAnimation10A50Bank1, NULL, NULL, _gHypervelocityAnimation10A50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation111F0Bank1[16] = {
#include "assets/hypervelocity_animation_111F0_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation111F0Bank4[157] = {
#include "assets/hypervelocity_animation_111F0_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation111F0Records[263] = {
#include "assets/hypervelocity_animation_111F0_records.inc"
};

static u16 _gHypervelocityAnimation111F0Indices[20] = {
#include "assets/hypervelocity_animation_111F0_indices.inc"
};

static AnimationSet _gHypervelocityAnimation111F0 = {
    _gHypervelocityAnimation111F0Records,
    _gHypervelocityAnimation111F0Indices,
    { NULL, _gHypervelocityAnimation111F0Bank1, NULL, NULL, _gHypervelocityAnimation111F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gHypervelocityAnimation11BE8Bank1[19] = {
#include "assets/hypervelocity_animation_11BE8_bank1.inc"
};

static AnimationPackedRotation _gHypervelocityAnimation11BE8Bank4[241] = {
#include "assets/hypervelocity_animation_11BE8_bank4.inc"
};

static AnimationRecord _gHypervelocityAnimation11BE8Records[320] = {
#include "assets/hypervelocity_animation_11BE8_records.inc"
};

static u16 _gHypervelocityAnimation11BE8Indices[20] = {
#include "assets/hypervelocity_animation_11BE8_indices.inc"
};

static AnimationSet _gHypervelocityAnimation11BE8 = {
    _gHypervelocityAnimation11BE8Records,
    _gHypervelocityAnimation11BE8Indices,
    { NULL, _gHypervelocityAnimation11BE8Bank1, NULL, NULL, _gHypervelocityAnimation11BE8Bank4, NULL, NULL, NULL },
};

AnimationBank D_hypervelocity_8012EDD0 = { { {
    NULL,
    &_gHypervelocityAnimation03854,
    &_gHypervelocityAnimation10A50,
    &_gHypervelocityAnimation111F0,
    &_gHypervelocityAnimation11BE8,
    &_gHypervelocityAnimation05030,
    &_gHypervelocityAnimation05894,
    &_gHypervelocityAnimation0F964,
    &_gHypervelocityAnimation10198,
    &_gHypervelocityAnimation0C914,
    &_gHypervelocityAnimation0E2F0,
    &_gHypervelocityAnimation0E2F0,
    &_gHypervelocityAnimation0D5C8,
    &_gHypervelocityAnimation0CDB4,
    &_gHypervelocityAnimation0E838,
    &_gHypervelocityAnimation0ED7C,
    &_gHypervelocityAnimation084F8,
    &_gHypervelocityAnimation08818,
    &_gHypervelocityAnimation08FDC,
    &_gHypervelocityAnimation03EF8,
    &_gHypervelocityAnimation0DB10,
    &_gHypervelocityAnimation03854,
    &_gHypervelocityAnimation03854,
    &_gHypervelocityAnimation0A274,
    &_gHypervelocityAnimation0B508,
    &_gHypervelocityAnimation0ADEC,
    &_gHypervelocityAnimation07578,
    &_gHypervelocityAnimation07774,
    &_gHypervelocityAnimation07A4C,
    &_gHypervelocityAnimation07CF0,
    &_gHypervelocityAnimation07EF0,
    &_gHypervelocityAnimation08244,
    &_gHypervelocityAnimation0B988,
    &_gHypervelocityAnimation0BB60,
    &_gHypervelocityAnimation0B988,
    &_gHypervelocityAnimation0BB60,
    &_gHypervelocityAnimation06304,
    &_gHypervelocityAnimation06A8C,
    &_gHypervelocityAnimation070F0,
    &_gHypervelocityAnimation06D60,
    &_gHypervelocityAnimation05BA8,
    &_gHypervelocityAnimation03854,
    &_gHypervelocityAnimation0C0C4,
    &_gHypervelocityAnimation0C2B8,
    &_gHypervelocityAnimation0C738,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

s16 D_hypervelocity_8012EF0C[16] = { 0 };
