#include "weapons/hypervelocity.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
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

#include "types.h"
/// Signed effect-age argument whose low bit selects one of the two flame cells.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/effects/sprite_quad.h"
#include "../../shared/effects/jet_cone.h"
#include "../../shared/effects/ground_glow.h"
#include "../shared/attack.h"

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

enum {
    /// Vertices per mouth/collar square; also the collar's offset in vertex elements.
    HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT = 4,
    /// Opposed walls drawn from corner pairs (0, 2) and (1, 3), on local -X and +X.
    HYPERVELOCITY_DISCHARGE_CONE_WALL_COUNT = 2,
};

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

/// The model `gHypervelocityRootModel`: generated from its asset (weapons/hypervelocity/root_model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/hypervelocity/root_model.inc"

/// The model `gHypervelocitySlideModel`: generated from its asset (weapons/hypervelocity/slide_model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/hypervelocity/slide_model.inc"

/// The model `gHypervelocityHingeModel`: generated from its asset (weapons/hypervelocity/hinge_model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/hypervelocity/hinge_model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/hypervelocity/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/hypervelocity/clips.inc"
#include "assets/weapons/hypervelocity/bank.inc"

/// Shared texture phases of the Hypervelocity trail segments and discharge walls.
///
/// Round initialization rewrites all sixteen signed halfwords with LCG values
/// 0..255. Both jet tails use every entry; discharge walls reuse entries 0 and 1.
/// Drawers select one of six 40-texel cells by (offset + active age) modulo six.
/// The table belongs to the loaded overlay, so firing another round also changes
/// any still-live discharge cones' phases. Initial zero values are valid phases.
static s16 _gHypervelocityConeTextureFrameOffsets[EFFECT_BAND_SEGMENT_COUNT] = { 0 };
STATIC_ASSERT(HYPERVELOCITY_DISCHARGE_CONE_WALL_COUNT <= ARRAY_SIZE(_gHypervelocityConeTextureFrameOffsets), hypervelocity_cone_wall_frame_offsets);

static void _hypervelocityReleaseRound(Task* task);
static void _hypervelocityReleaseWeapon(Task* task);

static void _hypervelocityDrawDischargeCone(const GfxCoord* coord, s16 ageFrames, s32 halfExtent, const u8* rgb);
static void _hypervelocityUpdateModelPose(Task* task);
static void _hypervelocityInitWeaponModel(Task* task);
static void _hypervelocityQueueWeaponTeardown(Task* task);

void hypervelocityChargeEffectTask(Task* task)
{
    enum {
        HYPERVELOCITY_CHARGE_EFFECT_INIT            = 0,
        HYPERVELOCITY_CHARGE_EFFECT_IDLE            = 1,
        HYPERVELOCITY_CHARGE_EFFECT_CHARGE          = 2,
        HYPERVELOCITY_CHARGE_EFFECT_DISCHARGE       = 3,
        HYPERVELOCITY_CHARGE_EFFECT_COOL            = 4,
        HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_UNIT = 256,
        HYPERVELOCITY_CHARGE_EFFECT_BRIGHTNESS_MAX  = 255,
        HYPERVELOCITY_CHARGE_EFFECT_FIRST_HAND      = 15,
        HYPERVELOCITY_CHARGE_EFFECT_HAND_STRIDE     = 3,
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
            RANDOM_LCG_ADVANCE();
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & HYPERVELOCITY_CHARGE_EFFECT_LIGHT_RANDOM_MASK) + HYPERVELOCITY_CHARGE_EFFECT_CHARGING_BLUE_MIN;
            pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
            pointLight->head.color.g = pointLight->head.color.b >> 1;
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
            RANDOM_LCG_ADVANCE();
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & HYPERVELOCITY_CHARGE_EFFECT_LIGHT_RANDOM_MASK) + HYPERVELOCITY_CHARGE_EFFECT_DISCHARGING_BLUE_MIN;
            pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
            pointLight->head.color.g = pointLight->head.color.b >> 1;
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
                    sndEvtRequestScriptStart(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_DISCHARGE), audioPan, (s8)worldCoordGetOriginAudioDepth(muzzleCoord));
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
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_DISCHARGE), SOUND_SCRIPT_STOP_KEEP_RELEASE);
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
            playerCoords = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            RANDOM_LCG_ADVANCE();
            effectSpawn(EFFECT_DUST_PUFF, &playerCoords[((RANDOM_LCG_DRAW() & 1) * HYPERVELOCITY_CHARGE_EFFECT_HAND_STRIDE) + HYPERVELOCITY_CHARGE_EFFECT_FIRST_HAND], HYPERVELOCITY_CHARGE_EFFECT_DUST_ARGUMENT, NULL);
            if (effectWork->age >= HYPERVELOCITY_CHARGE_EFFECT_RETIRE_AGE || task->spawnArg1.value < 0) {
                task->state = HYPERVELOCITY_CHARGE_EFFECT_IDLE;
            }
            return;
    }
}

void hypervelocityRoundTask(Task* task)
{
    enum {
        HYPERVELOCITY_ROUND_INIT          = 0,
        HYPERVELOCITY_ROUND_FLY           = 1,
        HYPERVELOCITY_ROUND_COLLAPSE      = 2,
        HYPERVELOCITY_ROUND_ROTATION_MASK = 0xFFF,
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
            gfxCopyRotationWords(&roundCoord->coord, &playerRootCoord->coord);
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
                RANDOM_LCG_ADVANCE();
                _gHypervelocityConeTextureFrameOffsets[jitterIndex] = RANDOM_LCG_DRAW() & HYPERVELOCITY_ROUND_JITTER_MASK;
            }
            effectWork->scale = HYPERVELOCITY_ROUND_BRIGHTNESS;
            effectWork->angle = HYPERVELOCITY_ROUND_VISUAL_RADIUS;
            RANDOM_LCG_ADVANCE();
            effectWork->period               = RANDOM_LCG_DRAW() & HYPERVELOCITY_ROUND_ROTATION_MASK;
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
            lightRandom              = RANDOM_LCG_NEXT(gRandomLcgState);
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
            RANDOM_LCG_ADVANCE();
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & HYPERVELOCITY_ROUND_LIGHT_RANDOM_MASK) + HYPERVELOCITY_ROUND_LIGHT_BLUE_BASE;
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

/// GPU CLUT selector for Hypervelocity's sixteen-colour jet-cone texture.
///
/// Palette at VRAM X=16 words, Y=267 rows. The next jet drawer inclusion
/// consumes this integer constant and undefines the binding.
#define JET_CONE_CLUT getClut(16, 267)
/// Radius of Hypervelocity's extended-tail jet rim in whole local coordinate units.
///
/// Signed integer constant in 0..32767 for the drawer's `extendedTail != 0`
/// mode. The local XY rim is at Z=-(2*baseLength + 256*ageFrames); its radius
/// stays fixed. The next drawer inclusion checks the range and undefines it.
#define JET_CONE_EXTENDED_TAIL_RIM_RADIUS 0x600
/// Radius of Hypervelocity's base-tail jet rim in whole local coordinate units.
///
/// Signed integer constant in 0..32767 for the drawer's `extendedTail == 0`
/// mode. The local XY rim is at Z=-(baseLength + 16*ageFrames); its radius
/// stays fixed. The next drawer inclusion checks the range and undefines it.
#define JET_CONE_BASE_TAIL_RIM_RADIUS 0x800
/// Selects Hypervelocity's retained per-segment texture animation offsets.
///
/// Binds the next jet drawer inclusion to the overlay-owned s16 array.
/// Round startup fills it with offsets 0..255; both jet tails use the same
/// values. See `jet_cone.h` for the extent and value contract. The fragment
/// undefines this binding after use.
#define JET_CONE_TEXTURE_FRAME_OFFSETS _gHypervelocityConeTextureFrameOffsets
#include "../../shared/effects/jet_cone_draw.inc.c"

/// Packed additive beam-flame page: 4-bit indexed texels at VRAM X=576 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 576, 0)
/// Beam-flame palette: VRAM X=176 words, Y=266 scanlines, also used by `SPRITE_QUAD_SET_FLICKER_ODD_CELL`.
#define SPRITE_QUAD_CLUT getClut(176, 266)
/// Texel width and horizontal stride of the two beam-flame cells selected by age parity.
#define SPRITE_QUAD_CELL_WIDTH 56
/// Low frame bit selecting one of the beam-flame strip's two horizontal columns.
///
/// The signed round age selects U=0x70 or U=0xA8; V stays at 0xC8..0xFF.
/// This signed integer constant configures the next drawer inclusion, which
/// undefines it. See `sprite_quad.h` for the column-mask contract.
#define SPRITE_QUAD_COLUMN_MASK 1
/// First flame-cell U coordinate in texels relative to the selected texture page.
///
/// The effect age's low bit selects U = 0x70..0xA7 or 0xA8..0xDF.
/// This integer constant configures the next drawer inclusion, which undefines it.
#define SPRITE_QUAD_U_BASE 0x70
/// Inclusive top texel row of both beam-flame cells, relative to their texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0xC8
/// Inclusive bottom texel row of both square beam-flame cells, relative to their texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier for the hypervelocity flame sprite.
///
/// Uses the cell's inclusive 55-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/effects/sprite_quad_draw.inc.c"

/// Red texture modulation for Hypervelocity's additive floor glow.
///
/// Integer constant in the GPU's 0..255 colour-byte range; 0x80 is unity,
/// so 0x30 scales the sampled red channel to three eighths of its intensity.
/// Configures the next ground-glow drawer inclusion, which undefines it.
#define GROUND_GLOW_R 0x30
/// Green texture modulation for Hypervelocity's additive floor glow.
///
/// Integer constant in the GPU's 0..255 colour-byte range; 0x80 is unity,
/// so 0x30 scales the sampled green channel to three eighths of its intensity.
/// Configures the next ground-glow drawer inclusion, which undefines it.
#define GROUND_GLOW_G 0x30
/// Blue texture modulation for Hypervelocity's additive floor glow.
///
/// Integer constant in the GPU's 0..255 colour-byte range; 0x80 is unity,
/// so 0x30 scales the sampled blue channel to three eighths of its intensity.
/// Configures the next ground-glow drawer inclusion, which undefines it.
#define GROUND_GLOW_B 0x30
/// GPU palette selector for Hypervelocity's two-frame additive floor glow.
///
/// Selects sixteen colours starting at VRAM (176, 266). The SDK packs these
/// coordinates into an integer constant stored in `POLY_FT4::clut`.
/// Configures the next ground-glow drawer inclusion, which undefines it.
#define GROUND_GLOW_CLUT getClut(176, 266)
#include "../../shared/effects/ground_glow_draw.inc.c"

#include "assets/engine/animation/player_schema.h"

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

    // Geometry is in local game units; the animation occupies six 40-texel cells.
    enum {
        HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_FRAME_COUNT = 6,
        HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_CELL_SIZE   = 40,
        HYPERVELOCITY_DISCHARGE_CONE_TEXTURE_TOP_V       = 0x60
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
        gte_ApplyMatrixSV(worldRotation, &scratch->vertices[cornerIndex], &scratch->vertices[cornerIndex]);
        scratch->vertices[cornerIndex].vx += (u16)coord->workm.t[0];
        scratch->vertices[cornerIndex].vy += (u16)coord->workm.t[1];
        scratch->vertices[cornerIndex].vz += (u16)coord->workm.t[2];
        collarVertex                       = &scratch->vertices[cornerIndex] + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT;
        collarVertex->vx                   = (u16)unitCorners[cornerIndex].axis0Sign * halfExtent;
        collarVertex->vy                   = HYPERVELOCITY_DISCHARGE_CONE_COLLAR_HEIGHT;
        collarVertex->vz                   = (u16)unitCorners[cornerIndex].axis1Sign * depthHalfExtent;
        gte_ApplyMatrixSV(worldRotation, &scratch->vertices[cornerIndex + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT], &scratch->vertices[cornerIndex + HYPERVELOCITY_DISCHARGE_CONE_SQUARE_VERTEX_COUNT]);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz), wall);
        }
        cornerIndex++;
    } while (cornerIndex < HYPERVELOCITY_DISCHARGE_CONE_WALL_COUNT);
    SCRATCH_STACK_RELEASE_BLOCK(_HypervelocityDischargeConeScratch);
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
        HYPERVELOCITY_SHOCK_RING_STATE_INITIALIZE = 0,
        HYPERVELOCITY_SHOCK_RING_STATE_EXPAND     = 1,
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
        HYPERVELOCITY_DISCHARGE_CONE_STATE_INITIALIZE = 0,
        HYPERVELOCITY_DISCHARGE_CONE_STATE_EXPAND     = 1,
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
        HYPERVELOCITY_MODEL_SCRATCH_BYTES = 16,
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
            if (((GameActor*)playerTask->work)->mode != GAME_ACTOR_MODE_NORMAL || ((GameActor*)playerTask->work)->state != PLAYER_ACTOR_STATE_ATTACK) {
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
                    sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_CANCEL), SOUND_SCRIPT_STOP_KEEP_RELEASE);
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
        HYPERVELOCITY_MODEL_POSE_STATE = 1,
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
        HYPERVELOCITY_PHASE_PREPARE = 0,
        HYPERVELOCITY_PHASE_CHARGE  = 1,
        HYPERVELOCITY_PHASE_RECOIL  = 2,
        HYPERVELOCITY_PHASE_RECOVER = 3,
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
            actor->state                             = PLAYER_ACTOR_STATE_ATTACK;
            actor->movementMode                      = GAME_ACTOR_MOVEMENT_STOPPED;
            actor->turnRateIndex                     = 0;
            actor->animationState                    = 0;
            actor->statePhase                        = HYPERVELOCITY_PHASE_CHARGE;
            actor->weaponEffectTask->spawnArg1.value = HYPERVELOCITY_CHARGE_REQUEST_START;
            actor->stateTimer                        = 0;
            weaponTask->spawnArg1.value             |= HYPERVELOCITY_MODEL_CHARGE_SLIDE;
            worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_START), 0);
            worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_LOOP), 0);
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_RELOAD, 0, 3);
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
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_HYPERVELOCITY), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_LOOP), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_SHOT), 1);
                    playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE);
                } else if (chargeTicks == HYPERVELOCITY_SECOND_CHARGE_STAGE_FRAME) {
                    weaponTask->spawnArg1.value |= HYPERVELOCITY_MODEL_CHARGE_OPEN;
                    sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_START), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_SECOND_STAGE), 0);
                }
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_CANCEL), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            } else {
                actor->statePhase                        = HYPERVELOCITY_PHASE_RECOVER;
                actor->weaponEffectTask->spawnArg1.value = HYPERVELOCITY_CHARGE_REQUEST_CANCEL;
                weaponTask->spawnArg1.value              = 0;
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_START), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_LOOP), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_CANCEL), 0);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_RELOAD, 0, 3);
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
            if (_weaponIsArmSlotAdvancing(playerTask) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_HypervelocityRecoilScratch);
}
