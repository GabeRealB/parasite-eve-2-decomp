#include "hypervelocity.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>
#include <psyq/libgs.h>

#include <core/pe2.h>
#include <math/gte.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/effect_tasks.h>
#include <gameplay/effects.h>
#include <gameplay/hud_sprites.h>
#include <gameplay/items.h>
#include <gameplay/light.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>
#include <gameplay/world_collision_types.h>

#include <engine/coord.h>
#include <engine/display.h>
#include <engine/display_types.h>
#include <engine/random.h>
#include <engine/gfx.h>
#include <engine/mc_types.h>
#include <engine/mem.h>
#include <engine/pad.h>
#include <engine/scratch.h>
#include <engine/session.h>
#include <engine/session_types.h>
#include <engine/sound.h>
#include <engine/task.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

#include <weapons/weapon/weapon.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(RECOIL_FRAMES, int, 0x15)
PE2_OPTION_DEF(CHARGE_FRAMES, int, 90)
PE2_OPTION_DEF(SECOND_CHARGE_STAGE_FRAME, int, 60)
PE2_OPTION_DEF(RECOIL_PUSH_WINDOW, int, 19)
PE2_OPTION_DEF(RECOIL_FIRST_PUSH_FRAME, int, 18)
PE2_OPTION_DEF(RECOIL_DIVISOR, int, 378)
PE2_OPTION_DEF(RECOIL_FIRST_PUSH_DIVISOR, int, 244)
/// The fired round (`hypervelocityRoundTask`): times in dispatch ticks, radii in game units.
PE2_OPTION_DEF(ROUND_SPEED, int, 0x400)
PE2_OPTION_DEF(ROUND_BRIGHTNESS, int, 0xC0)
PE2_OPTION_DEF(ROUND_VISUAL_RADIUS, int, 0x500)
PE2_OPTION_DEF(ROUND_COLLISION_RADIUS, int, 0x800)
PE2_OPTION_DEF(ROUND_LIFETIME_FRAMES, int, 0x15)
PE2_OPTION_DEF(ROUND_COLLAPSE_STEP, int, 0x40)
PE2_OPTION_DEF(ROUND_COLLAPSE_MIN_RADIUS, int, 0x80)
PE2_OPTION_DEF(ROUND_LIGHT_TTL, int, 4)
PE2_OPTION_DEF(ROUND_LIGHT_INNER_BASE, int, 0x200)
PE2_OPTION_DEF(ROUND_LIGHT_OUTER_SCALE, int, 16)
PE2_OPTION_DEF(ROUND_LIGHT_BLUE_BASE, int, 0x800)
/// The discharge cones (`hypervelocityDischargeConeTask`): brightness 0..255, extents in game units.
PE2_OPTION_DEF(DISCHARGE_CONE_COLLAR_HEIGHT, int, 0x700)
PE2_OPTION_DEF(DISCHARGE_CONE_INITIAL_MOUTH_HEIGHT, int, 0x600)
PE2_OPTION_DEF(DISCHARGE_CONE_INITIAL_FLARE, int, 0x200)
PE2_OPTION_DEF(DISCHARGE_CONE_AGE_GEOMETRY_SHIFT, int, 7)
PE2_OPTION_DEF(DISCHARGE_CONE_INITIAL_BRIGHTNESS, int, 0x80)
PE2_OPTION_DEF(DISCHARGE_CONE_INITIAL_HALF_EXTENT, int, 0x200)
PE2_OPTION_DEF(DISCHARGE_CONE_HALF_EXTENT_STEP, int, 0x60)
PE2_OPTION_DEF(DISCHARGE_CONE_FADE_STEP, int, 8)
PE2_OPTION_DEF(DISCHARGE_CONE_MIN_BRIGHTNESS, int, 6)
/// The launch's shock ring (`hypervelocityShockRingTask`): brightness 0..255, radii in game units.
PE2_OPTION_DEF(SHOCK_RING_INITIAL_BRIGHTNESS, int, 0xF0)
PE2_OPTION_DEF(SHOCK_RING_INITIAL_RADIUS, int, 0x100)
PE2_OPTION_DEF(SHOCK_RING_RADIUS_STEP, int, 0x40)
PE2_OPTION_DEF(SHOCK_RING_FADE_STEP, int, 0x10)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(root_model, model)
PE2_BIND_ARG(root_model, _symbol, gHypervelocityRootModel)

PE2_BIND(slide_model, model)
PE2_BIND_ARG(slide_model, _symbol, gHypervelocitySlideModel)

PE2_BIND(hinge_model, model)
PE2_BIND_ARG(hinge_model, _symbol, gHypervelocityHingeModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gHypervelocityPlayerAnimationBank)

/// The Hypervelocity's slots the attack state plays alone (`HYPERVELOCITY_SOUND`; the charge's are public).
enum {
    SOUND_SECOND_STAGE = 2,
    SOUND_DISCHARGE    = 6,
    SOUND_SHOT         = 7,
};

/// The charge glow, discharge and cooling smoke (`hypervelocityChargeEffectTask`): times in dispatch ticks, brightness 0..255, radii in game units.
enum {
    CHARGE_EFFECT_TIMEOUT_FRAMES        = 65,
    CHARGE_EFFECT_DISCHARGE_FRAMES      = 24,
    CHARGE_EFFECT_RETIRE_AGE            = 111,
    CHARGE_EFFECT_ARC_START_BRIGHTNESS  = 129,
    CHARGE_EFFECT_RADIUS_START          = 64,
    CHARGE_EFFECT_RADIUS_MAX            = 512,
    CHARGE_EFFECT_RADIUS_STEP           = 8,
    CHARGE_EFFECT_FADE_STEP             = 32,
    CHARGE_EFFECT_PARTICLE_SIZE         = 384,
    CHARGE_EFFECT_LIGHT_FRAMES          = 4,
    CHARGE_EFFECT_LIGHT_RANDOM_MASK     = 0x700,
    CHARGE_EFFECT_CHARGING_BLUE_MIN     = 1024,
    CHARGE_EFFECT_DISCHARGING_BLUE_MIN  = 2048,
    CHARGE_EFFECT_LIGHT_INNER           = 256,
    CHARGE_EFFECT_LIGHT_OUTER           = 4096,
    CHARGE_EFFECT_DISCHARGE_LIGHT_INNER = 1024,
    CHARGE_EFFECT_DISCHARGE_LIGHT_OUTER = 16384,
    CHARGE_EFFECT_DUST_ARGUMENT         = 0x2300,
    CHARGE_EFFECT_DRIFT_AGE_MASK        = 15,
    CHARGE_EFFECT_CHARGE_DRIFT_SHIFT    = 5,
    CHARGE_EFFECT_DISCHARGE_DRIFT_SHIFT = 6,
    CHARGE_EFFECT_ARC_RADIUS            = 96,
    CHARGE_EFFECT_ARC_COUNTDOWN_SCALE   = 128,
};

/// The fired round (`hypervelocityRoundTask`) and the table rows its attack is read from: times in dispatch ticks, radii in game units.
/// The table rows the round's attack is read from, and the key its sphere carries.
enum {
    ROUND_ATTACK_ROW   = 26,
    ROUND_DISTANCE_ROW = 22,
    /// Category-2 weapon-attack key carried by the Hypervelocity round's sphere.
    ///
    /// Int constant expression: low seven bits select weapon attack row 26
    /// (2000 base damage, stagger, spark burst, 10-frame hit cooldown); bits 8..13
    /// select distance/critical row 22, the Hypervelocity weapon index. Attachment,
    /// companion and alternate-critical bits are clear. Independent of live ammunition.
    ROUND_COLLISION_KEY     = WORLD_COLLISION_CONTACT_ATTACK | (ROUND_DISTANCE_ROW << 8) | ROUND_ATTACK_ROW,
    ROUND_SPARK_ARGUMENT    = 0x400,
    ROUND_LIGHT_SLOT        = 0,
    ROUND_LIGHT_INDEX_SHIFT = 9,
    ROUND_LIGHT_RANDOM_MASK = 0x700,
    ROUND_JITTER_MASK       = 0xFF,
};

/// The weapon model and its moving parts (`hypervelocityWeaponModelTask`): the task bank and descriptors that spawn them, and their charge motion.
enum {
    MODEL_SLIDE_MAX_TICKS      = 60,
    MODEL_SLIDE_UNITS_PER_TICK = 4,
    MODEL_HINGE_STEP           = 0x110,
    MODEL_HINGE_LIMIT          = -0x400,
};

/// Signed effect-age argument whose low bit selects one of the two flame cells.
#define SPRITE_QUAD_FRAME_T s16
#include <effects/sprite_quad/sprite_quad.h>
#include <effects/jet_cone/jet_cone.h>
#include <effects/ground_glow/ground_glow.h>
#include <weapons/attack/attack.h>

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
} _RecoilScratch;
STATIC_ASSERT_SIZEOF(_RecoilScratch, 0x18);

enum {
    /// Vertices per mouth/collar square; also the collar's offset in vertex elements.
    DISCHARGE_CONE_SQUARE_VERTEX_COUNT = 4,
    /// Opposed walls drawn from corner pairs (0, 2) and (1, 3), on local -X and +X.
    DISCHARGE_CONE_WALL_COUNT = 2,
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
/// `DISCHARGE_CONE_SQUARE_VERTEX_COUNT` elements on.
///
/// Only two opposed walls are drawn, one at a time. Wall `i` takes its
/// vertices 0 and 1 from mouth vertices `i` and `i + 2`, the pair sharing a
/// local X sign, and its vertices 2 and 3 from the collar vertices of the same
/// corners. The remaining fields are refilled for each wall.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order after drawing. Pointers into the block must not survive its release.
typedef struct {
    SVECTOR vertices[2 * DISCHARGE_CONE_SQUARE_VERTEX_COUNT]; // World-space vertices: the mouth square's, then the collar square's
    s32     otz;                                              // Ordering-table depth of the current wall: SZ3 / 4 of its last vertex, plus 1
    s32     projectionFlags;                                  // GTE FLAG word after the wall's RTPT; bit 31 makes it negative and drops the wall
    DVECTOR sxy0;                                             // Screen position of the current wall's vertex 0
    DVECTOR sxy1;                                             // Screen position of vertex 1
    DVECTOR sxy2;                                             // Screen position of vertex 2
    DVECTOR sxy3;                                             // Screen position of vertex 3
} _DischargeConeScratch;
STATIC_ASSERT_SIZEOF(_DischargeConeScratch, 0x58);
// The drawer stages one mouth and one collar vertex per unit-quad corner.
STATIC_ASSERT(ARRAY_SIZE(gEffectUnitQuadCorners) == DISCHARGE_CONE_SQUARE_VERTEX_COUNT, hypervelocity_discharge_cone_square_vertex_count);

/// Collision block of one hypervelocity round in flight, allocated zeroed on
/// its first tick and kept at `Task::work`.
///
/// The sphere is linked on collision list 1 for as long as the round flies.
/// Its radius is a fixed 0x800 game-coordinate units, its packed key is
/// `ROUND_COLLISION_KEY`, and its centre stays the origin of the
/// coordinate the round flies on. The sphere takes pair tests only, so it is
/// what the bodies it passes through record as a contact. The round itself
/// never reads `contacts`: a contact does not stop it, and its flight ends
/// only on room geometry, a separate segment test along each step, or on age.
/// The task's exit callback unlinks `Task::work` as a `WorldCollisionBody`,
/// which addresses this block while `body` remains its first member.
typedef struct {
    WorldCollisionBody    body;        // Sphere linked on list 1; pair tests are enabled after the link
    WorldCollisionContact contacts[1]; // One-entry table `body` borrows. The entry is marked LAST; occupied contacts are cleared each flight frame and never read
} _RoundBody;
STATIC_ASSERT_SIZEOF(_RoundBody, 0x38);

/// Charge-effect muzzle position in the weapon root's local game coordinates.
///
/// The persistent effect uses this translation for its glow, point light and
/// round launch. The fourth halfword is unused by coordinate placement.
static SVECTOR _gMuzzleOffset = { 0, 0x240, 0x80, 0 };

/// Its three models - its body, the slide and the hinge - and the player's clips: here, as the package stores them
/// after the data above.
PE2_VALUE(root_model)
PE2_VALUE(slide_model)
PE2_VALUE(hinge_model)
PE2_VALUE(bank)

/// Low-nibble model component selector and charge-driven motions in spawnArg1.
enum {
    MODEL_COMPONENT_MASK  = 0xF,
    MODEL_ROOT            = 0,
    MODEL_SLIDE           = 1,
    MODEL_HINGE           = 2,
    MODEL_CHARGE_SLIDE    = 0x10,
    MODEL_CHARGE_OPEN     = 0x20,
    CHARGE_REQUEST_CANCEL = -1,
    CHARGE_REQUEST_START  = 1,
};

/// Shared texture phases of the Hypervelocity trail segments and discharge walls.
///
/// Round initialization rewrites all sixteen signed halfwords with LCG values
/// 0..255. Both jet tails use every entry; discharge walls reuse entries 0 and 1.
/// Drawers select one of six 40-texel cells by (offset + active age) modulo six.
/// The table belongs to the loaded overlay, so firing another round also changes
/// any still-live discharge cones' phases. Initial zero values are valid phases.
static s16 _gConeTextureFrameOffsets[EFFECT_BAND_SEGMENT_COUNT] = { 0 };
STATIC_ASSERT(DISCHARGE_CONE_WALL_COUNT <= ARRAY_SIZE(_gConeTextureFrameOffsets), hypervelocity_cone_wall_frame_offsets);

static void _releaseRound(Task* task);
static void _releaseWeapon(Task* task);

static void _drawDischargeCone(const GfxCoord* coord, s16 ageFrames, s32 halfExtent, const u8* rgb);
static void _updateModelPose(Task* task);
static void _initWeaponModel(Task* task);
static void _queueWeaponTeardown(Task* task);

void hypervelocityChargeEffectTask(Task* task)
{
    enum {
        CHARGE_EFFECT_INIT            = 0,
        CHARGE_EFFECT_IDLE            = 1,
        CHARGE_EFFECT_CHARGE          = 2,
        CHARGE_EFFECT_DISCHARGE       = 3,
        CHARGE_EFFECT_COOL            = 4,
        CHARGE_EFFECT_BRIGHTNESS_UNIT = 256,
        CHARGE_EFFECT_BRIGHTNESS_MAX  = 255,
        CHARGE_EFFECT_FIRST_HAND      = 15,
        CHARGE_EFFECT_HAND_STRIDE     = 3,
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
            task->state = CHARGE_EFFECT_IDLE;
        }
        return;
    }

    effectWork->age = effectWork->age + 1;
    switch (task->state) {
        case CHARGE_EFFECT_INIT:
            muzzleCoord->parent = effectWork->parent;
            gfxSetRotIdentity(&muzzleCoord->coord);
            muzzleCoord->coord.t[0]   = _gMuzzleOffset.vx;
            muzzleCoord->coord.t[1]   = _gMuzzleOffset.vy;
            muzzleCoord->coord.t[2]   = _gMuzzleOffset.vz;
            muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state               = CHARGE_EFFECT_IDLE;
            /* fallthrough */
        case CHARGE_EFFECT_IDLE:
            if (task->spawnArg1.value == CHARGE_REQUEST_START) {
                task->state     = CHARGE_EFFECT_CHARGE;
                effectWork->age = 0;
            }
            return;
        case CHARGE_EFFECT_CHARGE:
            // Charge may time out into discharge independently of the player's shot timer.
            actorRenderComposeCoord(muzzleCoord);
            effectWork->move.vy = -((effectWork->age & CHARGE_EFFECT_DRIFT_AGE_MASK) << CHARGE_EFFECT_CHARGE_DRIFT_SHIFT);
            if (effectWork->age & 1) {
                effectSpawn(EFFECT_SPARK_FADE, muzzleCoord, CHARGE_EFFECT_PARTICLE_SIZE, &effectWork->move);
            }
            transientLight->framesLeft = CHARGE_EFFECT_LIGHT_FRAMES;
            pointLight->inner          = CHARGE_EFFECT_LIGHT_INNER;
            pointLight->outer          = CHARGE_EFFECT_LIGHT_OUTER;
            RANDOM_LCG_ADVANCE();
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & CHARGE_EFFECT_LIGHT_RANDOM_MASK) + CHARGE_EFFECT_CHARGING_BLUE_MIN;
            pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
            pointLight->head.color.g = pointLight->head.color.b >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &muzzleCoord->workm, &lightCoord->coord);
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->spawnArg1.value < 0) {
                task->spawnArg1.value = 0;
                task->state           = CHARGE_EFFECT_IDLE;
                return;
            }
            if (effectWork->age >= CHARGE_EFFECT_TIMEOUT_FRAMES) {
                task->spawnArg1.value = CHARGE_EFFECT_DISCHARGE_FRAMES;
            }
            if (task->spawnArg1.value >= 2) {
                effectWork->scale  = 0;
                effectWork->angle  = CHARGE_EFFECT_RADIUS_START;
                effectWork->period = 0;
                effectWork->step   = CHARGE_EFFECT_BRIGHTNESS_UNIT / task->spawnArg1.value;
                task->state        = CHARGE_EFFECT_DISCHARGE;
            }
            return;
        case CHARGE_EFFECT_DISCHARGE:
            // The request becomes a countdown; its zero tick owns the round spawn.
            actorRenderComposeCoord(muzzleCoord);
            effectWork->move.vy = -((effectWork->age & CHARGE_EFFECT_DRIFT_AGE_MASK) << CHARGE_EFFECT_DISCHARGE_DRIFT_SHIFT);
            effectSpawn(EFFECT_FLASH_BURST, muzzleCoord, CHARGE_EFFECT_PARTICLE_SIZE, &effectWork->move);
            transientLight->framesLeft = CHARGE_EFFECT_LIGHT_FRAMES;
            pointLight->inner          = CHARGE_EFFECT_DISCHARGE_LIGHT_INNER;
            pointLight->outer          = CHARGE_EFFECT_DISCHARGE_LIGHT_OUTER;
            RANDOM_LCG_ADVANCE();
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & CHARGE_EFFECT_LIGHT_RANDOM_MASK) + CHARGE_EFFECT_DISCHARGING_BLUE_MIN;
            pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
            pointLight->head.color.g = pointLight->head.color.b >> 1;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &muzzleCoord->workm, &lightCoord->coord);
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            effectWork->scale       += effectWork->step;
            if (effectWork->scale >= CHARGE_EFFECT_BRIGHTNESS_UNIT) {
                effectWork->scale = CHARGE_EFFECT_BRIGHTNESS_MAX;
            }
            effectWork->angle += CHARGE_EFFECT_RADIUS_STEP;
            if (effectWork->angle >= CHARGE_EFFECT_RADIUS_MAX + 1) {
                effectWork->angle = CHARGE_EFFECT_RADIUS_MAX;
            }
            tintRgb[0] = effectWork->scale >> 1;
            tintRgb[1] = effectWork->scale >> 1;
            tintRgb[2] = effectWork->scale;
            effectDrawGouraudDisc(muzzleCoord, effectWork->angle, tintRgb);
            effectDrawGouraudDisc(muzzleCoord, (s16)((u16)effectWork->angle * 2), tintRgb);
            if (effectWork->scale >= CHARGE_EFFECT_ARC_START_BRIGHTNESS) {
                if (effectWork->period == 0) {
                    audioPan = (s8)worldCoordGetOriginAudioPan(muzzleCoord);
                    sndEvtRequestScriptStart(HYPERVELOCITY_SOUND(SOUND_DISCHARGE), audioPan, (s8)worldCoordGetOriginAudioDepth(muzzleCoord));
                }
                effectWork->period += (u16)effectWork->step * 2;
                if (effectWork->period >= CHARGE_EFFECT_BRIGHTNESS_UNIT) {
                    effectWork->period = CHARGE_EFFECT_BRIGHTNESS_MAX;
                }
                tintRgb[0] = effectWork->period >> 1;
                tintRgb[1] = effectWork->period >> 1;
                tintRgb[2] = effectWork->period;
                effectDrawOuterGlowBand(muzzleCoord, (s16)((u16)task->spawnArg1.value * CHARGE_EFFECT_ARC_COUNTDOWN_SCALE), CHARGE_EFFECT_ARC_RADIUS, tintRgb);
            }
            if (task->spawnArg1.value < 0) {
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(SOUND_DISCHARGE), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                task->spawnArg1.value = 0;
                task->state           = CHARGE_EFFECT_IDLE;
                return;
            }
            task->spawnArg1.value = task->spawnArg1.value - 1;
            if (task->spawnArg1.value == 0) {
                task->state = CHARGE_EFFECT_COOL;
                roundWork   = effectSpawn(EFFECT_HYPERVELOCITY_ROUND, muzzleCoord, 0, NULL);
                if (roundWork != NULL) {
                    taskReparent(task, roundWork->task);
                }
                effectWork->scale = CHARGE_EFFECT_BRIGHTNESS_MAX;
            }
            return;
        case CHARGE_EFFECT_COOL:
            // Fade the ring and emit smoke from alternating randomly selected hands.
            actorRenderComposeCoord(muzzleCoord);
            effectWork->move.vy = -((effectWork->age & CHARGE_EFFECT_DRIFT_AGE_MASK) << CHARGE_EFFECT_DISCHARGE_DRIFT_SHIFT);
            effectSpawn(EFFECT_SPARK_FADE, muzzleCoord, CHARGE_EFFECT_PARTICLE_SIZE, &effectWork->move);
            if (effectWork->angle > 0) {
                tintRgb[0] = effectWork->scale >> 1;
                tintRgb[1] = effectWork->scale >> 1;
                tintRgb[2] = effectWork->scale;
                effectDrawGouraudDisc(muzzleCoord, effectWork->angle, tintRgb);
                effectDrawGouraudDisc(muzzleCoord, (s16)((u16)effectWork->angle * 2), tintRgb);
                effectDrawScreenTint(tintRgb, GPU_BLEND_ADD);
                effectWork->scale = effectWork->scale - CHARGE_EFFECT_FADE_STEP;
                effectWork->angle = effectWork->angle - CHARGE_EFFECT_FADE_STEP;
            }
            playerCoords = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            RANDOM_LCG_ADVANCE();
            effectSpawn(EFFECT_DUST_PUFF, &playerCoords[((RANDOM_LCG_DRAW() & 1) * CHARGE_EFFECT_HAND_STRIDE) + CHARGE_EFFECT_FIRST_HAND], CHARGE_EFFECT_DUST_ARGUMENT, NULL);
            if (effectWork->age >= CHARGE_EFFECT_RETIRE_AGE || task->spawnArg1.value < 0) {
                task->state = CHARGE_EFFECT_IDLE;
            }
            return;
    }
}

void hypervelocityRoundTask(Task* task)
{
    enum {
        ROUND_INIT          = 0,
        ROUND_FLY           = 1,
        ROUND_COLLAPSE      = 2,
        ROUND_ROTATION_MASK = 0xFFF,
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
    _RoundBody*                    roundBody;
    u32                            lightRandom;
    s32                            jitterIndex;

    roundBody  = task->work;
    effectWork = task->spawnArg2.pointer;
    roundCoord = task->extra.coordBody->coord;
    lightSlot  = &gWorldCoordTransientPointLights[ROUND_LIGHT_SLOT];
    lightCoord = &lightSlot->light.head.transform.coord;
    pointLight = &lightSlot->light;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        effectWork->age = effectWork->age - 1;
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state != ROUND_INIT) {
                worldCollisionUnlinkBody(&roundBody->body);
            }
            effectKillTask(effectWork, task);
        }
        return;
    }

    effectWork->age = effectWork->age + 1;
    switch (task->state) {
        case ROUND_INIT:
            // Allocate collision separately and borrow only the player's local rotation.
            roundBody = memCalloc(sizeof(_RoundBody), 0);
            if (roundBody == NULL) {
                effectWork->age = 0;
                return;
            }
            task->exitCallback = _releaseRound;
            playerRootCoord    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            gfxCopyRotationWords(&roundCoord->coord, &playerRootCoord->coord);
            roundCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
            gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(roundCoord);
            effectWork->move.vx = 0;
            effectWork->move.vy = 0;
            effectWork->move.vz = PE2_OPTION(ROUND_SPEED);
            gte_SetRotMatrix(&playerRootCoord->coord);
            gte_ldv0(&effectWork->move);
            gte_rtv0();
            gte_stsv(&effectWork->move);
            for (jitterIndex = 0; jitterIndex < ARRAY_SIZE(_gConeTextureFrameOffsets); jitterIndex++) {
                RANDOM_LCG_ADVANCE();
                _gConeTextureFrameOffsets[jitterIndex] = RANDOM_LCG_DRAW() & ROUND_JITTER_MASK;
            }
            effectWork->scale = PE2_OPTION(ROUND_BRIGHTNESS);
            effectWork->angle = PE2_OPTION(ROUND_VISUAL_RADIUS);
            RANDOM_LCG_ADVANCE();
            effectWork->period               = RANDOM_LCG_DRAW() & ROUND_ROTATION_MASK;
            task->work                       = roundBody;
            roundBody->body.context.contacts = roundBody->contacts;
            roundBody->body.radius           = PE2_OPTION(ROUND_COLLISION_RADIUS);
            roundBody->body.coord            = roundCoord;
            roundBody->body.key              = ROUND_COLLISION_KEY;
            roundBody->body.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &roundBody->body);
            // The allocation already zeroed the entry; LAST terminates the table.
            roundBody->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
            roundBody->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            childEffect                  = effectSpawn(EFFECT_HYPERVELOCITY_SHOCK_RING, roundCoord, 0, NULL);
            if (childEffect != NULL) {
                taskReparent(task, childEffect->task);
            }
            task->state              = ROUND_FLY;
            lightSlot->framesLeft    = PE2_OPTION(ROUND_LIGHT_TTL);
            pointLight->inner        = (effectWork->index << ROUND_LIGHT_INDEX_SHIFT) + PE2_OPTION(ROUND_LIGHT_INNER_BASE);
            pointLight->outer        = pointLight->inner * PE2_OPTION(ROUND_LIGHT_OUTER_SCALE);
            lightRandom              = RANDOM_LCG_NEXT(gRandomLcgState);
            pointLight->head.color.b = ((lightRandom >> 16) & ROUND_LIGHT_RANDOM_MASK) + PE2_OPTION(ROUND_LIGHT_BLUE_BASE);
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
        case ROUND_FLY:
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
            if (effectWork->age < PE2_OPTION(ROUND_LIFETIME_FRAMES)) {
                effectSpawn(EFFECT_FLASH_BURST, roundCoord, ROUND_SPARK_ARGUMENT, NULL);
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
            pointLight->head.color.b = (RANDOM_LCG_DRAW() & ROUND_LIGHT_RANDOM_MASK) + PE2_OPTION(ROUND_LIGHT_BLUE_BASE);
            pointLight->head.color.r = (u16)pointLight->head.color.b >> 1;
            lightSlot->framesLeft    = PE2_OPTION(ROUND_LIGHT_TTL);
            pointLight->head.color.g = pointLight->head.color.b >> 1;
            if (worldCollisionProbeGridSegment(&stepEnd, &stepStart, NULL, NULL) == 1) {
                worldCollisionUnlinkBody(&roundBody->body);
                task->state = ROUND_COLLAPSE;
                return;
            }
            if (effectWork->age >= PE2_OPTION(ROUND_LIFETIME_FRAMES)) {
                worldCollisionUnlinkBody(&roundBody->body);
                effectKillTask(effectWork, task);
                return;
            }
            worldCollisionClearContacts(roundBody->contacts);
            return;
        case ROUND_COLLAPSE:
            // The unlinked round stays at the hit point while its visual radius contracts.
            actorRenderComposeCoord(roundCoord);
            effectWork->angle = effectWork->angle - PE2_OPTION(ROUND_COLLAPSE_STEP);
            rgb[0]            = effectWork->scale >> 2;
            rgb[1]            = effectWork->scale >> 2;
            rgb[2]            = effectWork->scale >> 1;
            spriteQuadDraw(roundCoord, effectWork->age, effectWork->angle, effectWork->period);
            effectDrawGouraudDisc(roundCoord, effectWork->angle, rgb);
            if (effectWork->angle < PE2_OPTION(ROUND_COLLAPSE_MIN_RADIUS)) {
                effectKillTask(effectWork, task);
                return;
            }
            effectSpawn(EFFECT_FLASH_BURST, roundCoord, ROUND_SPARK_ARGUMENT, NULL);
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
#define JET_CONE_TEXTURE_FRAME_OFFSETS _gConeTextureFrameOffsets
#include <effects/jet_cone/draw.inc.c>

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
#include <effects/sprite_quad/draw.inc.c>

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
#include <effects/ground_glow/draw.inc.c>

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
static void _drawDischargeCone(const GfxCoord* coord, s16 ageFrames, s32 halfExtent, const u8* rgb)
{

    // Geometry is in local game units; the animation occupies six 40-texel cells.
    enum {
        DISCHARGE_CONE_TEXTURE_FRAME_COUNT = 6,
        DISCHARGE_CONE_TEXTURE_CELL_SIZE   = 40,
        DISCHARGE_CONE_TEXTURE_TOP_V       = 0x60
    };

    _DischargeConeScratch*      scratch;
    POLY_FT4*                   wall;
    const EffectUnitQuadCorner* unitCorners;
    SVECTOR*                    collarVertex;
    const MATRIX*               worldRotation;
    s32                         cornerIndex;
    s32                         mouthExpansion;
    s32                         mouthHeight;
    u16                         mouthHalfWidth;
    s32                         depthHalfExtent;
    s32                         textureU;

    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_DischargeConeScratch);
    mouthExpansion  = ageFrames;
    mouthExpansion  = mouthExpansion << PE2_OPTION(DISCHARGE_CONE_AGE_GEOMETRY_SHIFT);
    mouthHeight     = PE2_OPTION(DISCHARGE_CONE_INITIAL_MOUTH_HEIGHT) - mouthExpansion;
    mouthExpansion  = mouthExpansion + PE2_OPTION(DISCHARGE_CONE_INITIAL_FLARE);
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
        collarVertex                       = &scratch->vertices[cornerIndex] + DISCHARGE_CONE_SQUARE_VERTEX_COUNT;
        collarVertex->vx                   = (u16)unitCorners[cornerIndex].axis0Sign * halfExtent;
        collarVertex->vy                   = PE2_OPTION(DISCHARGE_CONE_COLLAR_HEIGHT);
        collarVertex->vz                   = (u16)unitCorners[cornerIndex].axis1Sign * depthHalfExtent;
        gte_ApplyMatrixSV(worldRotation, &scratch->vertices[cornerIndex + DISCHARGE_CONE_SQUARE_VERTEX_COUNT], &scratch->vertices[cornerIndex + DISCHARGE_CONE_SQUARE_VERTEX_COUNT]);
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
                 &scratch->vertices[cornerIndex + DISCHARGE_CONE_SQUARE_VERTEX_COUNT],
                 &scratch->vertices[cornerIndex + DISCHARGE_CONE_SQUARE_VERTEX_COUNT + 2]);
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
            textureU    = (s16)((_gConeTextureFrameOffsets[cornerIndex] + ageFrames) % DISCHARGE_CONE_TEXTURE_FRAME_COUNT) * DISCHARGE_CONE_TEXTURE_CELL_SIZE;
            wall->u0    = textureU;
            wall->v0    = DISCHARGE_CONE_TEXTURE_TOP_V;
            wall->u1    = textureU + DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            wall->v1    = DISCHARGE_CONE_TEXTURE_TOP_V;
            wall->u2    = textureU;
            wall->u3    = textureU + DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            wall->v2    = DISCHARGE_CONE_TEXTURE_TOP_V + DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            wall->v3    = DISCHARGE_CONE_TEXTURE_TOP_V + DISCHARGE_CONE_TEXTURE_CELL_SIZE - 1;
            setXY4(wall, scratch->sxy0.vx, scratch->sxy0.vy, scratch->sxy1.vx, scratch->sxy1.vy, scratch->sxy2.vx, scratch->sxy2.vy, scratch->sxy3.vx,
                   scratch->sxy3.vy);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz), wall);
        }
        cornerIndex++;
    } while (cornerIndex < DISCHARGE_CONE_WALL_COUNT);
    SCRATCH_STACK_RELEASE_BLOCK(_DischargeConeScratch);
}

/// Tears down a counted hypervelocity round after unlinking its collision sphere.
///
/// The live task owns a `_RoundBody` in `work` (or NULL) and a
/// separate `EffectWork` in `spawnArg2.pointer`. Default teardown frees the
/// collision block and releases the coordinate body and task. Call once;
/// neither allocation nor the task may be used after teardown.
static void _releaseRound(Task* task)
{
    _RoundBody* roundBody  = task->work;
    EffectWork* effectWork = task->spawnArg2.pointer;

    if (roundBody != NULL) {
        worldCollisionUnlinkBody(&roundBody->body);
    }
    effectKillTask(effectWork, task);
}

void hypervelocityShockRingTask(Task* task)
{
    enum {
        SHOCK_RING_STATE_INITIALIZE = 0,
        SHOCK_RING_STATE_EXPAND     = 1,
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
    if (task->state == SHOCK_RING_STATE_INITIALIZE) {
        effectWork->scale = PE2_OPTION(SHOCK_RING_INITIAL_BRIGHTNESS);
        effectWork->angle = PE2_OPTION(SHOCK_RING_INITIAL_RADIUS);
        task->state       = SHOCK_RING_STATE_EXPAND;
    }
    rgb[0] = effectWork->scale >> 1;
    rgb[1] = effectWork->scale >> 1;
    rgb[2] = effectWork->scale;
    effectDrawRaisedGlowBand(coord, effectWork->angle, rgb);
    effectWork->angle += PE2_OPTION(SHOCK_RING_RADIUS_STEP);
    nextBrightness     = effectWork->scale - PE2_OPTION(SHOCK_RING_FADE_STEP);
    effectWork->scale  = nextBrightness;
    if (nextBrightness < PE2_OPTION(SHOCK_RING_FADE_STEP)) {
        effectKillTask(effectWork, task);
    }
}

void hypervelocityDischargeConeTask(Task* task)
{
    enum {
        DISCHARGE_CONE_STATE_INITIALIZE = 0,
        DISCHARGE_CONE_STATE_EXPAND     = 1,
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
    if (task->state == DISCHARGE_CONE_STATE_INITIALIZE) {
        effectWork->scale = PE2_OPTION(DISCHARGE_CONE_INITIAL_BRIGHTNESS);
        effectWork->angle = PE2_OPTION(DISCHARGE_CONE_INITIAL_HALF_EXTENT);
        task->state       = DISCHARGE_CONE_STATE_EXPAND;
    }
    rgb[0] = effectWork->scale;
    rgb[1] = effectWork->scale;
    rgb[2] = effectWork->scale;
    _drawDischargeCone(coord, effectWork->age, effectWork->angle, rgb);
    effectWork->angle += PE2_OPTION(DISCHARGE_CONE_HALF_EXTENT_STEP);
    nextBrightness     = effectWork->scale - PE2_OPTION(DISCHARGE_CONE_FADE_STEP);
    effectWork->scale  = nextBrightness;
    if (nextBrightness < PE2_OPTION(DISCHARGE_CONE_MIN_BRIGHTNESS)) {
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
static void _updateModelPose(Task* task)
{
    enum {
        MODEL_SCRATCH_BYTES = 16,
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

    SCRATCH_STACK_RESERVE_BYTES(MODEL_SCRATCH_BYTES);
    switch (task->spawnArg1.value & MODEL_COMPONENT_MASK) {
        case MODEL_ROOT:
            if (((GameActor*)playerTask->work)->mode != GAME_ACTOR_MODE_NORMAL || ((GameActor*)playerTask->work)->state != PLAYER_ACTOR_STATE_ATTACK) {
                task->spawnArg1.value = 0;
            }
            break;
        case MODEL_SLIDE:
            if (weaponTask->spawnArg1.value & MODEL_CHARGE_SLIDE) {
                if (task->killCountdown < MODEL_SLIDE_MAX_TICKS) {
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
            modelCoord->coord.t[1] = -task->killCountdown * MODEL_SLIDE_UNITS_PER_TICK;
            modelCoord->coord.t[2] = -0x16;
            break;
        case MODEL_HINGE:
            if (weaponTask->spawnArg1.value & MODEL_CHARGE_OPEN) {
                if (modelCoord->param.rot.vx >= MODEL_HINGE_LIMIT + 1) {
                    modelCoord->param.rot.vx = modelCoord->param.rot.vx - MODEL_HINGE_STEP;
                }
            } else if (modelCoord->param.rot.vx < 0) {
                modelCoord->param.rot.vx = modelCoord->param.rot.vx + MODEL_HINGE_STEP;
            }
            modelCoord->coord.t[0] = -0x14;
            modelCoord->coord.t[1] = -0x15C;
            modelCoord->coord.t[2] = 0xA8;

            gfxSetRotIdentity(&modelCoord->coord);
            RotMatrixX(modelCoord->param.rot.vx, &modelCoord->coord);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(MODEL_SCRATCH_BYTES);
}

/// Attaches a Hypervelocity slide or hinge to the weapon's transform and teardown tree.
///
/// Both tasks must be live TMD tasks with a root coordinate. `weaponCoord`
/// is the weapon's root captured before spawning; `weaponModel` supplies its
/// lighting. The newly spawned component's cache must still be dirty, and
/// its local pose stays intact. It borrows the parent coordinate and lighting
/// matrices, which must outlive their use.
/// Reparenting makes the weapon task responsible for the component's teardown.
static inline void _attachModelComponent(Task* weaponTask, Task* componentTask,
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
static void _initWeaponModel(Task* task)
{
    enum {
        MODEL_POSE_STATE = 1,
    };
    Task*      componentTask;
    TmdObject* weaponModel;
    GfxCoord*  weaponCoord;

    weaponModel               = task->extra.tmd;
    weaponCoord               = weaponModel->coords;
    task->state              += MODEL_POSE_STATE;
    task->exitCallback        = _releaseWeapon;
    task->killCountdown       = 0;
    weaponCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    weaponModel->flags        = 0;
    // Each child has its own transform under the root and shares its lighting.
    if (!(task->spawnArg1.value & MODEL_COMPONENT_MASK)) {
        componentTask = taskSpawn(TASK_BANK_MODEL, TASK_MODEL_HYPERVELOCITY_SLIDE, MODEL_SLIDE, 0);
        if (componentTask != NULL) {
            _attachModelComponent(task, componentTask, weaponCoord, weaponModel);
        }
        componentTask = taskSpawn(TASK_BANK_MODEL, TASK_MODEL_HYPERVELOCITY_HINGE, MODEL_HINGE, 0);
        if (componentTask != NULL) {
            _attachModelComponent(task, componentTask, weaponCoord, weaponModel);
            weaponCoord->coord.t[0] = -6;
            weaponCoord->coord.t[1] = -0x3C;
            weaponCoord->coord.t[2] = -0x16;
        }
    }
}

/// Selects weapon teardown for the next dispatch of the four-state weapon task.
static void _queueWeaponTeardown(Task* task)
{
    enum { WEAPON_STATE_TEARDOWN = 3 };

    task->state = WEAPON_STATE_TEARDOWN;
}

/// Releases a live hypervelocity weapon-model task and its child tasks.
///
/// Used both as the weapon's final state and its exit callback. Call once;
/// `taskKill`'s work ownership and deferred model release rules apply.
static void _releaseWeapon(Task* task)
{
    taskKill(task);
}

void hypervelocityWeaponModelTask(Task* task)
{
    TaskFunc stateHandlers[] = {
        _initWeaponModel,
        _updateModelPose,
        _queueWeaponTeardown,
        _releaseWeapon,
    };

    stateHandlers[task->state](task);
}

void hypervelocityAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE = 0,
        PHASE_CHARGE  = 1,
        PHASE_RECOIL  = 2,
        PHASE_RECOVER = 3,
    };
    _RecoilScratch* scratch;
    GameActor*      actor;
    GfxCoord*       rootCoord;
    Task*           weaponTask;
    s32             recoilDivisor;
    s32             chargeTicks;
    s32             recoilTicksLeft;

    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_RecoilScratch);
    actor      = playerTask->work;
    weaponTask = actor->equipmentTasks[1];
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            actor->mode                              = GAME_ACTOR_MODE_NORMAL;
            actor->state                             = PLAYER_ACTOR_STATE_ATTACK;
            actor->movementMode                      = GAME_ACTOR_MOVEMENT_STOPPED;
            actor->turnRateIndex                     = 0;
            actor->animationState                    = 0;
            actor->statePhase                        = PHASE_CHARGE;
            actor->weaponEffectTask->spawnArg1.value = CHARGE_REQUEST_START;
            actor->stateTimer                        = 0;
            weaponTask->spawnArg1.value             |= MODEL_CHARGE_SLIDE;
            worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_START), 0);
            worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_LOOP), 0);
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_RELOAD, 0, 3);
            /* fallthrough */
        case PHASE_CHARGE:
            if (actor->padHeld & (PAD_BUTTON_R1 | PAD_BUTTON_R2)) {
                chargeTicks       = actor->stateTimer + 1;
                actor->stateTimer = chargeTicks;
                if (chargeTicks >= PE2_OPTION(CHARGE_FRAMES)) {
                    actor->rumblePosted = 0;
                    actor->statePhase++;
                    weaponTask->spawnArg1.value = 0;
                    actor->stateTimer           = PE2_OPTION(RECOIL_FRAMES);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_HYPERVELOCITY), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_LOOP), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(SOUND_SHOT), 1);
                    playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE);
                } else if (chargeTicks == PE2_OPTION(SECOND_CHARGE_STAGE_FRAME)) {
                    weaponTask->spawnArg1.value |= MODEL_CHARGE_OPEN;
                    sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_START), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(SOUND_SECOND_STAGE), 0);
                }
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_CANCEL), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            } else {
                actor->statePhase                        = PHASE_RECOVER;
                actor->weaponEffectTask->spawnArg1.value = CHARGE_REQUEST_CANCEL;
                weaponTask->spawnArg1.value              = 0;
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_START), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_LOOP), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                worldCoordPlaySound(playerTask->extra.tmd->coords, HYPERVELOCITY_SOUND(HYPERVELOCITY_SOUND_CHARGE_CANCEL), 0);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_RELOAD, 0, 3);
            }
            break;
        case PHASE_RECOIL:
            // Push backward on the last eighteen ticks, with a stronger first step.
            recoilTicksLeft   = actor->stateTimer - 1;
            actor->stateTimer = recoilTicksLeft;
            if (recoilTicksLeft != 0) {
                if (recoilTicksLeft < PE2_OPTION(RECOIL_PUSH_WINDOW)) {
                    rootCoord     = playerTask->extra.tmd->coords;
                    recoilDivisor = PE2_OPTION(RECOIL_DIVISOR);
                    if (recoilTicksLeft == PE2_OPTION(RECOIL_FIRST_PUSH_FRAME)) {
                        recoilDivisor = PE2_OPTION(RECOIL_FIRST_PUSH_DIVISOR);
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
        case PHASE_RECOVER:
            if (_weaponIsArmSlotAdvancing(playerTask) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_RecoilScratch);
}
