#include "weapons/hypervelocity.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

static s16 _gHypervelocityConeTextureFrameOffsets[16];

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
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"

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
STATIC_ASSERT(ARRAY_SIZE(gEffectUnitQuadCorners) == HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT, hypervelocity_discharge_cone_square_vertex_count);

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

/// Charge-effect muzzle position in the weapon root's local game coordinates.
///
/// The persistent effect uses this translation for its glow, point light and
/// round launch. The fourth halfword is unused by coordinate placement.
static SVECTOR _gHypervelocityMuzzleOffset = { 0, 0x240, 0x80, 0 };

/// Low-nibble model component selector and charge-driven motions in spawnArg1.
enum {
    HYPERVELOCITY_MODEL_COMPONENT_MASK  = 0xF,
    HYPERVELOCITY_MODEL_ROOT            = 0,
    HYPERVELOCITY_MODEL_SLIDE           = 1,
    HYPERVELOCITY_MODEL_HINGE           = 2,
    HYPERVELOCITY_MODEL_CHARGE_SLIDE    = 0x10,
    HYPERVELOCITY_MODEL_CHARGE_OPEN     = 0x20,
    HYPERVELOCITY_CHARGE_REQUEST_CANCEL = -1,
    HYPERVELOCITY_CHARGE_REQUEST_START  = 1,
};

static void _hypervelocityReleaseRound(Task* task);
static void _hypervelocityReleaseWeapon(Task* task);

static void _hypervelocityDrawDischargeCone(const GfxCoord* coord, s16 ageFrames, s32 halfExtent, const u8* rgb);
static void _hypervelocityUpdateModelPose(Task* task);
static void _hypervelocityInitWeaponModel(Task* task);
static void _hypervelocityQueueWeaponTeardown(Task* task);

void hypervelocityChargeEffectTask(Task* task)
{
    enum {
        HYPERVELOCITY_CHARGE_EFFECT_INIT                  = 0,
        HYPERVELOCITY_CHARGE_EFFECT_IDLE                  = 1,
        HYPERVELOCITY_CHARGE_EFFECT_CHARGE                = 2,
        HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE             = 3,
        HYPERVELOCITY_CHARGE_EFFECT_COOL                  = 4,
        HYPERVELOCITY_CHARGE_EFFECT_TIMEOUT_FRAMES        = 65,
        HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_FRAMES      = 24,
        HYPERVELOCITY_CHARGE_EFFECT_RETIRE_AGE            = 111,
        HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_UNIT       = 256,
        HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_MAX        = 255,
        HYPERVELOCITY_CHARGE_EFFECT_ARC_START_BRIGHTNESS  = 129,
        HYPERVELOCITY_CHARGE_EFFECT_RADIUS_START          = 64,
        HYPERVELOCITY_CHARGE_EFFECT_RADIUS_MAX            = 512,
        HYPERVELOCITY_CHARGE_EFFECT_RADIUS_STEP           = 8,
        HYPERVELOCITY_CHARGE_EFFECT_FADE_STEP             = 32,
        HYPERVELOCITY_CHARGE_EFFECT_PARTICLE_SIZE         = 384,
        HYPERVELOCITY_CHARGE_EFFECT_LIGHT_FRAMES          = 4,
        HYPERVELOCITY_CHARGE_EFFECT_LIGHT_RANDOM_MASK     = 0x700,
        HYPERVELOCITY_CHARGE_EFFECT_CHARGING_BLUE_MIN     = 1024,
        HYPERVELOCITY_CHARGE_EFFECT_DISCHARGING_BLUE_MIN  = 2048,
        HYPERVELOCITY_CHARGE_EFFECT_LIGHT_INNER           = 256,
        HYPERVELOCITY_CHARGE_EFFECT_LIGHT_OUTER           = 4096,
        HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_LIGHT_INNER = 1024,
        HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_LIGHT_OUTER = 16384,
        HYPERVELOCITY_CHARGE_EFFECT_DUST_ARGUMENT         = 0x2300,
        HYPERVELOCITY_CHARGE_EFFECT_FIRST_HAND            = 15,
        HYPERVELOCITY_CHARGE_EFFECT_HAND_STRIDE           = 3,
        HYPERVELOCITY_CHARGE_EFFECT_DRIFT_AGE_MASK        = 15,
        HYPERVELOCITY_CHARGE_EFFECT_CHARGE_DRIFT_SHIFT    = 5,
        HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_DRIFT_SHIFT = 6,
        HYPERVELOCITY_CHARGE_EFFECT_ARC_RADIUS            = 96,
        HYPERVELOCITY_CHARGE_EFFECT_ARC_COUNTDOWN_SCALE   = 128,
    };
    u8                             tintRgb[3];
    GfxCoord*                      muzzleCoord;
    GfxCoord*                      lightCoord;
    GfxCoord*                      playerCoords;
    EffectWork*                    effectWork;
    EffectWork*                    roundWork;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    s32                            audioPan;

    effectWork     = task->spawnArg2.pointer;
    transientLight = &gWorldCoordTransientPointLights[1];
    lightCoord     = &transientLight->light.head.transform.coord;
    pointLight     = &transientLight->light;
    muzzleCoord    = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            task->state = HYPERVELOCITY_CHARGE_EFFECT_IDLE;
        }
        return;
    }

    effectWork->age = effectWork->age + 1;
    switch (task->state) {
        case HYPERVELOCITY_CHARGE_EFFECT_INIT:
            muzzleCoord->parent = effectWork->parent;
            gfxSetRotIdentity(&muzzleCoord->coord);
            muzzleCoord->coord.t[0]   = _gHypervelocityMuzzleOffset.vx;
            muzzleCoord->coord.t[1]   = _gHypervelocityMuzzleOffset.vy;
            muzzleCoord->coord.t[2]   = _gHypervelocityMuzzleOffset.vz;
            muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state               = HYPERVELOCITY_CHARGE_EFFECT_IDLE;
            /* fallthrough */
        case HYPERVELOCITY_CHARGE_EFFECT_IDLE:
            if (task->spawnArg1.value == HYPERVELOCITY_CHARGE_REQUEST_START) {
                task->state     = HYPERVELOCITY_CHARGE_EFFECT_CHARGE;
                effectWork->age = 0;
            }
            return;
        case HYPERVELOCITY_CHARGE_EFFECT_CHARGE:
            // Charge may time out into discharge independently of the player's shot timer.
            actorRenderComposeCoord(muzzleCoord);
            effectWork->move.vy = -((effectWork->age & HYPERVELOCITY_CHARGE_EFFECT_DRIFT_AGE_MASK) << HYPERVELOCITY_CHARGE_EFFECT_CHARGE_DRIFT_SHIFT);
            if (effectWork->age & 1) {
                effectSpawn(EFFECT_SPARK_FADE, muzzleCoord, HYPERVELOCITY_CHARGE_EFFECT_PARTICLE_SIZE, &effectWork->move);
            }
            transientLight->framesLeft = HYPERVELOCITY_CHARGE_EFFECT_LIGHT_FRAMES;
            pointLight->inner          = HYPERVELOCITY_CHARGE_EFFECT_LIGHT_INNER;
            pointLight->outer          = HYPERVELOCITY_CHARGE_EFFECT_LIGHT_OUTER;
            gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            pointLight->head.color.b   = ((gRandomLcgState >> 16) & HYPERVELOCITY_CHARGE_EFFECT_LIGHT_RANDOM_MASK) + HYPERVELOCITY_CHARGE_EFFECT_CHARGING_BLUE_MIN;
            pointLight->head.color.r   = (u16)pointLight->head.color.b >> 1;
            pointLight->head.color.g   = pointLight->head.color.b >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &muzzleCoord->workm, &lightCoord->coord);
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->spawnArg1.value < 0) {
                task->spawnArg1.value = 0;
                task->state           = HYPERVELOCITY_CHARGE_EFFECT_IDLE;
                return;
            }
            if (effectWork->age >= HYPERVELOCITY_CHARGE_EFFECT_TIMEOUT_FRAMES) {
                task->spawnArg1.value = HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_FRAMES;
            }
            if (task->spawnArg1.value >= 2) {
                effectWork->scale  = 0;
                effectWork->angle  = HYPERVELOCITY_CHARGE_EFFECT_RADIUS_START;
                effectWork->period = 0;
                effectWork->step   = HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_UNIT / task->spawnArg1.value;
                task->state        = HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE;
            }
            return;
        case HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE:
            // The request becomes a countdown; its zero tick owns the round spawn.
            actorRenderComposeCoord(muzzleCoord);
            effectWork->move.vy = -((effectWork->age & HYPERVELOCITY_CHARGE_EFFECT_DRIFT_AGE_MASK) << HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_DRIFT_SHIFT);
            effectSpawn(EFFECT_FLASH_BURST, muzzleCoord, HYPERVELOCITY_CHARGE_EFFECT_PARTICLE_SIZE, &effectWork->move);
            transientLight->framesLeft = HYPERVELOCITY_CHARGE_EFFECT_LIGHT_FRAMES;
            pointLight->inner          = HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_LIGHT_INNER;
            pointLight->outer          = HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_LIGHT_OUTER;
            gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            pointLight->head.color.b   = ((gRandomLcgState >> 16) & HYPERVELOCITY_CHARGE_EFFECT_LIGHT_RANDOM_MASK) + HYPERVELOCITY_CHARGE_EFFECT_DISCHARGING_BLUE_MIN;
            pointLight->head.color.r   = (u16)pointLight->head.color.b >> 1;
            pointLight->head.color.g   = pointLight->head.color.b >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &muzzleCoord->workm, &lightCoord->coord);
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            effectWork->scale       += effectWork->step;
            if (effectWork->scale >= HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_UNIT) {
                effectWork->scale = HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_MAX;
            }
            effectWork->angle += HYPERVELOCITY_CHARGE_EFFECT_RADIUS_STEP;
            if (effectWork->angle >= HYPERVELOCITY_CHARGE_EFFECT_RADIUS_MAX + 1) {
                effectWork->angle = HYPERVELOCITY_CHARGE_EFFECT_RADIUS_MAX;
            }
            tintRgb[0] = effectWork->scale >> 1;
            tintRgb[1] = effectWork->scale >> 1;
            tintRgb[2] = effectWork->scale;
            effectDrawGouraudDisc(muzzleCoord, effectWork->angle, tintRgb);
            effectDrawGouraudDisc(muzzleCoord, (s16)((u16)effectWork->angle * 2), tintRgb);
            if (effectWork->scale >= HYPERVELOCITY_CHARGE_EFFECT_ARC_START_BRIGHTNESS) {
                if (effectWork->period == 0) {
                    audioPan = (s8)worldCoordGetOriginAudioPan(muzzleCoord);
                    sndEvtRequestScriptStart(SOUND_HYPERVELOCITY_DISCHARGE, audioPan, (s8)worldCoordGetOriginAudioDepth(muzzleCoord));
                }
                effectWork->period += (u16)effectWork->step * 2;
                if (effectWork->period >= HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_UNIT) {
                    effectWork->period = HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_MAX;
                }
                tintRgb[0] = effectWork->period >> 1;
                tintRgb[1] = effectWork->period >> 1;
                tintRgb[2] = effectWork->period;
                effectDrawOuterGlowBand(muzzleCoord, (s16)((u16)task->spawnArg1.value * HYPERVELOCITY_CHARGE_EFFECT_ARC_COUNTDOWN_SCALE), HYPERVELOCITY_CHARGE_EFFECT_ARC_RADIUS, tintRgb);
            }
            if (task->spawnArg1.value < 0) {
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_DISCHARGE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                task->spawnArg1.value = 0;
                task->state           = HYPERVELOCITY_CHARGE_EFFECT_IDLE;
                return;
            }
            task->spawnArg1.value = task->spawnArg1.value - 1;
            if (task->spawnArg1.value == 0) {
                task->state = HYPERVELOCITY_CHARGE_EFFECT_COOL;
                roundWork   = effectSpawn(EFFECT_HYPERVELOCITY_ROUND, muzzleCoord, 0, NULL);
                if (roundWork != NULL) {
                    taskReparent(task, roundWork->task);
                }
                effectWork->scale = HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_MAX;
            }
            return;
        case HYPERVELOCITY_CHARGE_EFFECT_COOL:
            // Fade the ring and emit smoke from alternating randomly selected hands.
            actorRenderComposeCoord(muzzleCoord);
            effectWork->move.vy = -((effectWork->age & HYPERVELOCITY_CHARGE_EFFECT_DRIFT_AGE_MASK) << HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE_DRIFT_SHIFT);
            effectSpawn(EFFECT_SPARK_FADE, muzzleCoord, HYPERVELOCITY_CHARGE_EFFECT_PARTICLE_SIZE, &effectWork->move);
            if (effectWork->angle > 0) {
                tintRgb[0] = effectWork->scale >> 1;
                tintRgb[1] = effectWork->scale >> 1;
                tintRgb[2] = effectWork->scale;
                effectDrawGouraudDisc(muzzleCoord, effectWork->angle, tintRgb);
                effectDrawGouraudDisc(muzzleCoord, (s16)((u16)effectWork->angle * 2), tintRgb);
                effectDrawScreenTint(tintRgb, GPU_BLEND_ADD);
                effectWork->scale = effectWork->scale - HYPERVELOCITY_CHARGE_EFFECT_FADE_STEP;
                effectWork->angle = effectWork->angle - HYPERVELOCITY_CHARGE_EFFECT_FADE_STEP;
            }
            playerCoords    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_DUST_PUFF, &playerCoords[(((gRandomLcgState >> 16) & 1) * HYPERVELOCITY_CHARGE_EFFECT_HAND_STRIDE) + HYPERVELOCITY_CHARGE_EFFECT_FIRST_HAND], HYPERVELOCITY_CHARGE_EFFECT_DUST_ARGUMENT, NULL);
            if (effectWork->age >= HYPERVELOCITY_CHARGE_EFFECT_RETIRE_AGE || task->spawnArg1.value < 0) {
                task->state = HYPERVELOCITY_CHARGE_EFFECT_IDLE;
            }
            return;
    }
}

/// Copies the player's local 3x3 basis into the Hypervelocity round's local matrix.
///
/// Requires live disjoint word-aligned matrices and a writable `roundMatrix`.
/// Coefficients have twelve fractional bits; any source scale is copied too.
/// Transfers exactly 18 bytes as four packed words and a final signed halfword,
/// preserving the round's alignment halfword and translation. A complete
/// `GfxRotationWords` assignment would also copy its tail alignment bytes.
/// Changes no GTE or scratch state and retains no pointer. The caller must
/// invalidate the round node's composition cache after updating its matrix.
static inline void _hypervelocityCopyRoundRotation(MATRIX* roundMatrix, const MATRIX* playerMatrix)
{
    GfxRotationWords*       roundRotation  = (GfxRotationWords*)roundMatrix;
    const GfxRotationWords* playerRotation = (const GfxRotationWords*)playerMatrix;

    roundRotation->m00M01 = playerRotation->m00M01;
    roundRotation->m02M10 = playerRotation->m02M10;
    roundRotation->m11M12 = playerRotation->m11M12;
    roundRotation->m20M21 = playerRotation->m20M21;
    roundRotation->m22    = playerRotation->m22;
}

void hypervelocityRoundTask(Task* task)
{
    enum {
        HYPERVELOCITY_ROUND_INIT                = 0,
        HYPERVELOCITY_ROUND_FLY                 = 1,
        HYPERVELOCITY_ROUND_COLLAPSE            = 2,
        HYPERVELOCITY_ROUND_SPEED               = 0x400,
        HYPERVELOCITY_ROUND_BRIGHTNESS          = 0xC0,
        HYPERVELOCITY_ROUND_VISUAL_RADIUS       = 0x500,
        HYPERVELOCITY_ROUND_COLLISION_RADIUS    = 0x800,
        HYPERVELOCITY_ROUND_LIFETIME_TICKS      = 0x15,
        HYPERVELOCITY_ROUND_COLLAPSE_STEP       = 0x40,
        HYPERVELOCITY_ROUND_COLLAPSE_MIN_RADIUS = 0x80,
        HYPERVELOCITY_ROUND_SPARK_ARGUMENT      = 0x400,
        HYPERVELOCITY_ROUND_LIGHT_SLOT          = 0,
        HYPERVELOCITY_ROUND_LIGHT_TTL           = 4,
        HYPERVELOCITY_ROUND_LIGHT_INNER_BASE    = 0x200,
        HYPERVELOCITY_ROUND_LIGHT_INDEX_SHIFT   = 9,
        HYPERVELOCITY_ROUND_LIGHT_OUTER_SCALE   = 16,
        HYPERVELOCITY_ROUND_LIGHT_RANDOM_MASK   = 0x700,
        HYPERVELOCITY_ROUND_LIGHT_BLUE_BASE     = 0x800,
        HYPERVELOCITY_ROUND_ROTATION_MASK       = 0xFFF,
        HYPERVELOCITY_ROUND_JITTER_MASK         = 0xFF,
    };
    GfxCoord                       groundCoord;
    SVECTOR                        stepEnd;
    SVECTOR                        stepStart;
    u8                             rgb[3];
    GfxCoord*                      roundCoord;
    GfxCoord*                      playerRootCoord;
    GfxCoord*                      lightCoord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          pointLight;
    EffectWork*                    effectWork;
    EffectWork*                    childEffect;
    _HypervelocityRoundBody*       roundBody;
    u32                            lightRandom;
    s32                            jitterIndex;

    roundBody  = task->work;
    effectWork = task->spawnArg2.pointer;
    roundCoord = task->extra.coordBody->coord;
    lightSlot  = &gWorldCoordTransientPointLights[HYPERVELOCITY_ROUND_LIGHT_SLOT];
    lightCoord = &lightSlot->light.head.transform.coord;
    pointLight = &lightSlot->light;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        effectWork->age = effectWork->age - 1;
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state != HYPERVELOCITY_ROUND_INIT) {
                worldCollisionUnlinkBody(&roundBody->body);
            }
            effectKillTask(effectWork, task);
        }
        return;
    }

    effectWork->age = effectWork->age + 1;
    switch (task->state) {
        case HYPERVELOCITY_ROUND_INIT:
            // Allocate collision separately and borrow only the player's local rotation.
            roundBody = memCalloc(sizeof(_HypervelocityRoundBody), 0);
            if (roundBody == NULL) {
                effectWork->age = 0;
                return;
            }
            task->exitCallback = _hypervelocityReleaseRound;
            playerRootCoord    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            _hypervelocityCopyRoundRotation(&roundCoord->coord, &playerRootCoord->coord);
            roundCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
            gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(roundCoord);
            effectWork->move.vx = 0;
            effectWork->move.vy = 0;
            effectWork->move.vz = HYPERVELOCITY_ROUND_SPEED;
            gte_SetRotMatrix(&playerRootCoord->coord);
            gte_ldv0(&effectWork->move);
            gte_rtv0();
            gte_stsv(&effectWork->move);
            for (jitterIndex = 0; jitterIndex < ARRAY_SIZE(_gHypervelocityConeTextureFrameOffsets); jitterIndex++) {
                gRandomLcgState                                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                _gHypervelocityConeTextureFrameOffsets[jitterIndex] = (gRandomLcgState >> 16) & HYPERVELOCITY_ROUND_JITTER_MASK;
            }
            effectWork->scale                = HYPERVELOCITY_ROUND_BRIGHTNESS;
            effectWork->angle                = HYPERVELOCITY_ROUND_VISUAL_RADIUS;
            gRandomLcgState                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectWork->period               = (gRandomLcgState >> 16) & HYPERVELOCITY_ROUND_ROTATION_MASK;
            task->work                       = roundBody;
            roundBody->body.context.contacts = roundBody->contacts;
            roundBody->body.radius           = HYPERVELOCITY_ROUND_COLLISION_RADIUS;
            roundBody->body.coord            = roundCoord;
            roundBody->body.key              = HYPERVELOCITY_ROUND_COLLISION_KEY;
            roundBody->body.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &roundBody->body);
            // The allocation already zeroed the entry; LAST terminates the table.
            roundBody->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
            roundBody->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            childEffect                  = effectSpawn(EFFECT_HYPERVELOCITY_SHOCK_RING, roundCoord, 0, NULL);
            if (childEffect != NULL) {
                taskReparent(task, childEffect->task);
            }
            task->state              = HYPERVELOCITY_ROUND_FLY;
            lightSlot->framesLeft    = HYPERVELOCITY_ROUND_LIGHT_TTL;
            pointLight->inner        = (effectWork->index << HYPERVELOCITY_ROUND_LIGHT_INDEX_SHIFT) + HYPERVELOCITY_ROUND_LIGHT_INNER_BASE;
            pointLight->outer        = pointLight->inner * HYPERVELOCITY_ROUND_LIGHT_OUTER_SCALE;
            lightRandom              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            pointLight->head.color.b = ((lightRandom >> 16) & HYPERVELOCITY_ROUND_LIGHT_RANDOM_MASK) + HYPERVELOCITY_ROUND_LIGHT_BLUE_BASE;
            pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
            pointLight->head.color.g = pointLight->head.color.b >> 1;
            lightCoord->coord.t[0]   = roundCoord->coord.t[0];
            lightCoord->coord.t[1]   = roundCoord->coord.t[1];
            lightCoord->coord.t[2]   = roundCoord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            rgb[0]                   = effectWork->scale >> 2;
            rgb[1]                   = effectWork->scale >> 2;
            rgb[2]                   = effectWork->scale >> 1;
            gRandomLcgState          = lightRandom;
            spriteQuadDraw(roundCoord, effectWork->age, effectWork->angle, effectWork->period);
            effectDrawGouraudDisc(roundCoord, effectWork->angle, rgb);
            return;
        case HYPERVELOCITY_ROUND_FLY:
            // Probe the composed step segment; pair contacts do not stop the round.
            actorRenderComposeCoord(roundCoord);
            stepStart.vx               = roundCoord->workm.t[0];
            stepStart.vy               = roundCoord->workm.t[1];
            stepStart.vz               = roundCoord->workm.t[2];
            roundCoord->coord.t[0]    += effectWork->move.vx;
            roundCoord->coord.t[1]    += effectWork->move.vy;
            roundCoord->coord.t[2]    += effectWork->move.vz;
            roundCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
            gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(roundCoord);
            stepEnd.vx = roundCoord->workm.t[0];
            stepEnd.vy = roundCoord->workm.t[1];
            stepEnd.vz = roundCoord->workm.t[2];
            rgb[0]     = effectWork->scale >> 2;
            rgb[1]     = effectWork->scale >> 2;
            rgb[2]     = effectWork->scale >> 1;
            spriteQuadDraw(roundCoord, effectWork->age, effectWork->angle, effectWork->period);
            effectDrawGouraudDisc(roundCoord, effectWork->angle, rgb);
            _jetConeDraw(roundCoord, effectWork->age, effectWork->angle, 0);
            _jetConeDraw(roundCoord, effectWork->age, effectWork->angle, 1);
            if (gRoomEffectState->groundTraceEnabled != 0 && worldCollisionProjectGroundCoord(roundCoord, &groundCoord) == 1) {
                _groundGlowDraw(&groundCoord, effectWork->angle);
            }
            if (effectWork->age < HYPERVELOCITY_ROUND_LIFETIME_TICKS) {
                effectSpawn(EFFECT_FLASH_BURST, roundCoord, HYPERVELOCITY_ROUND_SPARK_ARGUMENT, NULL);
                childEffect = effectSpawn(EFFECT_HYPERVELOCITY_DISCHARGE_CONE, roundCoord, 0, NULL);
                if (childEffect != NULL) {
                    taskReparent(task, childEffect->task);
                }
            }
            lightCoord->coord.t[0]   = roundCoord->coord.t[0];
            lightCoord->coord.t[1]   = roundCoord->coord.t[1];
            lightCoord->coord.t[2]   = roundCoord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            pointLight->head.color.b = ((gRandomLcgState >> 16) & HYPERVELOCITY_ROUND_LIGHT_RANDOM_MASK) + HYPERVELOCITY_ROUND_LIGHT_BLUE_BASE;
            pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
            lightSlot->framesLeft    = HYPERVELOCITY_ROUND_LIGHT_TTL;
            pointLight->head.color.g = pointLight->head.color.b >> 1;
            if (worldCollisionProbeGridSegment(&stepEnd, &stepStart, NULL, NULL) == 1) {
                worldCollisionUnlinkBody(&roundBody->body);
                task->state = HYPERVELOCITY_ROUND_COLLAPSE;
                return;
            }
            if (effectWork->age >= HYPERVELOCITY_ROUND_LIFETIME_TICKS) {
                worldCollisionUnlinkBody(&roundBody->body);
                effectKillTask(effectWork, task);
                return;
            }
            worldCollisionClearContacts(roundBody->contacts);
            return;
        case HYPERVELOCITY_ROUND_COLLAPSE:
            // The unlinked round stays at the hit point while its visual radius contracts.
            actorRenderComposeCoord(roundCoord);
            effectWork->angle = effectWork->angle - HYPERVELOCITY_ROUND_COLLAPSE_STEP;
            rgb[0]            = effectWork->scale >> 2;
            rgb[1]            = effectWork->scale >> 2;
            rgb[2]            = effectWork->scale >> 1;
            spriteQuadDraw(roundCoord, effectWork->age, effectWork->angle, effectWork->period);
            effectDrawGouraudDisc(roundCoord, effectWork->angle, rgb);
            if (effectWork->angle < HYPERVELOCITY_ROUND_COLLAPSE_MIN_RADIUS) {
                effectKillTask(effectWork, task);
                return;
            }
            effectSpawn(EFFECT_FLASH_BURST, roundCoord, HYPERVELOCITY_ROUND_SPARK_ARGUMENT, NULL);
            return;
    }
}

#define JET_CONE_CLUT         0x42C1
#define JET_CONE_RIM_SHORT    0x600
#define JET_CONE_RIM_LONG     0x800
#define JET_CONE_FRAME_JITTER _gHypervelocityConeTextureFrameOffsets
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

/// Draws the two opposed textured walls of a rising, opening discharge cone.
///
/// `coord` supplies a composed transform in the input space of `GsWSMATRIX`.
/// `halfExtent` is the collar's local X half-width and both squares' Z
/// half-depth, in game units; `ageFrames` raises and widens the mouth by 128
/// units per tick. The effect calls this at ages 1..16 with half-extents
/// 512..1952. `rgb` supplies three readable tint bytes. Vertices narrow to
/// signed 16 bits after rotation and translation.
///
/// Borrows one scratch block and appends up to two additive `POLY_FT4`
/// packets to the unchecked frame arena, rejecting negative GTE FLAG words.
/// The initialized jitter table selects six texture cells independently for
/// the walls. Inputs must remain clear of the scratch block and packet arena;
/// packets live through GPU drawing. Changes GTE state; retains no inputs.
static void _hypervelocityDrawDischargeCone(const GfxCoord* coord, s16 ageFrames, s32 halfExtent, const u8* rgb)
{
/// Rotates one cone vertex in place, narrowing the result to signed 16 bits.
///
/// Captures the initialized local worldRotation (4096 per unit); vertex must
/// be a side-effect-free pointer because it is evaluated twice. Expands to
/// four statements: use only as a standalone sequence inside this function.
/// Translation is added separately. Changes GTE state; retains no pointers.
#define HYPERVELOCITY_ROTATE_CONE_VERTEX(vertex) \
    gte_SetRotMatrix(worldRotation);             \
    gte_ldv0((vertex));                          \
    gte_rtv0();                                  \
    gte_stsv((vertex))

    // Geometry is in local game units; the animation occupies six 40-texel cells.
    enum {
        HYPERVELOCITY_DISCHARGE_CONE_COLLAR_HEIGHT        = 0x700,
        HYPERVELOCITY_DISCHARGE_CONE_INITIAL_MOUTH_HEIGHT = 0x600,
        HYPERVELOCITY_DISCHARGE_CONE_INITIAL_FLARE        = 0x200,
        HYPERVELOCITY_DISCHARGE_CONE_AGE_GEOMETRY_SHIFT   = 7,
        HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_FRAME_COUNT  = 6,
        HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_CELL_SIZE    = 40,
        HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_TOP_V        = 0x60
    };

    _HypervelocityDischargeConeScratch* scratch;
    POLY_FT4*                           wall;
    const EffectUnitQuadCorner*         unitCorners;
    SVECTOR*                            collarVertex;
    const MATRIX*                       worldRotation;
    s32                                 cornerIndex;
    s32                                 mouthExpansion;
    s32                                 mouthHeight;
    u16                                 mouthHalfWidth;
    s32                                 depthHalfExtent;
    s32                                 textureU;

    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_HypervelocityDischargeConeScratch);
    mouthExpansion  = ageFrames;
    mouthExpansion  = mouthExpansion << HYPERVELOCITY_DISCHARGE_CONE_AGE_GEOMETRY_SHIFT;
    mouthHeight     = HYPERVELOCITY_DISCHARGE_CONE_INITIAL_MOUTH_HEIGHT - mouthExpansion;
    mouthExpansion  = mouthExpansion + HYPERVELOCITY_DISCHARGE_CONE_INITIAL_FLARE;
    mouthHalfWidth  = halfExtent + mouthExpansion;
    depthHalfExtent = halfExtent;
    gte_SetTransMatrix(&GsWSMATRIX);
    cornerIndex   = 0;
    worldRotation = &coord->workm;
    unitCorners   = gEffectUnitQuadCorners;
    // Build and rotate both squares; corner products and translations keep only
    // their low 16 bits when stored as signed world-space vertices.
    do {
        scratch->vertices[cornerIndex].vx = (u16)unitCorners[cornerIndex].axis0Sign * mouthHalfWidth;
        scratch->vertices[cornerIndex].vy = mouthHeight;
        scratch->vertices[cornerIndex].vz = (u16)unitCorners[cornerIndex].axis1Sign * depthHalfExtent;
        HYPERVELOCITY_ROTATE_CONE_VERTEX(&scratch->vertices[cornerIndex]);
        scratch->vertices[cornerIndex].vx += (u16)coord->workm.t[0];
        scratch->vertices[cornerIndex].vy += (u16)coord->workm.t[1];
        scratch->vertices[cornerIndex].vz += (u16)coord->workm.t[2];
        collarVertex                       = &scratch->vertices[cornerIndex] + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT;
        collarVertex->vx                   = (u16)unitCorners[cornerIndex].axis0Sign * halfExtent;
        collarVertex->vy                   = HYPERVELOCITY_DISCHARGE_CONE_COLLAR_HEIGHT;
        collarVertex->vz                   = (u16)unitCorners[cornerIndex].axis1Sign * depthHalfExtent;
        HYPERVELOCITY_ROTATE_CONE_VERTEX(&scratch->vertices[cornerIndex + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT]);
        collarVertex->vx += (u16)coord->workm.t[0];
        cornerIndex++;
        collarVertex->vy += (u16)coord->workm.t[1];
        collarVertex->vz += (u16)coord->workm.t[2];
    } while (cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners));

    // Project one wall at a time from the corners sharing a local X sign.
    gte_SetRotMatrix(&GsWSMATRIX);
    cornerIndex = 0;
    do {
        gte_ldv0(&scratch->vertices[cornerIndex]);
        gte_rtps();
        gte_stsxy(&scratch->sxy0);
        gte_ldv3(&scratch->vertices[cornerIndex + 2],
                 &scratch->vertices[cornerIndex + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT],
                 &scratch->vertices[cornerIndex + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT + 2]);
        gte_rtpt();
        gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            scratch->otz++;
            wall           = gGpuPrimCursor;
            gGpuPrimCursor = wall + 1;
            setPolyFT4(wall);
            setRGB0(wall, rgb[0], rgb[1], rgb[2]);
            setSemiTrans(wall, 1);
            wall->tpage = getTPage(0, GPU_BLEND_ADD, 640, 0);
            wall->clut  = getClut(16, 267);
            textureU    = (s16)((_gHypervelocityConeTextureFrameOffsets[cornerIndex] + ageFrames) % HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_FRAME_COUNT) * HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_CELL_SIZE;
            wall->u0    = textureU;
            wall->v0    = HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_TOP_V;
            wall->u1    = textureU + HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            wall->v1    = HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_TOP_V;
            wall->u2    = textureU;
            wall->u3    = textureU + HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            wall->v2    = HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_TOP_V + HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            wall->v3    = HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_TOP_V + HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            setXY4(wall, scratch->sxy0.vx, scratch->sxy0.vy, scratch->sxy1.vx, scratch->sxy1.vy, scratch->sxy2.vx, scratch->sxy2.vy, scratch->sxy3.vx,
                   scratch->sxy3.vy);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), wall);
        }
        cornerIndex++;
    } while (cornerIndex < HYPERVELOCITY_DISCHARGE_CONE_WALL_COUNT);
    SCRATCH_STACK_RELEASE_BLOCK(_HypervelocityDischargeConeScratch);
#undef HYPERVELOCITY_ROTATE_CONE_VERTEX
}

/// Tears down a counted hypervelocity round after unlinking its collision sphere.
///
/// The live task owns a `_HypervelocityRoundBody` in `work` (or NULL) and a
/// separate `EffectWork` in `spawnArg2.pointer`. Default teardown frees the
/// collision block and releases the coordinate body and task. Call once;
/// neither allocation nor the task may be used after teardown.
static void _hypervelocityReleaseRound(Task* task)
{
    _HypervelocityRoundBody* roundBody  = task->work;
    EffectWork*              effectWork = task->spawnArg2.pointer;

    if (roundBody != NULL) {
        worldCollisionUnlinkBody(&roundBody->body);
    }
    effectKillTask(effectWork, task);
}

void hypervelocityShockRingTask(Task* task)
{
    enum {
        HYPERVELOCITY_SHOCK_RING_STATE_INITIALIZE   = 0,
        HYPERVELOCITY_SHOCK_RING_STATE_EXPAND       = 1,
        HYPERVELOCITY_SHOCK_RING_INITIAL_BRIGHTNESS = 0xF0,
        HYPERVELOCITY_SHOCK_RING_INITIAL_RADIUS     = 0x100,
        HYPERVELOCITY_SHOCK_RING_RADIUS_STEP        = 0x40,
        HYPERVELOCITY_SHOCK_RING_FADE_STEP          = 0x10
    };

    EffectWork* effectWork;
    GfxCoord*   coord;
    s16         effectControl;
    s16         nextBrightness;
    u8          rgb[3];

    effectWork    = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(effectWork, task);
        return;
    }

    actorRenderComposeCoord(coord);
    effectWork->age++;
    if (task->state == HYPERVELOCITY_SHOCK_RING_STATE_INITIALIZE) {
        effectWork->scale = HYPERVELOCITY_SHOCK_RING_INITIAL_BRIGHTNESS;
        effectWork->angle = HYPERVELOCITY_SHOCK_RING_INITIAL_RADIUS;
        task->state       = HYPERVELOCITY_SHOCK_RING_STATE_EXPAND;
    }
    rgb[0] = effectWork->scale >> 1;
    rgb[1] = effectWork->scale >> 1;
    rgb[2] = effectWork->scale;
    effectDrawRaisedGlowBand(coord, effectWork->angle, rgb);
    effectWork->angle += HYPERVELOCITY_SHOCK_RING_RADIUS_STEP;
    nextBrightness     = effectWork->scale - HYPERVELOCITY_SHOCK_RING_FADE_STEP;
    effectWork->scale  = nextBrightness;
    if (nextBrightness < HYPERVELOCITY_SHOCK_RING_FADE_STEP) {
        effectKillTask(effectWork, task);
    }
}

void hypervelocityDischargeConeTask(Task* task)
{
    enum {
        HYPERVELOCITY_DISCHARGE_CONE_STATE_INITIALIZE    = 0,
        HYPERVELOCITY_DISCHARGE_CONE_STATE_EXPAND        = 1,
        HYPERVELOCITY_DISCHARGE_CONE_INITIAL_BRIGHTNESS  = 0x80,
        HYPERVELOCITY_DISCHARGE_CONE_INITIAL_HALF_EXTENT = 0x200,
        HYPERVELOCITY_DISCHARGE_CONE_HALF_EXTENT_STEP    = 0x60,
        HYPERVELOCITY_DISCHARGE_CONE_FADE_STEP           = 8,
        HYPERVELOCITY_DISCHARGE_CONE_MIN_BRIGHTNESS      = 6
    };

    EffectWork* effectWork;
    GfxCoord*   coord;
    s16         effectControl;
    s16         nextBrightness;
    u8          rgb[3];

    effectWork    = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(effectWork, task);
        return;
    }

    actorRenderComposeCoord(coord);
    effectWork->age++;
    if (task->state == HYPERVELOCITY_DISCHARGE_CONE_STATE_INITIALIZE) {
        effectWork->scale = HYPERVELOCITY_DISCHARGE_CONE_INITIAL_BRIGHTNESS;
        effectWork->angle = HYPERVELOCITY_DISCHARGE_CONE_INITIAL_HALF_EXTENT;
        task->state       = HYPERVELOCITY_DISCHARGE_CONE_STATE_EXPAND;
    }
    rgb[0] = effectWork->scale;
    rgb[1] = effectWork->scale;
    rgb[2] = effectWork->scale;
    _hypervelocityDrawDischargeCone(coord, effectWork->age, effectWork->angle, rgb);
    effectWork->angle += HYPERVELOCITY_DISCHARGE_CONE_HALF_EXTENT_STEP;
    nextBrightness     = effectWork->scale - HYPERVELOCITY_DISCHARGE_CONE_FADE_STEP;
    effectWork->scale  = nextBrightness;
    if (nextBrightness < HYPERVELOCITY_DISCHARGE_CONE_MIN_BRIGHTNESS) {
        effectKillTask(effectWork, task);
    }
}

/// Copies the player's draw state and advances one attached Hypervelocity model component.
///
/// Low-nibble selectors are root (0), sliding component (1) and hinged component
/// (2). Child tasks borrow the root weapon task's 0x10/0x20 charge-motion flags;
/// the slide uses killCountdown as a 0..60 travel counter, the hinge uses stored
/// pitch in 4096 angle units per turn. Requires live player/root/child models.
/// Reserves sixteen unused scratch bytes per dispatch; their role is unproven.
static void _hypervelocityUpdateModelPose(Task* task)
{
    enum {
        HYPERVELOCITY_MODEL_SLIDE_MAX_TICKS      = 60,
        HYPERVELOCITY_MODEL_SLIDE_UNITS_PER_TICK = 4,
        HYPERVELOCITY_MODEL_HINGE_STEP           = 0x110,
        HYPERVELOCITY_MODEL_HINGE_LIMIT          = -0x400,
        HYPERVELOCITY_MODEL_PLAYER_ATTACK_STATE  = 4,
        HYPERVELOCITY_MODEL_SCRATCH_BYTES        = 16,
    };
    Task*      weaponTask;
    TmdObject* weaponModel;
    TmdObject* playerModel;
    GfxCoord*  modelCoord;
    Task*      playerTask;
    s16        slideTicksLeft;

    weaponTask  = task->parent;
    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    weaponModel = task->extra.tmd;
    playerModel = playerTask->extra.tmd;
    modelCoord  = weaponModel->coords;

    modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    weaponModel->flags       = playerModel->flags;
    weaponModel->colorMtx    = playerModel->colorMtx;
    weaponModel->lightMtx    = playerModel->lightMtx;

    SCRATCH_STACK_RESERVE_BYTES(HYPERVELOCITY_MODEL_SCRATCH_BYTES);
    switch (task->spawnArg1.value & HYPERVELOCITY_MODEL_COMPONENT_MASK) {
        case HYPERVELOCITY_MODEL_ROOT:
            if (((GameActor*)playerTask->work)->mode != GAME_ACTOR_MODE_NORMAL || ((GameActor*)playerTask->work)->state != HYPERVELOCITY_MODEL_PLAYER_ATTACK_STATE) {
                task->spawnArg1.value = 0;
            }
            break;
        case HYPERVELOCITY_MODEL_SLIDE:
            if (weaponTask->spawnArg1.value & HYPERVELOCITY_MODEL_CHARGE_SLIDE) {
                if (task->killCountdown < HYPERVELOCITY_MODEL_SLIDE_MAX_TICKS) {
                    task->killCountdown = task->killCountdown + 1;
                }
            } else if (task->killCountdown > 0) {
                slideTicksLeft      = task->killCountdown - 1;
                task->killCountdown = slideTicksLeft;
                if (slideTicksLeft == 0) {
                    sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_CANCEL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                }
            }
            modelCoord->coord.t[0] = 0;
            modelCoord->coord.t[1] = -task->killCountdown * HYPERVELOCITY_MODEL_SLIDE_UNITS_PER_TICK;
            modelCoord->coord.t[2] = -0x16;
            break;
        case HYPERVELOCITY_MODEL_HINGE:
            if (weaponTask->spawnArg1.value & HYPERVELOCITY_MODEL_CHARGE_OPEN) {
                if (modelCoord->param.rot.vx >= HYPERVELOCITY_MODEL_HINGE_LIMIT + 1) {
                    modelCoord->param.rot.vx = modelCoord->param.rot.vx - HYPERVELOCITY_MODEL_HINGE_STEP;
                }
            } else if (modelCoord->param.rot.vx < 0) {
                modelCoord->param.rot.vx = modelCoord->param.rot.vx + HYPERVELOCITY_MODEL_HINGE_STEP;
            }
            modelCoord->coord.t[0] = -0x14;
            modelCoord->coord.t[1] = -0x15C;
            modelCoord->coord.t[2] = 0xA8;

            gfxSetRotIdentity(&modelCoord->coord);
            RotMatrixX(modelCoord->param.rot.vx, &modelCoord->coord);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(HYPERVELOCITY_MODEL_SCRATCH_BYTES);
}

/// Attaches a Hypervelocity slide or hinge to the weapon's transform and teardown tree.
///
/// Both tasks must be live TMD tasks with a root coordinate. `weaponCoord`
/// is the weapon's root captured before spawning; `weaponModel` supplies its
/// lighting. The newly spawned component's cache must still be dirty, and
/// its local pose stays intact. It borrows the parent coordinate and lighting
/// matrices, which must outlive their use.
/// Reparenting makes the weapon task responsible for the component's teardown.
static inline void _hypervelocityAttachModelComponent(Task* weaponTask, Task* componentTask,
                                                      GfxCoord* weaponCoord, const TmdObject* weaponModel)
{
    TmdObject* componentModel;
    GfxCoord*  componentCoord;

    componentCoord           = componentTask->extra.tmd->coords;
    componentCoord->parent   = weaponCoord;
    componentModel           = componentTask->extra.tmd;
    componentModel->colorMtx = weaponModel->colorMtx;
    componentModel->lightMtx = weaponModel->lightMtx;
    taskReparent(weaponTask, componentTask);
}

/// Initializes a Hypervelocity model and attaches the root's slide and hinge.
///
/// Requires state 0 and a live TMD body; spawnArg1's low nibble selects root
/// (0), slide (1) or hinge (2). Child allocations fail independently. Only a
/// successful hinge spawn also places the root at (-6, -60, -22) local game
/// units. Children borrow root lighting/coordinate storage and become owned
/// tasks through reparenting. Installs the common model release callback and
/// advances to pose state 1 without allocating a separate work block.
static void _hypervelocityInitWeaponModel(Task* task)
{
    enum {
        HYPERVELOCITY_MODEL_TASK_BANK        = 7,
        HYPERVELOCITY_MODEL_SLIDE_DESCRIPTOR = 0x70,
        HYPERVELOCITY_MODEL_HINGE_DESCRIPTOR = 0x74,
        HYPERVELOCITY_MODEL_POSE_STATE       = 1,
    };
    Task*      componentTask;
    TmdObject* weaponModel;
    GfxCoord*  weaponCoord;

    weaponModel               = task->extra.tmd;
    weaponCoord               = weaponModel->coords;
    task->state              += HYPERVELOCITY_MODEL_POSE_STATE;
    task->exitCallback        = _hypervelocityReleaseWeapon;
    task->killCountdown       = 0;
    weaponCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    weaponModel->flags        = 0;
    // Each child has its own transform under the root and shares its lighting.
    if (!(task->spawnArg1.value & HYPERVELOCITY_MODEL_COMPONENT_MASK)) {
        componentTask = taskSpawn(HYPERVELOCITY_MODEL_TASK_BANK, HYPERVELOCITY_MODEL_SLIDE_DESCRIPTOR, HYPERVELOCITY_MODEL_SLIDE, 0);
        if (componentTask != NULL) {
            _hypervelocityAttachModelComponent(task, componentTask, weaponCoord, weaponModel);
        }
        componentTask = taskSpawn(HYPERVELOCITY_MODEL_TASK_BANK, HYPERVELOCITY_MODEL_HINGE_DESCRIPTOR, HYPERVELOCITY_MODEL_HINGE, 0);
        if (componentTask != NULL) {
            _hypervelocityAttachModelComponent(task, componentTask, weaponCoord, weaponModel);
            weaponCoord->coord.t[0] = -6;
            weaponCoord->coord.t[1] = -0x3C;
            weaponCoord->coord.t[2] = -0x16;
        }
    }
}

/// Selects weapon teardown for the next dispatch of the four-state weapon task.
static void _hypervelocityQueueWeaponTeardown(Task* task)
{
    enum { HYPERVELOCITY_WEAPON_STATE_TEARDOWN = 3 };

    task->state = HYPERVELOCITY_WEAPON_STATE_TEARDOWN;
}

/// Releases a live hypervelocity weapon-model task and its child tasks.
///
/// Used both as the weapon's final state and its exit callback. Call once;
/// `taskKill`'s work ownership and deferred model release rules apply.
static void _hypervelocityReleaseWeapon(Task* task)
{
    taskKill(task);
}

void hypervelocityWeaponModelTask(Task* task)
{
    TaskFunc stateHandlers[] = {
        _hypervelocityInitWeaponModel,
        _hypervelocityUpdateModelPose,
        _hypervelocityQueueWeaponTeardown,
        _hypervelocityReleaseWeapon,
    };

    stateHandlers[task->state](task);
}

void hypervelocityAttackState(Task* playerTask)
{
    enum {
        HYPERVELOCITY_PHASE_PREPARE             = 0,
        HYPERVELOCITY_PHASE_CHARGE              = 1,
        HYPERVELOCITY_PHASE_RECOIL              = 2,
        HYPERVELOCITY_PHASE_RECOVER             = 3,
        HYPERVELOCITY_PLAYER_ATTACK_STATE       = 4,
        HYPERVELOCITY_ANIMATION_SECONDARY       = 0xB,
        HYPERVELOCITY_ANIMATION_CHARGE          = 0xE,
        HYPERVELOCITY_ANIMATION_RELEASE         = 0xF,
        HYPERVELOCITY_RECOIL_FRAMES             = 0x15,
        HYPERVELOCITY_CHARGE_FRAMES             = 90,
        HYPERVELOCITY_SECOND_CHARGE_STAGE_FRAME = 60,
        HYPERVELOCITY_RECOIL_PUSH_WINDOW        = 19,
        HYPERVELOCITY_RECOIL_FIRST_PUSH_FRAME   = 18,
        HYPERVELOCITY_RECOIL_DIVISOR            = 378,
        HYPERVELOCITY_RECOIL_FIRST_PUSH_DIVISOR = 244,
        HYPERVELOCITY_WEAPON_ID                 = 22,
        HYPERVELOCITY_SECOND_STAGE_SOUND        = SOUND_WEAPON(SOUND_BANK_HYPERVELOCITY, 2),
        HYPERVELOCITY_FIRE_SOUND                = SOUND_WEAPON(SOUND_BANK_HYPERVELOCITY, 7),
    };
    _HypervelocityRecoilScratch* scratch;
    GameActor*                   actor;
    GfxCoord*                    rootCoord;
    Task*                        weaponTask;
    s32                          recoilDivisor;
    s32                          chargeTicks;
    s32                          recoilTicksLeft;

    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_HypervelocityRecoilScratch);
    actor      = playerTask->work;
    weaponTask = actor->equipmentTasks[1];
    switch (actor->statePhase) {
        case HYPERVELOCITY_PHASE_PREPARE:
            actor->mode                              = GAME_ACTOR_MODE_NORMAL;
            actor->state                             = HYPERVELOCITY_PLAYER_ATTACK_STATE;
            actor->movementMode                      = 0;
            actor->turnRateIndex                     = 0;
            actor->animationState                    = 0;
            actor->statePhase                        = HYPERVELOCITY_PHASE_CHARGE;
            actor->weaponEffectTask->spawnArg1.value = HYPERVELOCITY_CHARGE_REQUEST_START;
            actor->stateTimer                        = 0;
            weaponTask->spawnArg1.value             |= HYPERVELOCITY_MODEL_CHARGE_SLIDE;
            worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND_HYPERVELOCITY_CHARGE_START, 0);
            worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND_HYPERVELOCITY_CHARGE_LOOP, 0);
            playerActorPlayChildSlotsWithBlend(playerTask, HYPERVELOCITY_ANIMATION_CHARGE, 0, 3);
            /* fallthrough */
        case HYPERVELOCITY_PHASE_CHARGE:
            if (actor->padHeld & (PAD_BUTTON_R1 | PAD_BUTTON_R2)) {
                chargeTicks       = actor->stateTimer + 1;
                actor->stateTimer = chargeTicks;
                if (chargeTicks >= HYPERVELOCITY_CHARGE_FRAMES) {
                    actor->rumblePosted = 0;
                    actor->statePhase++;
                    weaponTask->spawnArg1.value = 0;
                    actor->stateTimer           = HYPERVELOCITY_RECOIL_FRAMES;
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(HYPERVELOCITY_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_FIRE_SOUND, 1);
                    playerActorResetChildSlots(playerTask, HYPERVELOCITY_ANIMATION_SECONDARY);
                } else if (chargeTicks == HYPERVELOCITY_SECOND_CHARGE_STAGE_FRAME) {
                    weaponTask->spawnArg1.value |= HYPERVELOCITY_MODEL_CHARGE_OPEN;
                    sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_START, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SECOND_STAGE_SOUND, 0);
                }
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_CANCEL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            } else {
                actor->statePhase                        = HYPERVELOCITY_PHASE_RECOVER;
                actor->weaponEffectTask->spawnArg1.value = HYPERVELOCITY_CHARGE_REQUEST_CANCEL;
                weaponTask->spawnArg1.value              = 0;
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_START, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND_HYPERVELOCITY_CHARGE_CANCEL, 0);
                playerActorPlayChildSlotsWithBlend(playerTask, HYPERVELOCITY_ANIMATION_RELEASE, 0, 3);
            }
            break;
        case HYPERVELOCITY_PHASE_RECOIL:
            // Push backward on the last eighteen ticks, with a stronger first step.
            recoilTicksLeft   = actor->stateTimer - 1;
            actor->stateTimer = recoilTicksLeft;
            if (recoilTicksLeft != 0) {
                if (recoilTicksLeft < HYPERVELOCITY_RECOIL_PUSH_WINDOW) {
                    rootCoord     = playerTask->extra.tmd->coords;
                    recoilDivisor = HYPERVELOCITY_RECOIL_DIVISOR;
                    if (recoilTicksLeft == HYPERVELOCITY_RECOIL_FIRST_PUSH_FRAME) {
                        recoilDivisor = HYPERVELOCITY_RECOIL_FIRST_PUSH_DIVISOR;
                    }
                    actor->movementSign = -1;
                    gfxReadMatrixZAxis(&rootCoord->coord, &scratch->forward);
                    scratch->recoil.vx     = -(scratch->forward.vx * actor->stateTimer / recoilDivisor);
                    scratch->recoil.vy     = -(scratch->forward.vy * actor->stateTimer / recoilDivisor);
                    scratch->recoil.vz     = -(scratch->forward.vz * actor->stateTimer / recoilDivisor);
                    rootCoord->coord.t[0] += scratch->recoil.vx;
                    rootCoord->coord.t[1] += scratch->recoil.vy;
                    rootCoord->coord.t[2] += scratch->recoil.vz;
                }
            } else {
                actor->statePhase++;
            }
            /* fallthrough */
        case HYPERVELOCITY_PHASE_RECOVER:
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_HypervelocityRecoilScratch);
}

/// Initial identity transform of the Hypervelocity weapon root's single part.
static TmdBone _gHypervelocityRootModelSkeleton[] = {
#include "assets/hypervelocity_model_02C9C_skeleton.inc"
};

/// Vertex counts in part order for `gHypervelocityRootModel`.
static u32 _gHypervelocityRootModelPartVerts[] = {
#include "assets/hypervelocity_model_02C9C_partVerts.inc"
};

/// Hypervelocity weapon-root vertices in integer part-local coordinates.
static SVECTOR _gHypervelocityRootModelVerts[] = {
#include "assets/hypervelocity_model_02C9C_verts.inc"
};

/// Hypervelocity weapon-root lighting normals, with twelve fractional bits.
static SVECTOR _gHypervelocityRootModelNormals[] = {
#include "assets/hypervelocity_model_02C9C_normals.inc"
};

/// Writable commands for the root model's FT4, two GT3s and 38 GT4s.
///
/// Model creation replaces the commands' handler slots in place.
static u32 _gHypervelocityRootModelStream[] = {
#include "assets/hypervelocity_model_02C9C_stream.inc"
};

TmdSource gHypervelocityRootModel = {
    .handlersResolved    = 0,
    .bufferHalfBytes     = sizeof(POLY_FT4) + 2 * sizeof(POLY_GT3) + 38 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gHypervelocityRootModelSkeleton),
    .partVertexCounts    = _gHypervelocityRootModelPartVerts,
    .verts               = _gHypervelocityRootModelVerts,
    .normals             = _gHypervelocityRootModelNormals,
    .skeleton            = _gHypervelocityRootModelSkeleton,
    .stream              = _gHypervelocityRootModelStream,
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityRootModelPartVerts) == ARRAY_SIZE(_gHypervelocityRootModelSkeleton), hypervelocity_root_model_part_counts);

/// Initial identity transform of the Hypervelocity slide's single part.
static TmdBone _gHypervelocitySlideModelSkeleton[] = {
#include "assets/hypervelocity_model_03284_skeleton.inc"
};

/// Vertex counts in part order for `gHypervelocitySlideModel`.
static u32 _gHypervelocitySlideModelPartVerts[] = {
#include "assets/hypervelocity_model_03284_partVerts.inc"
};

/// Hypervelocity slide vertices in integer part-local coordinates.
static SVECTOR _gHypervelocitySlideModelVerts[] = {
#include "assets/hypervelocity_model_03284_verts.inc"
};

/// Hypervelocity slide lighting normals, with twelve fractional bits.
static SVECTOR _gHypervelocitySlideModelNormals[] = {
#include "assets/hypervelocity_model_03284_normals.inc"
};

/// Writable commands for the slide model's fourteen GT4s.
///
/// Model creation replaces the commands' handler slots in place.
static u32 _gHypervelocitySlideModelStream[] = {
#include "assets/hypervelocity_model_03284_stream.inc"
};

TmdSource gHypervelocitySlideModel = {
    .handlersResolved    = 0,
    .bufferHalfBytes     = 14 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gHypervelocitySlideModelSkeleton),
    .partVertexCounts    = _gHypervelocitySlideModelPartVerts,
    .verts               = _gHypervelocitySlideModelVerts,
    .normals             = _gHypervelocitySlideModelNormals,
    .skeleton            = _gHypervelocitySlideModelSkeleton,
    .stream              = _gHypervelocitySlideModelStream,
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocitySlideModelPartVerts) == ARRAY_SIZE(_gHypervelocitySlideModelSkeleton), hypervelocity_slide_model_part_counts);

/// Initial identity transform of the Hypervelocity hinge's single part.
static TmdBone _gHypervelocityHingeModelSkeleton[] = {
#include "assets/hypervelocity_model_03550_skeleton.inc"
};

/// Vertex counts in part order for `gHypervelocityHingeModel`.
static u32 _gHypervelocityHingeModelPartVerts[] = {
#include "assets/hypervelocity_model_03550_partVerts.inc"
};

/// Hypervelocity hinged-component vertices in integer part-local coordinates.
static SVECTOR _gHypervelocityHingeModelVerts[] = {
#include "assets/hypervelocity_model_03550_verts.inc"
};

/// Hypervelocity hinged-component lighting normals, with twelve fractional bits.
static SVECTOR _gHypervelocityHingeModelNormals[] = {
#include "assets/hypervelocity_model_03550_normals.inc"
};

/// Writable commands for the hinge model's FT4 and ten GT4s.
///
/// Model creation replaces the commands' handler slots in place.
static u32 _gHypervelocityHingeModelStream[] = {
#include "assets/hypervelocity_model_03550_stream.inc"
};

TmdSource gHypervelocityHingeModel = {
    .handlersResolved    = 0,
    .bufferHalfBytes     = sizeof(POLY_FT4) + 10 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gHypervelocityHingeModelSkeleton),
    .partVertexCounts    = _gHypervelocityHingeModelPartVerts,
    .verts               = _gHypervelocityHingeModelVerts,
    .normals             = _gHypervelocityHingeModelNormals,
    .skeleton            = _gHypervelocityHingeModelSkeleton,
    .stream              = _gHypervelocityHingeModelStream,
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityHingeModelPartVerts) == ARRAY_SIZE(_gHypervelocityHingeModelSkeleton), hypervelocity_hinge_model_part_counts);

/// Idle track encoding-1 poses: local translations and 1/4096-turn Euler angles.
static AnimationPackedPose _gHypervelocityIdleAnimationBank1[] = {
#include "assets/hypervelocity_animation_03854_bank1.inc"
};

/// Idle track encoding-4 rotations, packed in eight-unit angular steps.
static AnimationPackedRotation _gHypervelocityIdleAnimationBank4[] = {
#include "assets/hypervelocity_animation_03854_bank4.inc"
};

/// Idle keyframes and loop controls, addressed by absolute record indices.
static AnimationRecord _gHypervelocityIdleAnimationRecords[] = {
#include "assets/hypervelocity_animation_03854_records.inc"
};

/// Part-track starts in `_gHypervelocityIdleAnimationRecords`.
static u16 _gHypervelocityIdleAnimationIndices[] = {
#include "assets/hypervelocity_animation_03854_indices.inc"
};

/// Standing pose of the player using the Hypervelocity animation bank.
///
/// Set 1 supplies ordinary idle; bank slots 21, 22 and 41 reuse the same set.
/// Each part holds a fixed pose through a two-frame loop. Track starts and
/// pose offsets refer to the accompanying private arrays;
/// all remain loaded while player animation slots borrow them.
static AnimationSet _gHypervelocityIdleAnimation = {
    .records           = _gHypervelocityIdleAnimationRecords,
    .trackStartIndices = _gHypervelocityIdleAnimationIndices,
    .poseBanks         = { [1] = _gHypervelocityIdleAnimationBank1, [4] = _gHypervelocityIdleAnimationBank4 },
};

/// Unarmed-walk encoding-1 poses: local translations and 1/4096-turn Euler angles.
static AnimationPackedPose _gHypervelocityUnarmedWalkAnimationBank1[] = {
#include "assets/hypervelocity_animation_03EF8_bank1.inc"
};

/// Unarmed-walk encoding-4 rotations, packed in eight-unit angular steps.
static AnimationPackedRotation _gHypervelocityUnarmedWalkAnimationBank4[] = {
#include "assets/hypervelocity_animation_03EF8_bank4.inc"
};

/// Unarmed-walk keyframes, cues and loop controls in normal-rate frames.
static AnimationRecord _gHypervelocityUnarmedWalkAnimationRecords[] = {
#include "assets/hypervelocity_animation_03EF8_records.inc"
};

/// Part-track starts in `_gHypervelocityUnarmedWalkAnimationRecords`.
static u16 _gHypervelocityUnarmedWalkAnimationIndices[] = {
#include "assets/hypervelocity_animation_03EF8_indices.inc"
};

/// Player walk used when no held-weapon model is present in equipment slot 1.
///
/// Set 19 in the Hypervelocity bank supplies the player's child animation slots.
/// Moving part tracks repeat 29-frame loops after their initial keyframes;
/// fixed tracks hold a two-frame loop. Track starts and pose offsets remain
/// within this set's private arrays, which must stay loaded during playback.
static AnimationSet _gHypervelocityUnarmedWalkAnimation = {
    .records           = _gHypervelocityUnarmedWalkAnimationRecords,
    .trackStartIndices = _gHypervelocityUnarmedWalkAnimationIndices,
    .poseBanks         = { [1] = _gHypervelocityUnarmedWalkAnimationBank1, [4] = _gHypervelocityUnarmedWalkAnimationBank4 },
};

// Immutable payloads and their unused descriptor retain the serialized data-section layout.
/// Translation/rotation poses for `_gHypervelocityUnusedBackwardWalkAnimation`.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityUnusedBackwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_047D0_bank1.inc"
};

/// Rotation-only poses for `_gHypervelocityUnusedBackwardWalkAnimation`.
///
/// Tracks 2..18 use eight-angle-unit steps and retain their local translations.
static const AnimationPackedRotation _gHypervelocityUnusedBackwardWalkAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_047D0_bank4.inc"
};

/// Keyframes, paired footstep cues and loop controls for the unused backward walk.
///
/// Durations count normal-rate frames; loop targets are absolute record indices.
/// Each track's loop bypasses its following stop record.
static const AnimationRecord _gHypervelocityUnusedBackwardWalkAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_047D0_records.inc"
};

/// Record starts for the unused backward walk's model-part tracks 0..18.
///
/// The final zero halfword is retained serialized data outside the track domain.
static const u16 _gHypervelocityUnusedBackwardWalkAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_047D0_indices.inc"
};

/// Pose-bank selectors for the player clips' two supported track encodings.
enum {
    HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Player backward-walk clip retained outside the Hypervelocity animation bank.
///
/// No bank slot selects this clip. Its moving part tracks repeat a 33-frame
/// cycle after their entry keyframes; tracks 0 and 12 hold two-frame loops.
/// Track 1 carries the paired footstep cues. Tracks 0 and 1 carry translation
/// and rotation; tracks 2..18 carry rotation only. Track starts, control targets
/// and complete pose reads fit the accompanying private arrays. Playback would
/// borrow the arrays for as long as any slot or cached record refers to them.
static const AnimationSet _gHypervelocityUnusedBackwardWalkAnimation __attribute__((section(".data"), aligned(4))) = {
    .records           = _gHypervelocityUnusedBackwardWalkAnimationRecords,
    .trackStartIndices = _gHypervelocityUnusedBackwardWalkAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityUnusedBackwardWalkAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityUnusedBackwardWalkAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityUnusedBackwardWalkAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_unused_backward_walk_animation_track_starts);

// Immutable clip payloads retain the exported package's data-section layout.
/// Negative-turn encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityNegativeTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05030_bank1.inc"
};

/// Negative-turn encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityNegativeTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05030_bank4.inc"
};

/// Negative-turn keyframes and absolute-index loops in normal-rate frames.
static const AnimationRecord _gHypervelocityNegativeTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05030_records.inc"
};

/// Nineteen negative-turn track starts and the unused exported trailing halfword.
static const u16 _gHypervelocityNegativeTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05030_indices.inc"
};

/// Player turn toward decreasing yaw in Hypervelocity bank slot 5.
///
/// Moving tracks repeat a 32-frame cycle after their entry keyframes; fixed
/// tracks hold a two-frame loop. Tracks 0 and 1 carry translation and rotation,
/// and tracks 2..18 carry rotation only. Playback borrows these private arrays,
/// which must remain loaded while any slot or cached record refers to them.
static AnimationSet _gHypervelocityNegativeTurnAnimation = {
    .records           = _gHypervelocityNegativeTurnAnimationRecords,
    .trackStartIndices = _gHypervelocityNegativeTurnAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityNegativeTurnAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityNegativeTurnAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityNegativeTurnAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_negative_turn_animation_track_starts);

/// Positive-turn encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityPositiveTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05894_bank1.inc"
};

/// Positive-turn encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityPositiveTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05894_bank4.inc"
};

/// Positive-turn keyframes and absolute-index loops in normal-rate frames.
static const AnimationRecord _gHypervelocityPositiveTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05894_records.inc"
};

/// Nineteen positive-turn track starts and the unused exported trailing halfword.
static const u16 _gHypervelocityPositiveTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05894_indices.inc"
};

/// Player turn toward increasing yaw in Hypervelocity bank slot 6.
///
/// Moving tracks repeat a 32-frame cycle after their entry keyframes; fixed
/// tracks hold a two-frame loop. Tracks 0 and 1 carry translation and rotation,
/// and tracks 2..18 carry rotation only. Playback borrows these private arrays,
/// which must remain loaded while any slot or cached record refers to them.
static AnimationSet _gHypervelocityPositiveTurnAnimation = {
    .records           = _gHypervelocityPositiveTurnAnimationRecords,
    .trackStartIndices = _gHypervelocityPositiveTurnAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityPositiveTurnAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityPositiveTurnAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityPositiveTurnAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_positive_turn_animation_track_starts);

/// Item-use encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05BA8_bank1.inc"
};

/// Item-use encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05BA8_bank4.inc"
};

/// Item-use keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gHypervelocityItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05BA8_records.inc"
};

/// Nineteen item-use track starts and the unused exported trailing halfword.
static const u16 _gHypervelocityItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_05BA8_indices.inc"
};

/// Player's scripted item-use motion in Hypervelocity bank slot 40.
///
/// All tracks stop at their final keyframe. Tracks 0 and 1 carry translation
/// and rotation, and tracks 2..18 carry rotation only. The player blends into
/// this clip and resumes aim locomotion when it finishes. Playback borrows the
/// private arrays, which must stay loaded while any slot or record refers to them.
static AnimationSet _gHypervelocityItemUseAnimation = {
    .records           = _gHypervelocityItemUseAnimationRecords,
    .trackStartIndices = _gHypervelocityItemUseAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityItemUseAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityItemUseAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityItemUseAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_item_use_animation_track_starts);

/// Stair-ascent encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06304_bank1.inc"
};

/// Stair-ascent encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06304_bank4.inc"
};

/// Stair-ascent keyframes, footstep cues and loop controls in normal-rate frames.
static const AnimationRecord _gHypervelocityStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06304_records.inc"
};

/// Nineteen stair-ascent track starts and the unused exported trailing halfword.
static const u16 _gHypervelocityStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06304_indices.inc"
};

/// Player's scripted stair-ascent cycle in Hypervelocity bank slot 36.
///
/// Moving tracks repeat 32-frame loops; fixed tracks hold two-frame loops.
/// Track 1 carries the footstep cues that gate stair strides. Tracks 0 and 1
/// carry translation and rotation, and tracks 2..18 carry rotation only.
/// Playback borrows the private arrays, which must remain loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityStairAscentAnimation = {
    .records           = _gHypervelocityStairAscentAnimationRecords,
    .trackStartIndices = _gHypervelocityStairAscentAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityStairAscentAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityStairAscentAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityStairAscentAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_stair_ascent_animation_track_starts);

/// Stair-descent encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06A8C_bank1.inc"
};

/// Stair-descent encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06A8C_bank4.inc"
};

/// Stair-descent keyframes, footstep cues and loop controls in normal-rate frames.
static const AnimationRecord _gHypervelocityStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06A8C_records.inc"
};

/// Nineteen stair-descent track starts and the unused exported trailing halfword.
static const u16 _gHypervelocityStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06A8C_indices.inc"
};

/// Player's scripted stair-descent cycle in Hypervelocity bank slot 37.
///
/// Moving tracks repeat 30-frame loops; fixed tracks hold two-frame loops.
/// Track 1 carries the footstep cues that gate stair strides. Tracks 0 and 1
/// carry translation and rotation, and tracks 2..18 carry rotation only.
/// Playback borrows the private arrays, which must remain loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityStairDescentAnimation = {
    .records           = _gHypervelocityStairDescentAnimationRecords,
    .trackStartIndices = _gHypervelocityStairDescentAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityStairDescentAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityStairDescentAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityStairDescentAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_stair_descent_animation_track_starts);

/// Odd-step stair-finish encoding-1 poses in local units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06D60_bank1.inc"
};

/// Odd-step stair-finish encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06D60_bank4.inc"
};

/// Odd-step stair-finish keyframes, footstep cue and stops in normal-rate frames.
static const AnimationRecord _gHypervelocityStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06D60_records.inc"
};

/// Nineteen odd-step stair-finish track starts and the unused exported halfword.
static const u16 _gHypervelocityStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_06D60_indices.inc"
};

/// Player's stair-ascent finish for an odd initial step count, in bank slot 39.
///
/// All tracks stop at their final keyframe; track 1 carries a footstep cue.
/// Tracks 0 and 1 carry translation and rotation, and tracks 2..18 carry
/// rotation only. Playback borrows the private arrays, which must remain
/// loaded while any slot or cached record refers to them.
static AnimationSet _gHypervelocityStairAscentFinishOddAnimation = {
    .records           = _gHypervelocityStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gHypervelocityStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityStairAscentFinishOddAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityStairAscentFinishOddAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityStairAscentFinishOddAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_stair_ascent_finish_odd_animation_track_starts);

/// Translation and rotation poses for the stair-ascent finish after an even initial step count.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_070F0_bank1.inc"
};

/// Packed rotations for the stair-ascent finish after an even initial step count.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_070F0_bank4.inc"
};

/// Keyframes and controls for the stair-ascent finish after an even initial step count.
///
/// Durations count normal-rate frames; every track stops at its final pose.
/// Track 1 carries one footstep cue.
static const AnimationRecord _gHypervelocityStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_070F0_records.inc"
};

/// Track-start record indices for the stair-ascent finish after an even initial step count.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_070F0_indices.inc"
};

/// Player stair-ascent finish for an even initial step count, in bank slot 38.
///
/// All tracks stop at their final keyframe; track 1 carries a footstep cue.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityStairAscentFinishEvenAnimation = {
    .records           = _gHypervelocityStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gHypervelocityStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityStairAscentFinishEvenAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityStairAscentFinishEvenAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityStairAscentFinishEvenAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_stair_ascent_finish_even_animation_track_starts);

/// Translation and rotation poses for Parasite Energy family 0 release.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily0ReleaseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07578_bank1.inc"
};

/// Packed rotations for Parasite Energy family 0 release.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily0ReleaseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07578_bank4.inc"
};

/// Keyframes and controls for Parasite Energy family 0 release.
///
/// Durations count normal-rate frames; every track stops at its final pose.
/// No records carry action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily0ReleaseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07578_records.inc"
};

/// Track-start record indices for Parasite Energy family 0 release.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityParasiteEnergyFamily0ReleaseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07578_indices.inc"
};

/// Player Parasite Energy family 0 release clip, in bank slot 26.
///
/// A six-frame blend starts child tracks 1..18; their final stops advance
/// the action to charging. No records carry action cues.
/// Family 0 serves spell IDs 300..600 except decimal tens digit 3, and
/// item IDs at least 601 whose decimal tens digit is 1.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily0ReleaseAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily0ReleaseAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily0ReleaseAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily0ReleaseAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily0ReleaseAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily0ReleaseAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family0_release_animation_track_starts);

/// Translation and rotation poses for Parasite Energy family 0 charge.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily0ChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07774_bank1.inc"
};

/// Packed rotations for Parasite Energy family 0 charge.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily0ChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07774_bank4.inc"
};

/// Keyframes and controls for Parasite Energy family 0 charge.
///
/// Durations count normal-rate frames; every track loops two keyframes.
/// Each loop bypasses its following stop record; no records carry action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily0ChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07774_records.inc"
};

/// Track-start record indices for Parasite Energy family 0 charge.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityParasiteEnergyFamily0ChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07774_indices.inc"
};

/// Player Parasite Energy family 0 charge clip, in bank slot 27.
///
/// Child tracks 1..18 loop while the attachment countdown runs; the cast
/// clip replaces them when it reaches zero. No records carry action cues.
/// Family 0 serves spell IDs 300..600 except decimal tens digit 3, and
/// item IDs at least 601 whose decimal tens digit is 1.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily0ChargeAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily0ChargeAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily0ChargeAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily0ChargeAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily0ChargeAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily0ChargeAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family0_charge_animation_track_starts);

/// Translation and rotation poses for Parasite Energy family 0 cast.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily0CastAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07A4C_bank1.inc"
};

/// Packed rotations for Parasite Energy family 0 cast.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily0CastAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07A4C_bank4.inc"
};

/// Keyframes and controls for Parasite Energy family 0 cast.
///
/// Durations count normal-rate frames; every track stops at its final pose.
/// No records carry action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily0CastAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07A4C_records.inc"
};

/// Track-start record indices for Parasite Energy family 0 cast.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityParasiteEnergyFamily0CastAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07A4C_indices.inc"
};

/// Player Parasite Energy family 0 cast clip, in bank slot 28.
///
/// Starts child tracks 1..18 when the attachment countdown reaches zero.
/// Their final stops resume the previous control state; there are no action cues.
/// Family 0 serves spell IDs 300..600 except decimal tens digit 3, and
/// item IDs at least 601 whose decimal tens digit is 1.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily0CastAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily0CastAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily0CastAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily0CastAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily0CastAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily0CastAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family0_cast_animation_track_starts);

/// Translation and rotation poses for Parasite Energy family 1 release.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily1ReleaseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07CF0_bank1.inc"
};

/// Packed rotations for Parasite Energy family 1 release.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily1ReleaseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07CF0_bank4.inc"
};

/// Keyframes and controls for Parasite Energy family 1 release.
///
/// Durations count normal-rate frames; every track stops at its final pose.
/// No records carry action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily1ReleaseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07CF0_records.inc"
};

/// Track-start record indices for Parasite Energy family 1 release.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityParasiteEnergyFamily1ReleaseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07CF0_indices.inc"
};

/// Player Parasite Energy family 1 release clip, in bank slot 29.
///
/// A six-frame blend starts child tracks 1..18; their final stops advance
/// the action to charging. No records carry action cues.
/// Family 1 serves spell IDs below 300 except decimal tens digit 3, and
/// item IDs at least 601 whose decimal tens digit differs from 1.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily1ReleaseAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily1ReleaseAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily1ReleaseAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily1ReleaseAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily1ReleaseAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily1ReleaseAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family1_release_animation_track_starts);

/// Translation and rotation poses for Parasite Energy family 1 charge.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily1ChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07EF0_bank1.inc"
};

/// Packed rotations for Parasite Energy family 1 charge.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily1ChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07EF0_bank4.inc"
};

/// Keyframes and controls for Parasite Energy family 1 charge.
///
/// Durations count normal-rate frames; every track loops two keyframes.
/// Each loop bypasses its following stop record; no records carry action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily1ChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07EF0_records.inc"
};

/// Track-start record indices for Parasite Energy family 1 charge.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityParasiteEnergyFamily1ChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_07EF0_indices.inc"
};

/// Player Parasite Energy family 1 charge clip, in bank slot 30.
///
/// Child tracks 1..18 loop while the attachment countdown runs; the cast
/// clip replaces them when it reaches zero. No records carry action cues.
/// Family 1 serves spell IDs below 300 except decimal tens digit 3, and
/// item IDs at least 601 whose decimal tens digit differs from 1.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily1ChargeAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily1ChargeAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily1ChargeAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily1ChargeAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily1ChargeAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily1ChargeAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family1_charge_animation_track_starts);

/// Translation and rotation poses for Parasite Energy family 1 casting.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily1CastAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08244_bank1.inc"
};

/// Packed rotations for Parasite Energy family 1 casting.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily1CastAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08244_bank4.inc"
};

/// Keyframes and controls for Parasite Energy family 1 casting.
///
/// Durations count normal-rate frames. Every track ends in a held pose;
/// no records carry action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily1CastAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08244_records.inc"
};

/// Track-start record indices for Parasite Energy family 1 casting.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityParasiteEnergyFamily1CastAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08244_indices.inc"
};

/// Player Parasite Energy family 1 casting clip, in bank slot 31.
///
/// Child tracks replace the charge clip when the attachment countdown reaches zero.
/// Family 1 serves spell IDs below 300 except decimal tens digit 3, and
/// item IDs at least 601 whose decimal tens digit differs from 1.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily1CastAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily1CastAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily1CastAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily1CastAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily1CastAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily1CastAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family1_cast_animation_track_starts);

/// Translation and rotation poses for a hit on model part 4.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityPart4HitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_084F8_bank1.inc"
};

/// Packed rotations for a hit on model part 4.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityPart4HitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_084F8_bank4.inc"
};

/// Keyframes and controls for a hit on model part 4.
///
/// Durations count normal-rate frames. Every track ends in a held pose;
/// no records carry action cues.
static const AnimationRecord _gHypervelocityPart4HitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_084F8_records.inc"
};

/// Track-start record indices for a hit on model part 4.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityPart4HitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_084F8_indices.inc"
};

/// Player hit reaction for model part 4, in bank slot 16.
///
/// Hit region 1 selects this reaction with a three-frame blend; completion
/// returns the player to its preceding aim or locomotion state.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityPart4HitAnimation = {
    .records           = _gHypervelocityPart4HitAnimationRecords,
    .trackStartIndices = _gHypervelocityPart4HitAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityPart4HitAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityPart4HitAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityPart4HitAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_part4_hit_animation_track_starts);

/// Translation and rotation poses for a hit on the other body region.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityOtherBodyHitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08818_bank1.inc"
};

/// Packed rotations for a hit on the other body region.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityOtherBodyHitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08818_bank4.inc"
};

/// Keyframes and controls for a hit on the other body region.
///
/// Durations count normal-rate frames. Every track ends in a held pose;
/// no records carry action cues.
static const AnimationRecord _gHypervelocityOtherBodyHitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08818_records.inc"
};

/// Track-start record indices for a hit on the other body region.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityOtherBodyHitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08818_indices.inc"
};

/// Player hit reaction for the other body region, in bank slot 17.
///
/// A body hit outside model part 4 selects this reaction with a three-frame
/// blend; completion returns the player to its preceding aim or locomotion state.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityOtherBodyHitAnimation = {
    .records           = _gHypervelocityOtherBodyHitAnimationRecords,
    .trackStartIndices = _gHypervelocityOtherBodyHitAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityOtherBodyHitAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityOtherBodyHitAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityOtherBodyHitAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_other_body_hit_animation_track_starts);

/// Translation and rotation poses for the stopped damage pose.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityStoppedPoseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08FDC_bank1.inc"
};

/// Packed rotations for the stopped damage pose.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityStoppedPoseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08FDC_bank4.inc"
};

/// Keyframes and controls for the stopped damage pose.
///
/// Durations count normal-rate frames. Every track ends in a held pose;
/// no records carry action cues.
static const AnimationRecord _gHypervelocityStoppedPoseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08FDC_records.inc"
};

/// Track-start record indices for the stopped damage pose.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityStoppedPoseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_08FDC_indices.inc"
};

/// Player stopped damage pose clip, in bank slot 18.
///
/// Zero HP selects this clip with movement and turning stopped.
/// The final pose holds until another clip is requested.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityStoppedPoseAnimation = {
    .records           = _gHypervelocityStoppedPoseAnimationRecords,
    .trackStartIndices = _gHypervelocityStoppedPoseAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityStoppedPoseAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityStoppedPoseAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityStoppedPoseAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_stopped_pose_animation_track_starts);

/// Translation and rotation poses for idle above half maximum HP.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityHighHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0A274_bank1.inc"
};

/// Packed rotations for idle above half maximum HP.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityHighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0A274_bank4.inc"
};

/// Keyframes and controls for idle above half maximum HP.
///
/// Durations count normal-rate frames. Each child track plays an introduction,
/// then loops a suffix indefinitely.
/// Each loop bypasses its following stop record; no records carry action cues.
static const AnimationRecord _gHypervelocityHighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0A274_records.inc"
};

/// Track-start record indices for idle above half maximum HP.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityHighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0A274_indices.inc"
};

/// Player idle clip above half maximum HP, in bank slot 23.
///
/// Selected after 300 direction-free idle ticks when HP is above half maximum.
/// Child tracks play an introduction and then repeat their idle suffix.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityHighHealthIdleAnimation = {
    .records           = _gHypervelocityHighHealthIdleAnimationRecords,
    .trackStartIndices = _gHypervelocityHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityHighHealthIdleAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityHighHealthIdleAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityHighHealthIdleAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_high_health_idle_animation_track_starts);

/// Translation and rotation poses for idle at or below one quarter maximum HP.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityLowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ADEC_bank1.inc"
};

/// Packed rotations for idle at or below one quarter maximum HP.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ADEC_bank4.inc"
};

/// Keyframes and controls for idle at or below one quarter maximum HP.
///
/// Durations count normal-rate frames. Each track loops, bypassing its following
/// stop record; no records carry action cues.
/// Moving child tracks play an introduction before their repeated suffix.
static const AnimationRecord _gHypervelocityLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ADEC_records.inc"
};

/// Track-start record indices for idle at or below one quarter maximum HP.
///
/// Entries 0..18 select model-part tracks; the last halfword is retained serialized data.
static const u16 _gHypervelocityLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ADEC_indices.inc"
};

/// Player idle clip at or below one quarter maximum HP, in bank slot 25.
///
/// Selected after 300 direction-free idle ticks when HP is at most one quarter
/// maximum. Paralysis episodes also select this clip for all active child tracks.
/// Moving tracks play an introduction and then repeat their idle suffix.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityLowHealthIdleAnimation = {
    .records           = _gHypervelocityLowHealthIdleAnimationRecords,
    .trackStartIndices = _gHypervelocityLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityLowHealthIdleAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityLowHealthIdleAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityLowHealthIdleAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_low_health_idle_animation_track_starts);

/// Translation and rotation poses for `_gHypervelocityMidHealthIdleAnimation`.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityMidHealthIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B508_bank1.inc"
};

/// Packed rotation poses for `_gHypervelocityMidHealthIdleAnimation`.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityMidHealthIdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B508_bank4.inc"
};

/// Keyframes and controls for `_gHypervelocityMidHealthIdleAnimation`.
///
/// Durations count normal-rate frames. Each track loops before its stop record;
/// moving tracks play an introduction before their repeated suffix. No action cues.
static const AnimationRecord _gHypervelocityMidHealthIdleAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B508_records.inc"
};

/// Track-start record indices for `_gHypervelocityMidHealthIdleAnimation`.
///
/// Entries 0..18 select model-part tracks; the final zero halfword is retained
/// serialized data, outside the nineteen-track playback domain.
static const u16 _gHypervelocityMidHealthIdleAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B508_indices.inc"
};

/// Player idle clip above one quarter and at or below half maximum HP, in slot 24.
///
/// Selected after 300 direction-free idle ticks in the middle health band.
/// Moving tracks play an introduction before repeating a 73-frame suffix.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityMidHealthIdleAnimation = {
    .records           = _gHypervelocityMidHealthIdleAnimationRecords,
    .trackStartIndices = _gHypervelocityMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityMidHealthIdleAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityMidHealthIdleAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityMidHealthIdleAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_mid_health_idle_animation_track_starts);

/// Translation and rotation poses for `_gHypervelocityEventEntryAnimation`.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityEventEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B988_bank1.inc"
};

/// Packed rotation poses for `_gHypervelocityEventEntryAnimation`.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityEventEntryAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B988_bank4.inc"
};

/// Keyframes and controls for `_gHypervelocityEventEntryAnimation`.
///
/// Durations count normal-rate frames. Child tracks run for 30 frames and stop;
/// their final poses hold until another clip is requested. No action cues.
static const AnimationRecord _gHypervelocityEventEntryAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B988_records.inc"
};

/// Track-start record indices for `_gHypervelocityEventEntryAnimation`.
///
/// Entries 0..18 select model-part tracks; the final zero halfword is retained
/// serialized data, outside the nineteen-track playback domain.
static const u16 _gHypervelocityEventEntryAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0B988_indices.inc"
};

/// Player entry presentation for scripted events, in bank slots 32 and 34.
///
/// Scripted presentation with a zero variant selects slot 32 before CAP playback.
/// Child tracks animate for 30 normal-rate frames, then hold their final poses.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityEventEntryAnimation = {
    .records           = _gHypervelocityEventEntryAnimationRecords,
    .trackStartIndices = _gHypervelocityEventEntryAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityEventEntryAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityEventEntryAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityEventEntryAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_event_entry_animation_track_starts);

/// Translation and rotation poses for `_gHypervelocityEventExitAnimation`.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityEventExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0BB60_bank1.inc"
};

/// Packed rotation poses for `_gHypervelocityEventExitAnimation`.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityEventExitAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0BB60_bank4.inc"
};

/// Keyframes and controls for `_gHypervelocityEventExitAnimation`.
///
/// Durations count normal-rate frames. Child tracks run for 13 frames and stop;
/// their final poses hold until another clip is requested. No action cues.
static const AnimationRecord _gHypervelocityEventExitAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0BB60_records.inc"
};

/// Track-start record indices for `_gHypervelocityEventExitAnimation`.
///
/// Entries 0..18 select model-part tracks; the final zero halfword is retained
/// serialized data, outside the nineteen-track playback domain.
static const u16 _gHypervelocityEventExitAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0BB60_indices.inc"
};

/// Player exit presentation for scripted events, in bank slots 33 and 35.
///
/// Scripted presentation with a nonzero variant selects slot 33 after CAP playback.
/// Child tracks animate for 13 normal-rate frames, then hold their final poses.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityEventExitAnimation = {
    .records           = _gHypervelocityEventExitAnimationRecords,
    .trackStartIndices = _gHypervelocityEventExitAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityEventExitAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityEventExitAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityEventExitAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_event_exit_animation_track_starts);

/// Translation and rotation poses for `_gHypervelocityParasiteEnergyFamily2ReleaseAnimation`.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily2ReleaseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C0C4_bank1.inc"
};

/// Packed rotation poses for `_gHypervelocityParasiteEnergyFamily2ReleaseAnimation`.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily2ReleaseAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C0C4_bank4.inc"
};

/// Keyframes and controls for `_gHypervelocityParasiteEnergyFamily2ReleaseAnimation`.
///
/// Durations count normal-rate frames. Moving child tracks run for 21 frames
/// and stop at their final poses. No action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily2ReleaseAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C0C4_records.inc"
};

/// Track-start record indices for `_gHypervelocityParasiteEnergyFamily2ReleaseAnimation`.
///
/// Entries 0..18 select model-part tracks; the final zero halfword is retained
/// serialized data, outside the nineteen-track playback domain.
static const u16 _gHypervelocityParasiteEnergyFamily2ReleaseAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C0C4_indices.inc"
};

/// Player Parasite Energy family-2 release clip, in bank slot 42.
///
/// Selected for spell IDs below 601 whose decimal tens digit is 3.
/// Starts with the attachment release countdown; moving child tracks run for
/// 21 normal-rate frames and hold before the charge clip starts.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily2ReleaseAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily2ReleaseAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily2ReleaseAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily2ReleaseAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily2ReleaseAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily2ReleaseAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family2_release_animation_track_starts);

/// Translation and rotation poses for `_gHypervelocityParasiteEnergyFamily2ChargeAnimation`.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily2ChargeAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C2B8_bank1.inc"
};

/// Packed rotation poses for `_gHypervelocityParasiteEnergyFamily2ChargeAnimation`.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily2ChargeAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C2B8_bank4.inc"
};

/// Keyframes and controls for `_gHypervelocityParasiteEnergyFamily2ChargeAnimation`.
///
/// Durations count normal-rate frames. Each track loops two one-frame keyframes,
/// bypassing the following stop record. No action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily2ChargeAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C2B8_records.inc"
};

/// Track-start record indices for `_gHypervelocityParasiteEnergyFamily2ChargeAnimation`.
///
/// Entries 0..18 select model-part tracks; the final zero halfword is retained
/// serialized data, outside the nineteen-track playback domain.
static const u16 _gHypervelocityParasiteEnergyFamily2ChargeAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C2B8_indices.inc"
};

/// Player Parasite Energy family-2 charge loop, in bank slot 43.
///
/// Selected for spell IDs below 601 whose decimal tens digit is 3.
/// Repeats a two-frame pose loop until the attachment countdown reaches zero
/// and the cast clip starts.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily2ChargeAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily2ChargeAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily2ChargeAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily2ChargeAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily2ChargeAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily2ChargeAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family2_charge_animation_track_starts);

/// Translation and rotation poses for `_gHypervelocityParasiteEnergyFamily2CastAnimation`.
///
/// Tracks 0 and 1 use local model units and 4096 angle units per turn.
static const AnimationPackedPose _gHypervelocityParasiteEnergyFamily2CastAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C738_bank1.inc"
};

/// Packed rotation poses for `_gHypervelocityParasiteEnergyFamily2CastAnimation`.
///
/// Tracks 2..18 use 512 angle steps per turn and retain their local translations.
static const AnimationPackedRotation _gHypervelocityParasiteEnergyFamily2CastAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C738_bank4.inc"
};

/// Keyframes and controls for `_gHypervelocityParasiteEnergyFamily2CastAnimation`.
///
/// Durations count normal-rate frames. Moving child tracks run for 41 frames
/// and stop at their final poses. No action cues.
static const AnimationRecord _gHypervelocityParasiteEnergyFamily2CastAnimationRecords[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C738_records.inc"
};

/// Track-start record indices for `_gHypervelocityParasiteEnergyFamily2CastAnimation`.
///
/// Entries 0..18 select model-part tracks; the final zero halfword is retained
/// serialized data, outside the nineteen-track playback domain.
static const u16 _gHypervelocityParasiteEnergyFamily2CastAnimationIndices[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/hypervelocity_animation_0C738_indices.inc"
};

/// Player Parasite Energy family-2 cast clip, in bank slot 44.
///
/// Selected for spell IDs below 601 whose decimal tens digit is 3.
/// Starts when the attachment countdown reaches zero. Moving child tracks run
/// for 41 normal-rate frames and hold while normal control resumes.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows these word-aligned arrays, which must remain loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityParasiteEnergyFamily2CastAnimation = {
    .records           = _gHypervelocityParasiteEnergyFamily2CastAnimationRecords,
    .trackStartIndices = _gHypervelocityParasiteEnergyFamily2CastAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityParasiteEnergyFamily2CastAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityParasiteEnergyFamily2CastAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityParasiteEnergyFamily2CastAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_parasite_energy_family2_cast_animation_track_starts);

/// Aim-hold encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityAimHoldAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0C914_bank1.inc"
};

/// Aim-hold encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityAimHoldAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0C914_bank4.inc"
};

/// Aim-hold keyframes, absolute-index loops and retained trailing stop records.
static const AnimationRecord _gHypervelocityAimHoldAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0C914_records.inc"
};

/// Nineteen aim-hold track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityAimHoldAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0C914_indices.inc"
};

/// Stationary aiming pose of the player in Hypervelocity bank slot 9.
///
/// Every track repeats a fixed pose through a two-frame loop. Tracks 0 and 1
/// carry translation and rotation; tracks 2..18 carry rotation only. Playback
/// borrows the private arrays, which must stay loaded while any slot or cached
/// record refers to them. Immutable payloads retain their original data section.
static AnimationSet _gHypervelocityAimHoldAnimation = {
    .records           = _gHypervelocityAimHoldAnimationRecords,
    .trackStartIndices = _gHypervelocityAimHoldAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityAimHoldAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityAimHoldAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityAimHoldAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_aim_hold_animation_track_starts);

/// Backward-aim/turn encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityAimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0CDB4_bank1.inc"
};

/// Backward-aim/turn encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityAimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0CDB4_bank4.inc"
};

/// Backward-aim/turn keyframes, footstep cues, loops and trailing stop records.
static const AnimationRecord _gHypervelocityAimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0CDB4_records.inc"
};

/// Nineteen backward-aim/turn track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityAimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0CDB4_indices.inc"
};

/// Player's backward movement and stationary turning while aiming, in Hypervelocity bank slot 13.
///
/// Moving tracks repeat a 34-frame cycle after their entry keyframes; tracks
/// 0 and 12 hold fixed two-frame loops. Track 1 supplies the paired footstep
/// cues. Tracks 0 and 1 carry translation and rotation, and tracks 2..18 carry
/// rotation only. Playback borrows the private arrays, which must stay loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityAimBackwardOrTurnAnimation = {
    .records           = _gHypervelocityAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gHypervelocityAimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityAimBackwardOrTurnAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityAimBackwardOrTurnAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityAimBackwardOrTurnAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_aim_backward_or_turn_animation_track_starts);

/// Forward-aim encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityAimForwardAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0D5C8_bank1.inc"
};

/// Forward-aim encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityAimForwardAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0D5C8_bank4.inc"
};

/// Forward-aim keyframes, footstep cues, loops and retained trailing stop records.
static const AnimationRecord _gHypervelocityAimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0D5C8_records.inc"
};

/// Nineteen forward-aim track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityAimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0D5C8_indices.inc"
};

/// Player's forward movement while aiming, in Hypervelocity bank slot 12.
///
/// Moving tracks repeat a 19-frame cycle after their entry keyframes; tracks
/// 0 and 9 hold fixed two-frame loops. Track 1 supplies the paired footstep
/// cues. Tracks 0 and 1 carry translation and rotation, and tracks 2..18 carry
/// rotation only. Playback borrows the private arrays, which must stay loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityAimForwardAnimation = {
    .records           = _gHypervelocityAimForwardAnimationRecords,
    .trackStartIndices = _gHypervelocityAimForwardAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityAimForwardAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityAimForwardAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityAimForwardAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_aim_forward_animation_track_starts);

/// Battle-end-reload encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityBattleEndReloadAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0DB10_bank1.inc"
};

/// Battle-end-reload encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityBattleEndReloadAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0DB10_bank4.inc"
};

/// Battle-end-reload keyframes and stop controls, with no ammunition-loading cues.
static const AnimationRecord _gHypervelocityBattleEndReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0DB10_records.inc"
};

/// Nineteen battle-end-reload track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityBattleEndReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0DB10_indices.inc"
};

/// Player motion selected by battle-end reload in Hypervelocity bank slot 20.
///
/// All tracks stop at their final keyframe; the clip supplies no reload cues.
/// Tracks 0 and 1 carry translation and rotation, and tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityBattleEndReloadAnimation = {
    .records           = _gHypervelocityBattleEndReloadAnimationRecords,
    .trackStartIndices = _gHypervelocityBattleEndReloadAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityBattleEndReloadAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityBattleEndReloadAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityBattleEndReloadAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_battle_end_reload_animation_track_starts);

/// Firing encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityFireAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E2F0_bank1.inc"
};

/// Firing encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityFireAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E2F0_bank4.inc"
};

/// Firing keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gHypervelocityFireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E2F0_records.inc"
};

/// Nineteen firing track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityFireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E2F0_indices.inc"
};

/// Player firing and recoil motion shared by Hypervelocity bank slots 10 and 11.
///
/// The charged shot restarts slot 11 when charging completes. All tracks stop
/// at their final keyframe; projectile timing is independent of this clip.
/// Tracks 0 and 1 carry translation and rotation, and tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityFireAnimation = {
    .records           = _gHypervelocityFireAnimationRecords,
    .trackStartIndices = _gHypervelocityFireAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityFireAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityFireAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityFireAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_fire_animation_track_starts);

/// Charge encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityChargeAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E838_bank1.inc"
};

/// Charge encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityChargeAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E838_bank4.inc"
};

/// Charge keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gHypervelocityChargeAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E838_records.inc"
};

/// Nineteen charge track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityChargeAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0E838_indices.inc"
};

/// Player's charging pose transition in Hypervelocity bank slot 14.
///
/// The attack blends into this clip at charge entry. Every track stops at its
/// final pose, which is held through the remaining charge time. Generic
/// primary-reload requests also select this slot, but it supplies no reload
/// cues. Tracks 0 and 1 carry translation and rotation, and tracks 2..18 carry
/// rotation only. Playback borrows the private arrays, which must stay loaded
/// while any slot or cached record refers to them.
static AnimationSet _gHypervelocityChargeAnimation = {
    .records           = _gHypervelocityChargeAnimationRecords,
    .trackStartIndices = _gHypervelocityChargeAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityChargeAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityChargeAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityChargeAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_charge_animation_track_starts);

/// Charge-cancellation encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityChargeCancelAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ED7C_bank1.inc"
};

/// Charge-cancellation encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityChargeCancelAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ED7C_bank4.inc"
};

/// Charge-cancellation keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gHypervelocityChargeCancelAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ED7C_records.inc"
};

/// Nineteen charge-cancellation track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityChargeCancelAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0ED7C_indices.inc"
};

/// Player motion for cancelling an unfinished Hypervelocity charge, in bank slot 15.
///
/// Releasing both attack buttons before firing blends into this clip. All
/// tracks stop at their final keyframe, and none supplies action cues. Generic
/// secondary-load reload requests also select this bank slot.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityChargeCancelAnimation = {
    .records           = _gHypervelocityChargeCancelAnimationRecords,
    .trackStartIndices = _gHypervelocityChargeCancelAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityChargeCancelAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityChargeCancelAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityChargeCancelAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_charge_cancel_animation_track_starts);

/// Aim-entry encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityAimEntryAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0F964_bank1.inc"
};

/// Aim-entry encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityAimEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0F964_bank4.inc"
};

/// Aim-entry keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gHypervelocityAimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0F964_records.inc"
};

/// Nineteen aim-entry track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityAimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_0F964_indices.inc"
};

/// Player's transition into aiming with the Hypervelocity, in bank slot 7.
///
/// All tracks stop at their final keyframe; ordinary aim entry then selects
/// the aim-hold clip. Enemy grab and hit recovery also borrow this set as the
/// player's recovery motion. The clip supplies no action cues.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityAimEntryAnimation = {
    .records           = _gHypervelocityAimEntryAnimationRecords,
    .trackStartIndices = _gHypervelocityAimEntryAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityAimEntryAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityAimEntryAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityAimEntryAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_aim_entry_animation_track_starts);

/// Aim-exit encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityAimExitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10198_bank1.inc"
};

/// Aim-exit encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityAimExitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10198_bank4.inc"
};

/// Aim-exit keyframes and stop controls; durations count normal-rate frames.
static const AnimationRecord _gHypervelocityAimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10198_records.inc"
};

/// Nineteen aim-exit track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityAimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10198_indices.inc"
};

/// Player's transition out of Hypervelocity aiming, in bank slot 8.
///
/// Used for ordinary aim exit and scripted attack completion. All tracks stop
/// at their final keyframe; the clip supplies no action cues.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityAimExitAnimation = {
    .records           = _gHypervelocityAimExitAnimationRecords,
    .trackStartIndices = _gHypervelocityAimExitAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityAimExitAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityAimExitAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityAimExitAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_aim_exit_animation_track_starts);

/// Armed-walk encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10A50_bank1.inc"
};

/// Armed-walk encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10A50_bank4.inc"
};

/// Armed-walk keyframes, footstep cues, loop controls and terminal stops.
///
/// Keyframe durations count normal-rate frames; jumps use absolute record indices.
static const AnimationRecord _gHypervelocityWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10A50_records.inc"
};

/// Nineteen armed-walk track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_10A50_indices.inc"
};

/// Player's forward walk with the Hypervelocity model held, in bank slot 2.
///
/// Moving tracks repeat 30-frame loops after their initial poses; fixed tracks
/// hold two-frame loops. Track 1's two cue bits select the paired footsteps.
/// Root displacement is applied separately by player movement.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityWalkAnimation = {
    .records           = _gHypervelocityWalkAnimationRecords,
    .trackStartIndices = _gHypervelocityWalkAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityWalkAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityWalkAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityWalkAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_walk_animation_track_starts);

/// Backward-walk encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityBackwardWalkAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_111F0_bank1.inc"
};

/// Backward-walk encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityBackwardWalkAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_111F0_bank4.inc"
};

/// Backward-walk keyframes, footstep cues, loop controls and terminal stops.
///
/// Keyframe durations count normal-rate frames; jumps use absolute record indices.
static const AnimationRecord _gHypervelocityBackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_111F0_records.inc"
};

/// Nineteen backward-walk track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityBackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_111F0_indices.inc"
};

/// Player's backward walk while using the Hypervelocity bank, in slot 3.
///
/// Moving tracks repeat 33-frame loops after their initial poses; fixed tracks
/// hold two-frame loops. Track 1's two cue bits select the paired footsteps.
/// Root displacement is applied separately by player movement.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityBackwardWalkAnimation = {
    .records           = _gHypervelocityBackwardWalkAnimationRecords,
    .trackStartIndices = _gHypervelocityBackwardWalkAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityBackwardWalkAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityBackwardWalkAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityBackwardWalkAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_backward_walk_animation_track_starts);

/// Run encoding-1 poses in local model units and 1/4096-turn angles.
static const AnimationPackedPose _gHypervelocityRunAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_11BE8_bank1.inc"
};

/// Run encoding-4 rotations in packed 1/512-turn steps.
static const AnimationPackedRotation _gHypervelocityRunAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_11BE8_bank4.inc"
};

/// Run keyframes, footstep cues, loop controls and terminal stops.
///
/// Keyframe durations count normal-rate frames; jumps use absolute record indices.
static const AnimationRecord _gHypervelocityRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_11BE8_records.inc"
};

/// Nineteen run track starts and the unused trailing zero halfword.
static const u16 _gHypervelocityRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/hypervelocity_animation_11BE8_indices.inc"
};

/// Player's forward run with the Hypervelocity bank, in slot 4.
///
/// Used by normal locomotion and scripted running. Moving tracks repeat
/// 21-frame loops after their initial poses; track 0 holds a two-frame loop.
/// Track 1's two cue bits select paired running footsteps. Root displacement
/// is applied separately by player movement.
/// Tracks 0 and 1 carry translation and rotation; tracks 2..18 carry rotation
/// only. Playback borrows the private arrays, which must stay loaded while any
/// slot or cached record refers to them.
static AnimationSet _gHypervelocityRunAnimation = {
    .records           = _gHypervelocityRunAnimationRecords,
    .trackStartIndices = _gHypervelocityRunAnimationIndices,
    .poseBanks         = {
        [HYPERVELOCITY_ANIMATION_TRANSLATION_ROTATION_BANK] = _gHypervelocityRunAnimationBank1,
        [HYPERVELOCITY_ANIMATION_PACKED_ROTATION_BANK]      = _gHypervelocityRunAnimationBank4,
    },
};
STATIC_ASSERT(ARRAY_SIZE(_gHypervelocityRunAnimationIndices) == GAME_ACTOR_NORMAL_ANIMATION_SLOTS + 1, hypervelocity_run_animation_track_starts);

AnimationBank gHypervelocityPlayerAnimationBank = {
    .table = {
        .sets = {
            NULL,
            &_gHypervelocityIdleAnimation,
            &_gHypervelocityWalkAnimation,
            &_gHypervelocityBackwardWalkAnimation,
            &_gHypervelocityRunAnimation,
            &_gHypervelocityNegativeTurnAnimation,
            &_gHypervelocityPositiveTurnAnimation,
            &_gHypervelocityAimEntryAnimation,
            &_gHypervelocityAimExitAnimation,
            &_gHypervelocityAimHoldAnimation,
            &_gHypervelocityFireAnimation,
            &_gHypervelocityFireAnimation,
            &_gHypervelocityAimForwardAnimation,
            &_gHypervelocityAimBackwardOrTurnAnimation,
            &_gHypervelocityChargeAnimation,
            &_gHypervelocityChargeCancelAnimation,
            &_gHypervelocityPart4HitAnimation,
            &_gHypervelocityOtherBodyHitAnimation,
            &_gHypervelocityStoppedPoseAnimation,
            &_gHypervelocityUnarmedWalkAnimation,
            &_gHypervelocityBattleEndReloadAnimation,
            &_gHypervelocityIdleAnimation,
            &_gHypervelocityIdleAnimation,
            &_gHypervelocityHighHealthIdleAnimation,
            &_gHypervelocityMidHealthIdleAnimation,
            &_gHypervelocityLowHealthIdleAnimation,
            &_gHypervelocityParasiteEnergyFamily0ReleaseAnimation,
            &_gHypervelocityParasiteEnergyFamily0ChargeAnimation,
            &_gHypervelocityParasiteEnergyFamily0CastAnimation,
            &_gHypervelocityParasiteEnergyFamily1ReleaseAnimation,
            &_gHypervelocityParasiteEnergyFamily1ChargeAnimation,
            &_gHypervelocityParasiteEnergyFamily1CastAnimation,
            &_gHypervelocityEventEntryAnimation,
            &_gHypervelocityEventExitAnimation,
            &_gHypervelocityEventEntryAnimation,
            &_gHypervelocityEventExitAnimation,
            &_gHypervelocityStairAscentAnimation,
            &_gHypervelocityStairDescentAnimation,
            &_gHypervelocityStairAscentFinishEvenAnimation,
            &_gHypervelocityStairAscentFinishOddAnimation,
            &_gHypervelocityItemUseAnimation,
            &_gHypervelocityIdleAnimation,
            &_gHypervelocityParasiteEnergyFamily2ReleaseAnimation,
            &_gHypervelocityParasiteEnergyFamily2ChargeAnimation,
            &_gHypervelocityParasiteEnergyFamily2CastAnimation,
            NULL,
            NULL,
        },
    },
};

/// Shared texture phases of the Hypervelocity trail segments and discharge walls.
///
/// Round initialization rewrites all sixteen signed halfwords with LCG values
/// 0..255. Both jet tails use every entry; discharge walls reuse entries 0 and 1.
/// Drawers select one of six 40-texel cells by (offset + active age) modulo six.
/// The table belongs to the loaded overlay, so firing another round also changes
/// any still-live discharge cones' phases. Initial zero values are valid phases.
static s16 _gHypervelocityConeTextureFrameOffsets[EFFECT_BAND_SEGMENT_COUNT] = { 0 };
STATIC_ASSERT(HYPERVELOCITY_DISCHARGE_CONE_WALL_COUNT <= ARRAY_SIZE(_gHypervelocityConeTextureFrameOffsets), hypervelocity_cone_wall_frame_offsets);
