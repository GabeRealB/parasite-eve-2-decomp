#include "actors/actor_800100.h"
#include "actors/companion.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "weapons/weapon.h"

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
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/pyke_flame.h"

static void _actor800100UpdateMove(Task* task);
static void _actor800100TickTextureSequences(Task* task);
static void _actor800100TickBehavior(Task* task);

/// Scratch-stack block of the aim beam: the coordinate the beam is drawn on.
///
/// The beam is a line from the equipped weapon along the aim, ended by a
/// textured square. `coord` is placed twice, each time by moving a node along
/// `offset`. The first move starts from a copy of the weapon model's root
/// node turned a quarter turn about X and gives the beam's start, which the
/// line is drawn from. The second moves `coord` along its own Y axis by
/// `contactDistance` plus 0x38, which is where the square is drawn.
///
/// The block lives for the one call that draws both parts. Nothing in the
/// package calls that routine.
typedef struct {
    GfxCoord coord;           // Node the beam is drawn on, parented to the view coordinate by each placement
    SVECTOR  offset;          // Displacement of the next placement, in the frame of the node it starts from; `pad` is never written
    u16      contactDistance; // X/Z distance from the beam's start to the aim capsule's contact; 0 while it has none
} _Actor800100AimBeamScratch;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamScratch, 0x5C);

/// Scratch-stack block for ranging the actor against the point it is steering toward.
///
/// Holds the point in room space and its displacement from the model's root
/// coordinate, whose X and Z give the planar distance the state handlers
/// compare with their reach limits. The point is also what the body-turn and
/// aim-turn helpers are handed. The block lives only for the one call.
typedef struct {
    VECTOR3 targetPoint; // Lock position of the actor's target node, or the player's root translation when it has none
    byte    field_C[4];  // Never accessed; role unproven
    VECTOR3 targetDelta; // `targetPoint` minus the root coordinate's translation
    byte    field_1C[4]; // Never accessed; role unproven
} _Actor800100TargetScratch;
STATIC_ASSERT_SIZEOF(_Actor800100TargetScratch, 0x20);

/// Scratch-stack block of the aim beam's line.
///
/// The line runs along the Y axis of the beam's coordinate, from its origin to
/// `tip`. Both ends go through that coordinate's composed matrix, one
/// perspective transform each, and the screen points become the ends of an
/// additive gouraud line. The line is drawn only when `otz` is at least 0x20.
typedef struct {
    DVECTOR originScreen; // Screen X/Y of `origin`
    DVECTOR tipScreen;    // Screen X/Y of `tip`
    s32     otz;          // SZ3 / 4 of the tip's transform; ordering-table depth and the blend packet's depth
    SVECTOR origin;       // Near end in the coordinate's frame, always (0, 0, 0); `pad` is never written
    SVECTOR tip;          // Far end: (0, length, 0), the contact distance or the equipped weapon's reach when that is 0
} _Actor800100AimBeamLineScratch;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamLineScratch, 0x1C);

/// One corner of the aim beam's end square, as an offset from the
/// translation of the beam's coordinate. The X offset is always 0, so a
/// corner is stored as its other two components.
typedef struct {
    s16 vy; // Y offset
    s16 vz; // Z offset
} _Actor800100AimBeamQuadCorner;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamQuadCorner, 4);

/// Scratch-stack block of the aim beam's end square.
///
/// Each corner is a `_Actor800100AimBeamQuadCorner` added to the translation
/// of the beam coordinate's composed matrix; the coordinate's rotation is not
/// applied, so the square keeps one orientation. The corners are projected
/// through `GsWSMATRIX`, corner 0 with one perspective transform and corners
/// 1..3 with a three-vertex one, onto an additive textured quad. Corners and
/// screen points share indices 0..3 in GPU quad strip order.
typedef struct {
    DVECTOR screenCorners[4]; // Screen X/Y of each corner
    s32     otz;              // SZ3 / 4 of the three-vertex transform, corner 3's depth; selects the ordering-table bucket
    VECTOR  cornerOffset;     // Corner being staged, as (0, vy, vz); `pad` is never written
    SVECTOR worldCorners[4];  // Corner positions handed to the projection, cut to 16 bits; `pad` is never written
} _Actor800100AimBeamQuadScratch;
STATIC_ASSERT_SIZEOF(_Actor800100AimBeamQuadScratch, 0x44);

/// NULL-terminated `GpuImageUpload*` frame lists for `_actor800100TickTextureSequences`,
/// indexed `table[textureSequenceA - 1][textureFrameA]`; `D_actor_800100_80167210` is
/// the `textureSequenceB` sequence.
extern GpuImageUpload** D_actor_800100_80167200[];
extern GpuImageUpload** D_actor_800100_80167210[];

/// Translation the flare's own coordinate starts at, `(0, 0x200, 0x40)`.
extern SVECTOR D_actor_800100_80167128;

extern TaskMessageEntry D_actor_800100_80167130[26];
extern s16              D_actor_800100_80167218[];
extern s16              D_actor_800100_80167224[];
extern u8               D_actor_800100_80167230[];

static void func_actor_800100_80163214(Task* arg0);
static void _actor800100Teardown(Task* task);
static void _actor800100DecideIdleBehavior(Task* task);
static void _actor800100TickNativeMode(Task* task);
static void _actor800100FollowPlayerState(Task* task);
static void _actor800100TurnToTargetState(Task* task);
static void _actor800100CombatEntryState(Task* task);
static void _actor800100AttackLoopState(Task* task);
static void _actor800100RetreatState(Task* task);
static void _actor800100ApproachState(Task* task);
static void _actor800100ReloadState(Task* task);
static void _actor800100ObstacleScanState(Task* task);
static void _actor800100WanderState(Task* task);
static void _actor800100EnterCombatEntry(Task* task);
static void _actor800100EnterApproach(Task* task);
static void _actor800100EnterObstacleScan(Task* task);
static void _actor800100EnterFollowPlayer(Task* task);
static void _actor800100EnterTurnToPlayer(Task* task);
static void _actor800100EnterWander(Task* task);
static void _actor800100IdleState(Task* task);
static void _actor800100CombatDecisionState(Task* task);
static void _actor800100CombatExitState(Task* task);
static void _actor800100DamageMode(Task* task);
static void _actor800100DamageRecoveryState(Task* task);
static void _actor800100StoppedDamageState(Task* unusedTask);
static void _actor800100TickScriptedMode(Task* task);
static void _actor800100DecideCombatBehavior(Task* task);
static void func_actor_800100_80165C38(Task* arg0);
static void func_actor_800100_80165DE8(Task* arg0);
static void func_actor_800100_80165F50(Task* arg0);
static void func_actor_800100_80166190(Task* arg0);
static void _actor800100DrawAimBeamLine(const GfxCoord* beamCoord, s16 contactDistance);
static void _actor800100DrawAimBeamQuad(const GfxCoord* beamCoord);
static s32  func_actor_800100_80166B40(WorldCollisionContact* arg0, GfxCoord* arg1, GfxCoord* arg2);
static void _actor800100EnterAttackLoop(Task* task);
static void _actor800100EnterRetreat(Task* task);
static void _actor800100EnterCombatExit(Task* task);
static void _actor800100TickWeaponAttack(Task* task);
static s32  _actor800100GetContactDistance(const GfxCoord* originCoord, const WorldCollisionContact* contact, u16 contactZY[2]);

/// Animation controller 0 leaves the behavior phase alone; turn-rate row 1 steps 64/4096 turns per tick.
enum {
    ACTOR_800100_ANIMATION_CONTROLLER_NONE = 0,
    ACTOR_800100_TURN_RATE_64              = 1
};

/// Native armed-companion behavior, motion, animation-set and blend selectors.
enum {
    ACTOR_800100_STATE_COMBAT_ENTRY            = 3,
    ACTOR_800100_STATE_COMBAT_DECISION         = 4,
    ACTOR_800100_STATE_OBSTACLE_SCAN           = 10,
    ACTOR_800100_MOVEMENT_RUN                  = 3,
    ACTOR_800100_TURN_STOPPED                  = 0,
    ACTOR_800100_MOVE_FORWARD                  = 1,
    ACTOR_800100_TURN_NEGATIVE                 = -1,
    ACTOR_800100_TURN_POSITIVE                 = 1,
    ACTOR_800100_ANIMATION_IDLE                = 1,
    ACTOR_800100_ANIMATION_RUN                 = 4,
    ACTOR_800100_ANIMATION_TURN_LEFT           = 5,
    ACTOR_800100_ANIMATION_TURN_RIGHT          = 6,
    ACTOR_800100_ANIMATION_COMBAT_HOLD         = 9,
    ACTOR_800100_ANIMATION_CONTROLLER_CLIP_END = 7,
    ACTOR_800100_MOVEMENT_BLEND_FRAMES         = 5,
    ACTOR_800100_COMBAT_BLEND_FRAMES           = 6,
};

extern u8* D_actor_800100_801672F8[];
extern u8  D_actor_800100_80167308[];
extern u8  D_actor_800100_80167310[];

extern GpuImageUpload* D_actor_800100_80167A18[2];
extern GpuImageUpload* D_actor_800100_80167A20[4];
extern GpuImageUpload* D_actor_800100_80167A30[4];
extern GpuImageUpload* D_actor_800100_80167A40[6];
extern GpuImageUpload* D_actor_800100_80167A58[2];
extern GpuImageUpload* D_actor_800100_80167A60[2];

SVECTOR D_actor_800100_80167128 = { 0, 512, 64, 0 };

TaskMessageEntry D_actor_800100_80167130[26] = {
    { ANIMATION_MESSAGE_PLAY, companionPlayScriptedAnimation },
    { 1002, companionPlayScriptedAnimation },
    { 1003, companionPlayScriptedAnimation },
    { 1004, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_PLACE, playerActorPlace },
    { ANIMATION_MESSAGE_IS_PLAYING, playerActorIsAnimationPlaying },
    { GAME_ACTOR_MESSAGE_TURN_TO_YAW, companionTurnToYaw },
    { GAME_ACTOR_MESSAGE_CLIMB_STAIRS, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_END_SCRIPTED, companionEndScriptedMotion },
    { GAME_ACTOR_MESSAGE_MOVE_TO, companionMoveTo },
    { GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, playerActorSetModelDraw },
    { ANIMATION_MESSAGE_INSTALL_AND_PLAY, companionInstallScriptedAnimation },
    { GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, playerActorAttachToCoord },
    { GAME_ACTOR_MESSAGE_WALK_STEPS, playerActorWalkSteps },
    { ANIMATION_MESSAGE_COPY_BANK_EXTENSION, animationCopyCompanionBankExtension },
    { GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, companionAwaitButtonPresses },
    { GAME_ACTOR_MESSAGE_APPLY_DAMAGE, companionApplyDamage },
    { 1018, companionPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_RUN_TO, companionPlayScriptedAnimation },
    { 1020, companionPlayScriptedAnimation },
    { ANIMATION_MESSAGE_SET_RATE, playerActorSetAnimationRate },
    { GAME_ACTOR_MESSAGE_MOVE_BY, companionMoveBy },
    { ANIMATION_MESSAGE_REPLACE_AND_PLAY, companionEndScriptedMotion },
    { 1024, companionEndScriptedMotion },
    { GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, playerActorSetTextureSequence },
};

GpuImageUpload** D_actor_800100_80167200[4] = {
    D_actor_800100_80167A18,
    D_actor_800100_80167A20,
    D_actor_800100_80167A30,
    D_actor_800100_80167A40,
};

GpuImageUpload** D_actor_800100_80167210[2] = {
    D_actor_800100_80167A60,
    D_actor_800100_80167A58,
};

s16 D_actor_800100_80167218[6] = {
    0,
    5,
    12,
    3,
    28,
    0,
};

s16 D_actor_800100_80167224[6] = {
    0,
    3,
    0,
    3,
    16,
    0,
};

u8 D_actor_800100_80167230[8] = {
    12,
    12,
    12,
    100,
    40,
    0,
    0,
    0,
};

u8 D_actor_800100_80167238[48] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800100_80167268[48] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800100_80167298[48] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
};

u8 D_actor_800100_801672C8[48] = {
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    3,
    3,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
};

u8* D_actor_800100_801672F8[4] = {
    D_actor_800100_80167238,
    D_actor_800100_80167268,
    D_actor_800100_80167298,
    D_actor_800100_801672C8,
};

u8 D_actor_800100_80167308[8] = {
    0,
    0,
    0,
    0,
    3,
    4,
    4,
    4,
};

u8 D_actor_800100_80167310[8] = {
    1,
    1,
    2,
    2,
    3,
    3,
    4,
    4,
};

u_long D_actor_800100_80167318[84] = {
#include "assets/actor_800100_image_054F8.inc"
};

GpuImageUpload D_actor_800100_80167468[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 21, 8 }, D_actor_800100_80167318 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_80167488[84] = {
#include "assets/actor_800100_image_05668.inc"
};

GpuImageUpload D_actor_800100_801675D8[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 21, 8 }, D_actor_800100_80167488 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_801675F8[84] = {
#include "assets/actor_800100_image_057D8.inc"
};

GpuImageUpload D_actor_800100_80167748[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 21, 8 }, D_actor_800100_801675F8 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_80167768[78] = {
#include "assets/actor_800100_image_05948.inc"
};

GpuImageUpload D_actor_800100_801678A0[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 13, 12 }, D_actor_800100_80167768 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800100_801678C0[78] = {
#include "assets/actor_800100_image_05AA0.inc"
};

GpuImageUpload D_actor_800100_801679F8[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, 13, 12 }, D_actor_800100_801678C0 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

GpuImageUpload* D_actor_800100_80167A18[2] = {
    D_actor_800100_80167468,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A20[4] = {
    D_actor_800100_80167468,
    D_actor_800100_801675D8,
    D_actor_800100_80167748,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A30[4] = {
    D_actor_800100_80167748,
    D_actor_800100_801675D8,
    D_actor_800100_80167468,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A40[6] = {
    D_actor_800100_80167468,
    D_actor_800100_801675D8,
    D_actor_800100_80167748,
    D_actor_800100_801675D8,
    D_actor_800100_80167468,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A58[2] = {
    D_actor_800100_801678A0,
    NULL,
};

GpuImageUpload* D_actor_800100_80167A60[2] = {
    D_actor_800100_801679F8,
    NULL,
};

static void _actor800100ScheduleTeardown(Task* task);
static void _actor800100DrawAimBeam(Task* task);
static void _actor800100InitAimCollision(Task* actorTask);

/// Refreshes the emitter's transient light in the view coordinate's local frame.
///
/// Borrows the chosen pool slot and its embedded light/coordinate aliases.
/// Radii use integer world units; red intensity has 12 fractional bits. Consumes
/// one unsigned LCG sample and retains halfword narrowing before green/blue shifts.
static inline void _actor800100RefreshPykeLight(WorldCoordTransientPointLight* transientLight,
                                                WorldCoordPointLight* pointLight, GfxCoord* lightCoord,
                                                const GfxCoord* nozzleCoord, s32 innerRadius, s32 outerRadius, s32 redMinimum)
{
    enum {
        ACTOR_800100_PYKE_LIGHT_FRAMES      = 4,
        ACTOR_800100_PYKE_LIGHT_RANDOM_MASK = 0x700,
    };
    u32 lightRandom;

    transientLight->framesLeft = ACTOR_800100_PYKE_LIGHT_FRAMES;
    pointLight->inner          = innerRadius;
    pointLight->outer          = outerRadius;
    lightRandom                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState            = lightRandom;
    // Green halves the unsigned red halfword; blue quarters its signed value.
    pointLight->head.color.r = ((lightRandom >> 16) & ACTOR_800100_PYKE_LIGHT_RANDOM_MASK) + redMinimum;
    pointLight->head.color.g = (u16)pointLight->head.color.r >> 1;
    pointLight->head.color.b = pointLight->head.color.r >> 2;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &nozzleCoord->workm, &lightCoord->coord);
    lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void actor800100PykeEmitterTask(Task* task)
{
    enum {
        ACTOR_800100_PYKE_STATE_INIT       = 0,
        ACTOR_800100_PYKE_STATE_UPDATE     = 1,
        ACTOR_800100_PYKE_LIGHT_SLOT       = 3,
        ACTOR_800100_PYKE_SPEED_STEP       = 64,
        ACTOR_800100_PYKE_SPEED_MAX        = 384,
        ACTOR_800100_PYKE_IDLE_LIGHT_INNER = 128,
        ACTOR_800100_PYKE_IDLE_LIGHT_OUTER = 1024,
        ACTOR_800100_PYKE_FIRE_LIGHT_INNER = 1024,
        ACTOR_800100_PYKE_FIRE_LIGHT_OUTER = 16384,
        ACTOR_800100_PYKE_IDLE_RED_MIN     = 0x400,
        ACTOR_800100_PYKE_FIRE_RED_MIN     = 0x800,
    };
    EffectWork*                    effectWork;
    GfxCoord*                      nozzleCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    GfxCoord*                      lightCoord;
    EffectWork*                    flameWork;

    effectWork     = task->spawnArg2.pointer;
    nozzleCoord    = task->extra.coordBody->coord;
    transientLight = &gWorldCoordTransientPointLights[ACTOR_800100_PYKE_LIGHT_SLOT];
    lightCoord     = &transientLight->light.head.transform.coord;
    pointLight     = &transientLight->light;
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return;
    }
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }
    effectWork->age++;
    switch (task->state) {
        case ACTOR_800100_PYKE_STATE_INIT:
            nozzleCoord->parent = effectWork->parent;
            gfxSetRotIdentity(&nozzleCoord->coord);
            nozzleCoord->coord.t[0]   = D_actor_800100_80167128.vx;
            nozzleCoord->coord.t[1]   = D_actor_800100_80167128.vy;
            nozzleCoord->coord.t[2]   = D_actor_800100_80167128.vz;
            nozzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(nozzleCoord);
            task->state = ACTOR_800100_PYKE_STATE_UPDATE;
            break;
        case ACTOR_800100_PYKE_STATE_UPDATE:
            actorRenderComposeCoord(nozzleCoord);
            switch (task->spawnArg1.value) {
                case ACTOR_800100_PYKE_OFF:
                    break;
                case ACTOR_800100_PYKE_IDLE:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        effectWork->age--;
                        _pykeFlameDrawNozzle(
                            MATRIX_TRANS(&nozzleCoord->workm), effectWork->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                        break;
                    }
                    _pykeFlameDrawNozzle(
                        MATRIX_TRANS(&nozzleCoord->workm), effectWork->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                    _actor800100RefreshPykeLight(transientLight, pointLight, lightCoord, nozzleCoord,
                                                 ACTOR_800100_PYKE_IDLE_LIGHT_INNER, ACTOR_800100_PYKE_IDLE_LIGHT_OUTER,
                                                 ACTOR_800100_PYKE_IDLE_RED_MIN);
                    effectWork->scale = ACTOR_800100_PYKE_SPEED_STEP;
                    break;
                case ACTOR_800100_PYKE_FIRE:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        effectWork->age--;
                        break;
                    }
                    // Launch speed grows independently of each child's subsequent motion.
                    if (effectWork->scale < ACTOR_800100_PYKE_SPEED_MAX) {
                        effectWork->scale = effectWork->scale + ACTOR_800100_PYKE_SPEED_STEP;
                    }
                    flameWork = effectSpawn(EFFECT_ACTOR_800100_PYKE_FLAME, nozzleCoord, (s32)effectWork->scale, NULL);
                    if (flameWork != NULL) {
                        taskReparent(task, flameWork->task);
                    }
                    _actor800100RefreshPykeLight(transientLight, pointLight, lightCoord, nozzleCoord,
                                                 ACTOR_800100_PYKE_FIRE_LIGHT_INNER, ACTOR_800100_PYKE_FIRE_LIGHT_OUTER,
                                                 ACTOR_800100_PYKE_FIRE_RED_MIN);
                    break;
                case ACTOR_800100_PYKE_RESET_IDLE:
                    task->spawnArg1.value = ACTOR_800100_PYKE_IDLE;
                    break;
                case ACTOR_800100_PYKE_RESET_OFF:
                    task->spawnArg1.value = ACTOR_800100_PYKE_OFF;
                    break;
                case ACTOR_800100_PYKE_RELEASE:
                    effectKillTask(effectWork, task);
                    break;
            }
            break;
    }
}

#include "../../shared/pyke_flame_nozzle.inc.c"

#define PYKE_FLAME_KEY                  0x21C9E
#define PYKE_FLAME_REDRAW_UPDATES_COORD 1
#include "../../shared/pyke_flame_task.inc.c"

void actor800100PykeFlameTask(Task* task)
{
    _pykeFlameTask(task);
}

#include "../../shared/pyke_flame_blob.inc.c"

#include "../../shared/pyke_flame_splash.inc.c"

#include "../../shared/pyke_flame_release.inc.c"

static void func_actor_800100_80163214(Task* arg0)
{
    GameActor*             actor;
    TmdObject*             extra;
    GfxCoord*              coord;
    GfxCoord*              next;
    GfxCoord*              third;
    McSaveData*            save;
    WorldCollisionContact* recs;
    WorldCollisionBody*    obj;
    CompanionWork*         companion;
    EffectWork*            eff;
    Task*                  task;
    SVECTOR3*              scratch;
    s32                    idx;
    s32                    packed;
    u8                     savedResourceVariant;
    void*                  head;

    actor                      = arg0->work;
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = head - 8;
    scratch                    = (SVECTOR3*)(head - 8);
    extra                      = arg0->extra.tmd;
    coord                      = extra->coords;
    arg0->state++;
    arg0->msgTable                                 = D_actor_800100_80167130;
    arg0->exitCallback                             = _actor800100Teardown;
    actor->animationSlotCount                      = GAME_ACTOR_ARMED_COMPANION_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = arg0;
    coord->parent                                  = &gGfxViewCoord;
    coord->composeStamp                            = GRAPHICS_COORD_DIRTY;
    extra->flags                                   = 0;
    RotMatrix(&actor->rotation, &coord->coord);
    companionInitNativeAnimation(arg0);
    actor->animationRate = ANIMATION_RATE_ONE;
    playerActorResetChildSlots(arg0, actor->actionArgument);
    playerActorTickChildSlots(arg0);
    recs                                       = actor->collisionContacts;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    actor->previousPosition.vx                 = coord->coord.t[0];
    actor->previousPosition.vy                 = coord->coord.t[1];
    actor->previousPosition.vz                 = coord->coord.t[2];
    obj->context.motion                        = &actor->collisionMotionContexts[0];
    obj->coord                                 = coord;
    actor->collisionMotionContexts[0].contacts = recs;
    save                                       = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    obj->pos.vy                                = -0x12C;
    obj->pos.vx                                = 0;
    obj->pos.vz                                = 0;
    packed                                     = 0x10000;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0x12C;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed | 0x80;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    worldCollisionInitContacts(actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    obj->flags                                |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    next                                       = arg0->extra.tmd->coords + 4;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    obj->context.motion                        = &actor->collisionMotionContexts[1];
    obj->coord                                 = next;
    actor->collisionMotionContexts[1].contacts = recs;
    obj->pos.vx                                = 0;
    obj->pos.vy                                = 0x64;
    obj->pos.vz                                = 0;
    {
        s32 f = 0x14;
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xDC;
        obj->flags  = f;
        obj->key    = temp | packed | 0x80;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    obj->flags                                |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    obj                                        = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    third                                      = arg0->extra.tmd->coords;
    obj->context.motion                        = &actor->collisionMotionContexts[2];
    obj->coord                                 = third + 1;
    actor->collisionMotionContexts[2].contacts = recs;
    obj->pos.vx                                = 0;
    obj->pos.vy                                = 0x52;
    obj->pos.vz                                = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xDC;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed | 0x80;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, obj);
    }
    obj->flags                   |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->collisionEnableMask    = GAME_ACTOR_COLLISION_REQUEST_MASK;
    savedResourceVariant          = gPlayerStatus.resourceVariant;
    gPlayerStatus.resourceVariant = save->state.companionVariant;
    actor->attachmentTasks[0]     = playerActorSpawnAttachment(arg0, 0, PLAYER_ACTOR_ATTACHMENT_COMPANION_RIG, PLAYER_ACTOR_ATTACHMENT_SECOND_PAIR);
    actor->attachmentTasks[1]     = playerActorSpawnAttachment(arg0, 1, PLAYER_ACTOR_ATTACHMENT_COMPANION_RIG, PLAYER_ACTOR_ATTACHMENT_SECOND_PAIR);
    gPlayerStatus.resourceVariant = savedResourceVariant;
    if (actor->attachmentTasks[1] != NULL) {
        task                     = playerActorSpawnWeaponModel(actor->attachmentTasks[1], save->state.companionType + 1, save->state.companionVariant, 0);
        actor->equipmentTasks[1] = task;
        if (task != NULL) {
            companion = actor->companionWork;
            idx       = D_actor_800100_80167218[save->state.companionVariant];
            playerActorInitWeaponCollision(arg0, idx, D_actor_800100_80167224[save->state.companionVariant]);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key |= 0x80;
            companion->activity.combat.attacksRemaining         = D_actor_800100_80167230[save->state.companionVariant];
            if ((u8)save->state.companionVariant == 4) {
                eff = effectSpawn((EFFECT_COMPANION_WEAPON_FLARE | EFFECT_SPAWN_UNLIMITED), actor->equipmentTasks[1]->extra.tmd->coords, idx, 0);
                if (eff != NULL) {
                    actor->weaponEffectTask = eff->task;
                    taskReparent(arg0, eff->task);
                    playerActorResetWeaponAttack(arg0, idx, 0);
                }
            }
        }
    }
    scratch->vx = 0;
    scratch->vy = -0x200;
    scratch->vz = 0;
    companionBindCollisionProbe(arg0, scratch, 0x1000);
    companionSetDecisionDelay(arg0, 0x3C, 0x7F);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Restores the root translation from the position accepted before grid response.
static inline void _actor800100RestorePreviousPosition(GfxCoord* rootCoord, const GameActor* actor)
{
    rootCoord->coord.t[0] = actor->previousPosition.vx;
    rootCoord->coord.t[1] = actor->previousPosition.vy;
    rootCoord->coord.t[2] = actor->previousPosition.vz;
}

/// Publishes a Q12 movement or pushback heading to all three motion contexts.
///
/// Requires the composed root basis and a live caller-owned scratch block.
/// Only XYZ halfwords are staged and copied; vector pads remain untouched.
static inline void _actor800100PublishMotionDirection(GameActor* actor, const GfxCoord* rootCoord, CompanionMoveScratch* block)
{
    if (actor->usesPushbackDirection != 0) {
        block->motionDirection.vx = actor->pushbackDirection.vx;
        block->motionDirection.vy = actor->pushbackDirection.vy;
        block->motionDirection.vz = actor->pushbackDirection.vz;
    } else {
        block->motionDirection.vx = rootCoord->workm.m[0][2] * actor->movementSign;
        block->motionDirection.vy = rootCoord->workm.m[1][2] * actor->movementSign;
        block->motionDirection.vz = rootCoord->workm.m[2][2] * actor->movementSign;
    }
    actor->collisionMotionContexts[0].motionDirection.vx = block->motionDirection.vx;
    actor->collisionMotionContexts[0].motionDirection.vy = block->motionDirection.vy;
    actor->collisionMotionContexts[0].motionDirection.vz = block->motionDirection.vz;
    actor->collisionMotionContexts[1].motionDirection.vx = block->motionDirection.vx;
    actor->collisionMotionContexts[1].motionDirection.vy = block->motionDirection.vy;
    actor->collisionMotionContexts[1].motionDirection.vz = block->motionDirection.vz;
    actor->collisionMotionContexts[2].motionDirection.vx = block->motionDirection.vx;
    actor->collisionMotionContexts[2].motionDirection.vy = block->motionDirection.vy;
    actor->collisionMotionContexts[2].motionDirection.vz = block->motionDirection.vz;
}

/// Applies collision response, advances the companion, publishes motion and draws its shadow.
///
/// Requires initialized actor/companion work, linked bodies, live native/scripted
/// playback and room/view resources. Borrows one CompanionMoveScratch until return.
/// Rejects height jumps of at least 512 game units outside scripted mode. Probe
/// yaw uses 4096 units per turn; motion headings are Q12. Frozen behavior still
/// updates textures, clears the preceding contacts and refreshes transforms.
static void _actor800100UpdateMove(Task* task)
{
    enum {
        ACTOR_800100_MAX_VERTICAL_STEP      = 512,
        ACTOR_800100_ROOT_FLOOR_LIFT        = 8,
        ACTOR_800100_SHADOW_HALF_SIZE       = 512,
        ACTOR_800100_WEAPON_COLLISION_PITCH = -ACTOR_TRANSFORM_ANGLE_TURN / 4,
    };
    CompanionMoveScratch* block;
    void**                cursorSlot;
    CompanionMoveScratch* blockEnd;
    GameActor*            actor;
    TmdObject*            coordsModel;
    TmdObject*            model;
    GfxCoord*             rootCoord;
    GfxCoord*             shadowCoord;
    CompanionWork*        companion;
    Task*                 weaponTask;
    WorldCollisionBody*   bodies[2];
    s32                   verticalDelta;
    s32                   bodyIndex;
    s8                    updateRequests;

    cursorSlot                                        = SCRATCH_HEAD_ADDR;
    blockEnd                                          = SCRATCH_HEAD_AT(cursorSlot, CompanionMoveScratch);
    model                                             = task->extra.tmd;
    SCRATCH_HEAD_AT(cursorSlot, CompanionMoveScratch) = blockEnd - 1;
    // The two model handles retain the entry loads around the scratch reservation.
    coordsModel = model;
    block       = blockEnd - 1;
    rootCoord   = coordsModel->coords;
    actor       = task->work;
    companion   = actor->companionWork;

    // Consume the preceding collision pass before behavior changes the root.
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        (verticalDelta = rootCoord->coord.t[1], verticalDelta = verticalDelta - actor->previousPosition.vy, verticalDelta = ABS(verticalDelta), verticalDelta >= ACTOR_800100_MAX_VERTICAL_STEP)) {
        _actor800100RestorePreviousPosition(rootCoord, actor);
    } else {
        actor->previousPosition.vx = rootCoord->coord.t[0];
        actor->previousPosition.vy = rootCoord->coord.t[1];
        actor->previousPosition.vz = rootCoord->coord.t[2];
        if (actor->collisionEnableMask & (1 << GAME_ACTOR_BODY_ROOT)) {
            actor->gridResponse = worldCollisionApplyResponsePushback(rootCoord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
        } else {
            actor->gridResponse = WORLD_COLLISION_PUSHBACK_NO_GRID_HIT;
        }
    }

    weaponTask = actor->equipmentTasks[1];
    if (weaponTask != NULL) {
        actor->weaponCollisionCoord = *weaponTask->extra.tmd->coords;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, ACTOR_800100_WEAPON_COLLISION_PITCH, GRAPHICS_ROTATION_COMPOSE);
    }

    companion->probe.coord = *task->extra.tmd->coords;
    gfxRotMatrixY(&companion->probe.coord.workm, companion->scanAngle, GRAPHICS_ROTATION_COMPOSE);

    bodies[0] = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    bodies[1] = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    for (bodyIndex = 0; bodyIndex < (s32)ARRAY_SIZE(bodies); bodyIndex++) {
        updateRequests = actor->pendingCollisionUpdates;
        if ((updateRequests >> bodyIndex) & 1) {
            actor->collisionEnableMask |= 1 << bodyIndex;
            bodies[bodyIndex]->flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (updateRequests & ((1 << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT) << bodyIndex)) {
            actor->collisionEnableMask &= ~(1 << bodyIndex);
            bodies[bodyIndex]->flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;

    if (D_80115768 == 0 && gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        _actor800100TickBehavior(task);
    }
    _actor800100TickTextureSequences(task);

    // Contacts are consumed above; the next pass starts from fresh tables.
    worldCollisionClearContacts(actor->collisionContacts);
    worldCollisionClearContacts(actor->companionWork->probe.contacts);
    if (actor->equipmentTasks[1] != NULL) {
        worldCollisionClearContacts(actor->weaponContacts);
    }
    if (actor->collisionEnableMask & (1 << GAME_ACTOR_BODY_ROOT)) {
        rootCoord->coord.t[1] += ACTOR_800100_ROOT_FLOOR_LIFT;
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);

    _actor800100PublishMotionDirection(actor, rootCoord, block);

    if (!(coordsModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        shadowCoord               = task->extra.tmd->coords + 1;
        shadowCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(shadowCoord);
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&shadowCoord->workm), &block->shadowCentre) != 0) {
            effectDrawGroundShadow(&block->shadowCentre, ACTOR_800100_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(CompanionMoveScratch);
}

/// Posts one texture frame using a borrowed scratch rectangle.
///
/// Requires a live actor/model texture page and a writable operation-terminated
/// upload list. X counts two positions per VRAM word; width counts whole words
/// and Y/height count rows. The actor upload adds its page/VRAM base to this
/// relative rectangle. The GPU queue copies rectangles during this call and
/// borrows pixel storage until transfer completion; the scratch RECT is reusable.
static inline void _actor800100UploadTextureFrame(Task* task, GpuImageUpload* frameUploads, RECT* textureRect,
                                                  s16 xHalfWords, s16 yRows, s16 widthWords, s16 heightRows)
{
    textureRect->x = xHalfWords;
    textureRect->y = yRows;
    textureRect->w = widthWords;
    textureRect->h = heightRows;
    actorRenderUploadTexture(task, frameUploads, textureRect);
}

/// Advances the armed companion's two independent texture sequences.
///
/// Requires live actor/model resources, A selectors 1..4 and B selectors 1..2
/// (zero disables a sequence), and frame lists ending before signed-byte wrap.
/// Delay and frame bytes are read as s8. Reloads delays to four/eight actor ticks
/// after each upload and borrows one RECT from scratch; pixels stay live until
/// the queued GPU transfer completes.
static void _actor800100TickTextureSequences(Task* task)
{
    enum {
        ACTOR_800100_TEXTURE_SEQUENCE_OFF   = 0,
        ACTOR_800100_TEXTURE_A_DELAY_TICKS  = 4,
        ACTOR_800100_TEXTURE_B_DELAY_TICKS  = 8,
        ACTOR_800100_TEXTURE_A_X_HALF_WORDS = 0,
        ACTOR_800100_TEXTURE_A_Y_ROWS       = 40,
        ACTOR_800100_TEXTURE_A_WIDTH_WORDS  = 21,
        ACTOR_800100_TEXTURE_A_HEIGHT_ROWS  = 8,
        ACTOR_800100_TEXTURE_B_X_HALF_WORDS = 8,
        ACTOR_800100_TEXTURE_B_Y_ROWS       = 64,
        ACTOR_800100_TEXTURE_B_WIDTH_WORDS  = 13,
        ACTOR_800100_TEXTURE_B_HEIGHT_ROWS  = 12,
    };
    void**            scratchCursor;
    RECT*             previousHead;
    RECT*             reservedRect;
    RECT*             textureRect;
    GameActor*        actor;
    GpuImageUpload*** frameLists;
    s32               sequenceIndex;
    u32               sequenceRow;
    GpuImageUpload*   frameUploads;

    scratchCursor                        = SCRATCH_HEAD_ADDR;
    previousHead                         = SCRATCH_HEAD_AT(scratchCursor, RECT);
    actor                                = task->work;
    reservedRect                         = previousHead - 1;
    SCRATCH_HEAD_AT(scratchCursor, RECT) = reservedRect;
    textureRect                          = reservedRect;

    if ((s8)actor->textureSequenceA != ACTOR_800100_TEXTURE_SEQUENCE_OFF) {
        actor->textureDelayA--;
        if ((s8)actor->textureDelayA <= 0) {
            frameLists    = D_actor_800100_80167200;
            sequenceIndex = (s8)actor->textureSequenceA - 1;
            frameUploads  = frameLists[sequenceIndex][(s8)actor->textureFrameA];
            if (frameUploads != NULL) {
                _actor800100UploadTextureFrame(task, frameUploads, textureRect, ACTOR_800100_TEXTURE_A_X_HALF_WORDS, ACTOR_800100_TEXTURE_A_Y_ROWS, ACTOR_800100_TEXTURE_A_WIDTH_WORDS, ACTOR_800100_TEXTURE_A_HEIGHT_ROWS);
                actor->textureDelayA = ACTOR_800100_TEXTURE_A_DELAY_TICKS;
                actor->textureFrameA++;
            } else {
                actor->textureSequenceA = ACTOR_800100_TEXTURE_SEQUENCE_OFF;
            }
        }
    }

    if ((s8)actor->textureSequenceB != ACTOR_800100_TEXTURE_SEQUENCE_OFF) {
        actor->textureDelayB--;
        if ((s8)actor->textureDelayB <= 0) {
            frameLists   = D_actor_800100_80167210;
            sequenceRow  = (s8)actor->textureSequenceB - 1;
            frameUploads = frameLists[sequenceRow][(s8)actor->textureFrameB];
            if (frameUploads != NULL) {
                _actor800100UploadTextureFrame(task, frameUploads, textureRect, ACTOR_800100_TEXTURE_B_X_HALF_WORDS, ACTOR_800100_TEXTURE_B_Y_ROWS, ACTOR_800100_TEXTURE_B_WIDTH_WORDS, ACTOR_800100_TEXTURE_B_HEIGHT_ROWS);
                actor->textureDelayB = ACTOR_800100_TEXTURE_B_DELAY_TICKS;
                actor->textureFrameB++;
            } else {
                actor->textureSequenceB = ACTOR_800100_TEXTURE_SEQUENCE_OFF;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(RECT);
}

/// Schedules companion teardown for the next tick of its main task.
static void _actor800100ScheduleTeardown(Task* task)
{
    enum { ACTOR_800100_TASK_TEARDOWN = 3 };

    task->state = ACTOR_800100_TASK_TEARDOWN;
}

/// Releases the armed companion's attached tasks and linked collision bodies.
///
/// Requires live actor/companion work and live child tasks for each occupied
/// slot. Clears the companion registry before killing children and unlinking
/// the four actor bodies and forward probe. Task teardown releases actor work
/// and schedules model release; callers must not reuse the actor afterward.
static void _actor800100Teardown(Task* task)
{
    GameActor*     actor;
    CompanionWork* companion;
    Task*          childTask;

    actor                                          = task->work;
    companion                                      = actor->companionWork;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] = NULL;
    childTask                                      = actor->weaponEffectTask;
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->equipmentTasks[0];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->equipmentTasks[1];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->attachmentTasks[0];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->attachmentTasks[1];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART4]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART1]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_WEAPON]);
    worldCollisionUnlinkBody(&companion->probe.body);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state.
static const TaskFuncTable4 D_actor_800100_80161E3C = { {
    func_actor_800100_80163214,
    _actor800100UpdateMove,
    _actor800100ScheduleTeardown,
    _actor800100Teardown,
} };

void actor800100Task(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_800100_80161E3C;
    handlers.funcs[task->state](task);
}

/// Chooses following, an idle variation, wandering or a turn toward the player.
///
/// Requires live companion work, native animation resources and player/root
/// coordinates in their common room frame. Decisions run when the signed
/// timer expires and re-arm it for 10..41 active behavior ticks. Distance uses
/// game units; yaw uses 4096 units per turn. Aim follows the player every call.
static void _actor800100DecideIdleBehavior(Task* task)
{
    enum {
        ACTOR_800100_IDLE_DECISION_BASE_TICKS    = 10,
        ACTOR_800100_IDLE_DECISION_RANDOM_MASK   = 0x1F,
        ACTOR_800100_IDLE_FOLLOW_DISTANCE        = 0x600,
        ACTOR_800100_IDLE_FORCED_FOLLOW_DISTANCE = 0x400,
        ACTOR_800100_IDLE_ROLL_MASK              = 0xFF,
        ACTOR_800100_IDLE_FOLLOW_ROLL_MIN        = 0xF1,
        ACTOR_800100_IDLE_COUNT_RANDOM_MASK      = 3,
        ACTOR_800100_IDLE_COUNT_MIN              = 3,
        ACTOR_800100_IDLE_BEFORE_VARIATION       = 0,
        ACTOR_800100_IDLE_AFTER_VARIATION        = 1,
        ACTOR_800100_ANIMATION_IDLE_VARIATION    = 23,
        ACTOR_800100_IDLE_WANDER_ROLL_MIN        = 0xD0,
        ACTOR_800100_IDLE_TURN_THRESHOLD         = 0x200,
    };
    GameActor* actor;
    GameActor* decisionActor;
    GfxCoord*  rootCoord;
    GfxCoord*  playerCoord;
    s32        forceFollow;
    s32        playerDistance;
    s32        yawMagnitude;
    s16        decisionCount;

    actor         = task->work;
    rootCoord     = task->extra.tmd->coords;
    playerCoord   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    forceFollow   = (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 42, 0, 0);
    decisionActor = task->work;
    if (decisionActor->companionWork->decisionTimer <= 0) {
        companionSetDecisionDelay(task, ACTOR_800100_IDLE_DECISION_BASE_TICKS, ACTOR_800100_IDLE_DECISION_RANDOM_MASK);
        playerDistance = companionGetPlayerPlanarDistance(rootCoord);
        if ((playerDistance >= ACTOR_800100_IDLE_FOLLOW_DISTANCE && (rand() & ACTOR_800100_IDLE_ROLL_MASK) >= ACTOR_800100_IDLE_FOLLOW_ROLL_MIN) || (playerDistance >= ACTOR_800100_IDLE_FORCED_FOLLOW_DISTANCE && forceFollow != 0)) {
            _actor800100EnterFollowPlayer(task);
        } else {
            decisionCount = (u16)actor->idleTicks + 1;
            // Retained loop shape preserves the matching saved-register allocation.
            do {
                actor->idleTicks = decisionCount;
            } while (0);
            if (decisionCount >= ((rand() & ACTOR_800100_IDLE_COUNT_RANDOM_MASK) + ACTOR_800100_IDLE_COUNT_MIN)) {
                if (actor->statePhase == ACTOR_800100_IDLE_BEFORE_VARIATION) {
                    actor->statePhase = ACTOR_800100_IDLE_AFTER_VARIATION;
                    playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_IDLE_VARIATION, 0, ACTOR_800100_MOVEMENT_BLEND_FRAMES);
                } else if ((rand() & ACTOR_800100_IDLE_ROLL_MASK) >= ACTOR_800100_IDLE_WANDER_ROLL_MIN) {
                    _actor800100EnterWander(task);
                }
            } else {
                yawMagnitude = playerActorGetTurnToPoint(task, MATRIX_TRANS(&playerCoord->coord));
                if (yawMagnitude < 0) {
                    yawMagnitude = -yawMagnitude;
                }
                if (yawMagnitude >= ACTOR_800100_IDLE_TURN_THRESHOLD) {
                    actor->targetNode = NULL;
                    _actor800100EnterTurnToPlayer(task);
                }
            }
        }
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
}

/// Handlers `_actor800100TickBehavior` runs, indexed by `mode`.
static const TaskFuncTable3 D_actor_800100_80161E4C = { {
    _actor800100TickNativeMode,
    _actor800100DamageMode,
    _actor800100TickScriptedMode,
} };

/// Handlers `_actor800100TickNativeMode` runs, indexed by `state`.
static const TaskFuncTable12 D_actor_800100_80161E58 = { {
    _actor800100IdleState,
    _actor800100FollowPlayerState,
    _actor800100TurnToTargetState,
    _actor800100CombatEntryState,
    _actor800100CombatDecisionState,
    _actor800100AttackLoopState,
    _actor800100RetreatState,
    _actor800100ApproachState,
    _actor800100CombatExitState,
    _actor800100ReloadState,
    _actor800100ObstacleScanState,
    _actor800100WanderState,
} };

/// Places an effect at the water surface relative to the actor's room-space root.
///
/// Stores XYZ only, retaining the low signed halfword of the height difference.
static inline void _actor800100SetWaterSurfaceOffset(SVECTOR* surfaceOffset, const GfxCoord* rootCoord)
{
    surfaceOffset->vx = 0;
    surfaceOffset->vy = (u16)gGameSession->waterY - (u16)rootCoord->coord.t[1];
    surfaceOffset->vz = 0;
}

/// Ticks native decisions, footsteps, damage reactions, animation and motion.
///
/// Requires state 0..11, live native slots/model, companion work and the room's
/// surface-sound/effect bindings. Water footsteps seed 120 ticks of ripples,
/// emitted every tenth pre-decrement tick at the retained room water height.
/// Effect offsets are copied during spawning. Contacts are resolved only at
/// zero recoveryTicks; saved HP at or below zero enters the stopped pose.
static void _actor800100TickNativeMode(Task* task)
{
    enum {
        ACTOR_800100_WATER_FOOTSTEP_FIRST = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x89,
        ACTOR_800100_WATER_FOOTSTEP_COUNT = 4,
        ACTOR_800100_WATER_DRIP_TICKS     = 120,
        ACTOR_800100_WATER_DRIP_INTERVAL  = 10,
        // Upward spray: size 384, two ticks per cell, speed 32.
        ACTOR_800100_WATER_SPRAY_ARGUMENT    = (1 << 24) | (32 << 16) | (2 << 12) | 384,
        ACTOR_800100_WATER_RIPPLE_SIZE_MASK  = 31,
        ACTOR_800100_WATER_RIPPLE_MIN_SIZE   = 64,
        ACTOR_800100_DAMAGE_FIRST_VARIANT    = 1,
        ACTOR_800100_DAMAGE_SOUND_BANK_SHIFT = 16,
        ACTOR_800100_DAMAGE_FIRST_SOUND      = SOUND_CHARACTER(SOUND_BANK_ACTOR_800100, 10),
        ACTOR_800100_STOPPED_POSE_VARIANT    = 0,
    };
    TaskFuncTable12 states;
    SVECTOR         footstepSurfaceOffset;
    SVECTOR         dripSurfaceOffset;
    GameActor*      actor;
    CompanionWork*  companion;
    GfxCoord*       rootCoord;
    s16             dripTicks;
    s16             dripRemainder;
    s32             audioPan;

    states    = D_actor_800100_80161E58;
    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    companion = actor->companionWork;
    if (companion->decisionTimer > 0) {
        companion->decisionTimer--;
    }
    states.funcs[actor->state](task);
    // Preserve unsigned range testing of the four water-footstep sound entries.
    if ((u32)playerActorPlayFootstepCue(task) - ACTOR_800100_WATER_FOOTSTEP_FIRST < ACTOR_800100_WATER_FOOTSTEP_COUNT) {
        actor->effectTimer.waterDripTicks = ACTOR_800100_WATER_DRIP_TICKS;
        _actor800100SetWaterSurfaceOffset(&footstepSurfaceOffset, rootCoord);
        effectSpawn(gRoomEffectWaterSprayId, rootCoord, ACTOR_800100_WATER_SPRAY_ARGUMENT, &footstepSurfaceOffset);
        effectSpawn(gRoomEffectWaterRippleId, rootCoord, (rand() & ACTOR_800100_WATER_RIPPLE_SIZE_MASK) | ACTOR_800100_WATER_RIPPLE_MIN_SIZE, &footstepSurfaceOffset);
    }
    dripTicks = actor->effectTimer.waterDripTicks;
    if (dripTicks != 0) {
        actor->effectTimer.waterDripTicks--;
        dripRemainder = dripTicks % ACTOR_800100_WATER_DRIP_INTERVAL;
        if (dripRemainder == 0) {
            _actor800100SetWaterSurfaceOffset(&dripSurfaceOffset, rootCoord);
            effectSpawn(gRoomEffectWaterRippleId, rootCoord, (rand() & ACTOR_800100_WATER_RIPPLE_SIZE_MASK) | ACTOR_800100_WATER_RIPPLE_MIN_SIZE, &dripSurfaceOffset);
        }
    }
    if ((s8)actor->recoveryTicks == 0) {
        playerActorResolveBodyContacts(task, actor->collisionContacts);
        if ((u16)actor->hitRegion != 0) {
            companionEnterDamageReaction(task);
            audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant - ACTOR_800100_DAMAGE_FIRST_VARIANT) << ACTOR_800100_DAMAGE_SOUND_BANK_SHIFT) + ACTOR_800100_DAMAGE_FIRST_SOUND, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
    }
    // Advance poses before facing/movement consume the resulting animation state.
    playerActorTickAnimationState(task);
    playerActorTickChildSlots(task);
    playerActorUpdateFacing(task);
    playerActorStepMovement(task);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        playerActorEnterStoppedPose(task, ACTOR_800100_STOPPED_POSE_VARIANT);
    }
}

/// Follows the player, selecting walk/run speed and scanning when obstructed.
///
/// Requires live actor/companion work, its bound forward probe, native playback
/// and the player root in the same room frame. Arrives within 768 game units;
/// after 180 moving ticks, walking is forced until arrival. Speed can be
/// reconsidered every 60 ticks. The stage-4 area-42 override suppresses scanning.
static void _actor800100FollowPlayerState(Task* task)
{
    enum {
        ACTOR_800100_MOVEMENT_WALK               = 1,
        ACTOR_800100_ANIMATION_WALK              = 2,
        ACTOR_800100_FOLLOW_SELECT_SPEED         = 0,
        ACTOR_800100_FOLLOW_WALK                 = 1,
        ACTOR_800100_FOLLOW_RUN                  = 2,
        ACTOR_800100_FOLLOW_FORCED_WALK          = 3,
        ACTOR_800100_FOLLOW_ARRIVAL_DISTANCE     = 768,
        ACTOR_800100_FOLLOW_RUN_DISTANCE         = 0x1600,
        ACTOR_800100_FOLLOW_FORCE_WALK_TICKS     = 180,
        ACTOR_800100_FOLLOW_RESELECT_DELAY_TICKS = 60,
        ACTOR_800100_FOLLOW_SPEED_JITTER_MASK    = 0x3FF,
        ACTOR_800100_FOLLOW_RUN_KEEP_DISTANCE    = 0x1000,
        ACTOR_800100_FOLLOW_WALK_KEEP_DISTANCE   = 0x1400,
    };
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      rootCoord;
    GfxCoord*      playerCoord;
    s32            forceFollow;
    s32            distance;
    s32            speedJitter;
    s32            setIndex;
    u16            previousState;

    rootCoord   = task->extra.tmd->coords;
    playerCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor       = task->work;
    forceFollow = (GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 42, 0, 0);
    distance    = _actor800100GetContactDistance(rootCoord, actor->companionWork->probe.contacts, NULL);
    // Preserve the current behavior while the probe searches for a clear heading.
    if (distance != 0 && distance < (ACTOR_800100_FOLLOW_ARRIVAL_DISTANCE + 1) && forceFollow == 0) {
        GameActor* scanActor = task->work;

        previousState               = scanActor->state;
        scanActor->state            = ACTOR_800100_STATE_OBSTACLE_SCAN;
        scanActor->turnRateIndex    = ACTOR_800100_TURN_RATE_64;
        scanActor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        companion                   = scanActor->companionWork;
        scanActor->mode             = GAME_ACTOR_MODE_NORMAL;
        scanActor->animationState   = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
        scanActor->statePhase       = 0;
        scanActor->stateAux         = previousState;
        companion->scanClearance    = COMPANION_SCAN_UNTESTED;
        companion->scanAngle        = 0;
        playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_IDLE, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
        return;
    }
    switch (actor->statePhase) {
        case ACTOR_800100_FOLLOW_SELECT_SPEED:
            actor->stateTimer = 0;
            if (companionGetPlayerPlanarDistance(rootCoord) >= ACTOR_800100_FOLLOW_RUN_DISTANCE) {
                actor->statePhase   = ACTOR_800100_FOLLOW_RUN;
                actor->movementMode = ACTOR_800100_MOVEMENT_RUN;
                setIndex            = ACTOR_800100_ANIMATION_RUN;
            } else {
            selectWalk:
                if (actor->statePhase != ACTOR_800100_FOLLOW_FORCED_WALK) {
                    actor->statePhase = ACTOR_800100_FOLLOW_WALK;
                }
                actor->movementMode = ACTOR_800100_MOVEMENT_WALK;
                setIndex            = ACTOR_800100_ANIMATION_WALK;
            }
            playerActorPlayChildSlotsWithBlend(task, setIndex, 0, ACTOR_800100_MOVEMENT_BLEND_FRAMES);
            /* fallthrough */
        case ACTOR_800100_FOLLOW_WALK:
        case ACTOR_800100_FOLLOW_RUN:
        case ACTOR_800100_FOLLOW_FORCED_WALK:
            actor->movementSign = ACTOR_800100_MOVE_FORWARD;
            distance            = companionGetPlayerPlanarDistance(rootCoord);
            if (distance < (ACTOR_800100_FOLLOW_ARRIVAL_DISTANCE + 1)) {
                companionEnterIdle(task, 0);
            } else if (actor->statePhase != ACTOR_800100_FOLLOW_FORCED_WALK) {
                if (++actor->stateTimer == ACTOR_800100_FOLLOW_FORCE_WALK_TICKS) {
                    actor->statePhase = ACTOR_800100_FOLLOW_FORCED_WALK;
                    goto selectWalk;
                }
                if (actor->actionValue > 0) {
                    actor->actionValue = (u16)actor->actionValue - 1;
                } else {
                    speedJitter = rand() & ACTOR_800100_FOLLOW_SPEED_JITTER_MASK;
                    if (((ACTOR_800100_FOLLOW_RUN_KEEP_DISTANCE - speedJitter) >= distance && actor->statePhase == ACTOR_800100_FOLLOW_RUN) || (distance >= speedJitter + ACTOR_800100_FOLLOW_WALK_KEEP_DISTANCE && actor->statePhase == ACTOR_800100_FOLLOW_WALK)) {
                        actor->statePhase  = ACTOR_800100_FOLLOW_SELECT_SPEED;
                        actor->actionValue = ACTOR_800100_FOLLOW_RESELECT_DELAY_TICKS;
                    }
                }
            }
    }
    playerActorTurnBodyTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
}

/// Turns in place toward the selected lock target, or the player when none is set.
///
/// Requires live actor/native playback and player/model coordinates. The turn
/// completes within 128 angle units (4096 per turn); aim still follows the
/// player. A non-lockable node sets phase 2 without reading a target point.
/// Borrows 16 scratch bytes for XYZ; the fourth word is untouched.
static void _actor800100TurnToTargetState(Task* task)
{
    enum {
        ACTOR_800100_TARGET_TURN_START     = 0,
        ACTOR_800100_TARGET_TURN_ACTIVE    = 1,
        ACTOR_800100_TARGET_TURN_INVALID   = 2,
        ACTOR_800100_TARGET_TURN_TOLERANCE = 128,
    };
    VECTOR3*         targetPoint;
    GameActor*       actor;
    WorldTargetNode* node;
    TmdObject*       playerModel;
    GfxCoord*        playerCoord;
    s32              yawMagnitude;
    s32              setIndex;
    s32              turnPhase;

    actor       = task->work;
    playerModel = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd;
    targetPoint = SCRATCH_STACK_RESERVE_BYTES(sizeof(VECTOR));
    node        = actor->targetNode;
    playerCoord = playerModel->coords;
    if (node != NULL) {
        if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            worldTargetGetBodyPosition(node, targetPoint);
        } else {
            actor->statePhase = ACTOR_800100_TARGET_TURN_INVALID;
        }
    } else {
        targetPoint->vx = playerCoord->coord.t[0];
        targetPoint->vy = playerCoord->coord.t[1];
        targetPoint->vz = playerCoord->coord.t[2];
    }
    switch (actor->statePhase) {
        case ACTOR_800100_TARGET_TURN_START:
            turnPhase         = ACTOR_800100_TARGET_TURN_ACTIVE;
            actor->statePhase = turnPhase;
            if (playerActorGetTurnToPoint(task, targetPoint) < 0) {
                actor->actionValue = ACTOR_800100_TURN_NEGATIVE;
                setIndex           = ACTOR_800100_ANIMATION_TURN_LEFT;
            } else {
                actor->actionValue = ACTOR_800100_TURN_POSITIVE;
                setIndex           = ACTOR_800100_ANIMATION_TURN_RIGHT;
            }
            playerActorPlayChildSlotsWithBlend(task, setIndex, 0, ACTOR_800100_MOVEMENT_BLEND_FRAMES);
            /* fallthrough */
        case ACTOR_800100_TARGET_TURN_ACTIVE:
            actor->turnSign = (u8)actor->actionValue;
            yawMagnitude    = playerActorGetTurnToPoint(task, targetPoint);
            if (yawMagnitude < 0) {
                yawMagnitude = -yawMagnitude;
            }
            if ((yawMagnitude < (ACTOR_800100_TARGET_TURN_TOLERANCE + 1)) || (actor->statePhase == ACTOR_800100_TARGET_TURN_INVALID)) {
                companionEnterIdle(task, 0);
            }
            break;
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR));
}

/// Returns the absolute yaw turn to a lock target and writes its world XYZ point.
///
/// Requires a non-NULL initial target and live actor/model resources. A
/// non-lockable target is replaced using port-zero input. If no replacement
/// exists, the position query writes zero. Angles use 4096 units per turn;
/// `targetPoint` is borrowed writable storage for three signed 32-bit words.
static inline s32 _actor800100LockTargetTurn(Task* task, GameActor* actor, VECTOR3* targetPoint)
{
    s32 turnDelta;

    if (actor->targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
        actor->targetNode = worldTargetFindLockNodeFromPad(task);
    }
    worldTargetGetBodyPosition(actor->targetNode, targetPoint);
    turnDelta = playerActorGetTurnToPoint(task, targetPoint);
    if (turnDelta < 0) {
        turnDelta = -turnDelta;
    }
    return turnDelta;
}

/// Faces a lock target, plays combat entry, then starts combat decisions.
///
/// Requires live actor/native playback and 16 free scratch bytes. Entry waits
/// until the target is absent or within 512 yaw units (4096 per turn). Animation
/// controller 7 advances phase 2 to 3 at clip completion; both aim axes track
/// the target while waiting. The scratch point uses only the first three words.
static void _actor800100CombatEntryState(Task* task)
{
    enum {
        ACTOR_800100_ANIMATION_COMBAT_ENTRY     = 7,
        ACTOR_800100_COMBAT_ENTRY_BLEND_FRAMES  = 3,
        ACTOR_800100_COMBAT_ENTRY_FACE          = 0,
        ACTOR_800100_COMBAT_ENTRY_PLAY          = 1,
        ACTOR_800100_COMBAT_ENTRY_WAIT          = 2,
        ACTOR_800100_COMBAT_ENTRY_COMPLETE      = 3,
        ACTOR_800100_COMBAT_ENTRY_YAW_TOLERANCE = 512,
    };
    VECTOR3*   targetPoint;
    GameActor* actor;
    s32        facingPhase;
    s32        entrySet;

    actor       = task->work;
    targetPoint = SCRATCH_STACK_RESERVE_BYTES(sizeof(VECTOR));
    facingPhase = ACTOR_800100_COMBAT_ENTRY_PLAY;

    switch (actor->statePhase) {
        case ACTOR_800100_COMBAT_ENTRY_FACE:
            if (actor->targetNode == NULL || _actor800100LockTargetTurn(task, actor, targetPoint) < (ACTOR_800100_COMBAT_ENTRY_YAW_TOLERANCE + 1)) {
                actor->statePhase = facingPhase;
            } else {
                companionTrackLockTarget(task, COMPANION_LOCK_TRACK_YAW);
                break;
            }
            /* fallthrough */
        case ACTOR_800100_COMBAT_ENTRY_PLAY:
            entrySet              = ACTOR_800100_ANIMATION_COMBAT_ENTRY;
            actor->animationState = ACTOR_800100_ANIMATION_CONTROLLER_CLIP_END;
            actor->statePhase    += 1;
            playerActorPlayChildSlotsWithBlend(task, entrySet, 0, ACTOR_800100_COMBAT_ENTRY_BLEND_FRAMES);
            /* fallthrough */
        // Clip completion advances the phase before the next behavior tick.
        case ACTOR_800100_COMBAT_ENTRY_WAIT:
        case ACTOR_800100_COMBAT_ENTRY_COMPLETE:
            companionTrackLockTarget(task, COMPANION_LOCK_TRACK_YAW | COMPANION_LOCK_TRACK_PITCH);
            if (actor->statePhase == ACTOR_800100_COMBAT_ENTRY_COMPLETE) {
                GameActor* decisionActor      = task->work;
                decisionActor->mode           = GAME_ACTOR_MODE_NORMAL;
                decisionActor->state          = ACTOR_800100_STATE_COMBAT_DECISION;
                decisionActor->animationState = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
                decisionActor->statePhase     = 0;
                decisionActor->movementSign   = 0;
                decisionActor->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_COMBAT_HOLD, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR));
}

/// Resumes native combat decisions with the held-weapon pose.
///
/// Requires the live actor belonging to task and native animation resources.
/// Clears the animation controller, behavior phase and movement/turn requests;
/// blends native hold set 9 for six normal-rate frames, retaining the target.
static inline void _actor800100ResetAttackDecision(Task* task, GameActor* decisionActor)
{
    decisionActor->mode           = GAME_ACTOR_MODE_NORMAL;
    decisionActor->state          = ACTOR_800100_STATE_COMBAT_DECISION;
    decisionActor->animationState = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    decisionActor->statePhase     = 0;
    decisionActor->movementSign   = 0;
    decisionActor->turnSign       = 0;
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_COMBAT_HOLD, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
}

/// Faces the selected target and advances its cooldown-gated weapon attack loop.
///
/// Requires live native playback, companion work and a valid saved weapon variant
/// 0..4. A missing target or range at most 768 returns to combat decisions;
/// otherwise phase 0 waits for yaw within 384/4096 turns. Phase 1 ends on an
/// exhausted signed-byte repeat budget or an un-lockable target, disabling weapon
/// contacts and decaying aim. Borrows one target scratch block for this tick.
static void _actor800100AttackLoopState(Task* task)
{
    enum {
        ACTOR_800100_ATTACK_FACE          = 0,
        ACTOR_800100_ATTACK_REPEAT        = 1,
        ACTOR_800100_ATTACK_MIN_DISTANCE  = 768,
        ACTOR_800100_ATTACK_YAW_TOLERANCE = 384,
        ACTOR_800100_VARIANT_PYKE         = 4,
    };
    GameActor*                 actor;
    GameActor*                 nearTargetActor;
    GameActor*                 finishedAttackActor;
    CompanionWork*             companion;
    WorldTargetNode*           attackTarget;
    WorldTargetNode*           entryTarget;
    GfxCoord*                  rootCoord;
    _Actor800100TargetScratch* targetScratch;
    s32                        yawMagnitude;

    actor         = task->work;
    targetScratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor800100TargetScratch);
    companion     = actor->companionWork;
    companionTrackLockTarget(task, COMPANION_LOCK_TRACK_YAW | COMPANION_LOCK_TRACK_PITCH);
    switch (actor->statePhase) {
        case ACTOR_800100_ATTACK_FACE:
            entryTarget = actor->targetNode;
            if ((entryTarget == NULL) || (rootCoord = task->extra.tmd->coords, worldTargetGetBodyPosition(entryTarget, &targetScratch->targetPoint), playerActorGetPointDelta(rootCoord, &targetScratch->targetPoint, &targetScratch->targetDelta), ((playerActorPlanarLength(targetScratch->targetDelta.vx, targetScratch->targetDelta.vz) < (ACTOR_800100_ATTACK_MIN_DISTANCE + 1)) != 0))) {
                nearTargetActor = task->work;
                _actor800100ResetAttackDecision(task, nearTargetActor);
                break;
            }
            yawMagnitude = playerActorGetTurnToPoint(task, &targetScratch->targetPoint);
            if (yawMagnitude < 0) {
                yawMagnitude = -yawMagnitude;
            }
            if (yawMagnitude >= (ACTOR_800100_ATTACK_YAW_TOLERANCE + 1)) {
                break;
            }
            actor->statePhase += 1;
            /* fallthrough */
        case ACTOR_800100_ATTACK_REPEAT:
            if (((s8)companion->activity.combat.repeatsRemaining <= 0) || (attackTarget = actor->targetNode, attackTarget == NULL) || (attackTarget->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
                actor->targetNode                                     = NULL;
                actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_DECAY;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if ((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant == ACTOR_800100_VARIANT_PYKE) {
                    playerActorResetWeaponAttack(task, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
                }
                finishedAttackActor = task->work;
                _actor800100ResetAttackDecision(task, finishedAttackActor);
            } else if (actor->attackControl.cooldownTicks == 0) {
                _actor800100TickWeaponAttack(task);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor800100TargetScratch);
}

/// Turns away from the lock target, then runs forward for 20..51 behavior ticks.
///
/// Requires a live selected target, model, bound forward probe and native
/// playback. The turn stops at a random 1024..1535-unit yaw separation (4096
/// per turn). A probe contact within 640 game units starts obstacle scanning;
/// expiry returns to combat decisions. Borrows 16 scratch bytes for XYZ.
/// Probe distances retain their signed-halfword narrowing before comparison.
static void _actor800100RetreatState(Task* task)
{
    enum {
        ACTOR_800100_RETREAT_START             = 0,
        ACTOR_800100_RETREAT_TURN              = 1,
        ACTOR_800100_RETREAT_RUN               = 2,
        ACTOR_800100_RETREAT_YAW_RANDOM_MASK   = 0x1FF,
        ACTOR_800100_RETREAT_YAW_MIN           = 1024,
        ACTOR_800100_RETREAT_TICKS_RANDOM_MASK = 0x1F,
        ACTOR_800100_RETREAT_TICKS_MIN         = 20,
        ACTOR_800100_RETREAT_OBSTACLE_DISTANCE = 640,
    };
    VECTOR3*       targetPoint;
    GameActor*     actor;
    GfxCoord*      rootCoord;
    CompanionWork* companion;
    u16            previousState;
    s16            probeDistance;
    s32            forwardSign;
    s32            setIndex;
    s32            yawMagnitude;
    s32            ticksLeft;
    s32            retreatYaw;

    targetPoint = SCRATCH_STACK_RESERVE_BYTES(sizeof(VECTOR));
    actor       = task->work;
    rootCoord   = task->extra.tmd->coords;
    forwardSign = ACTOR_800100_MOVE_FORWARD;
    switch (actor->statePhase) {
        case ACTOR_800100_RETREAT_START:
            actor->statePhase    = ACTOR_800100_RETREAT_TURN;
            actor->turnRateIndex = ACTOR_800100_TURN_RATE_64;
            worldTargetGetBodyPosition(actor->targetNode, targetPoint);
            if (playerActorGetTurnToPoint(task, targetPoint) < 0) {
                actor->actionValue = ACTOR_800100_TURN_POSITIVE;
                setIndex           = ACTOR_800100_ANIMATION_TURN_RIGHT;
            } else {
                actor->actionValue = ACTOR_800100_TURN_NEGATIVE;
                setIndex           = ACTOR_800100_ANIMATION_TURN_LEFT;
            }
            actor->stateTimer = (rand() & ACTOR_800100_RETREAT_YAW_RANDOM_MASK) + ACTOR_800100_RETREAT_YAW_MIN;
            playerActorPlayChildSlotsWithBlend(task, setIndex, 0, ACTOR_800100_MOVEMENT_BLEND_FRAMES);
            /* fallthrough */
        // Turn in the direction opposite the bearing to the target.
        case ACTOR_800100_RETREAT_TURN:
            actor->turnSign = (u8)actor->actionValue;
            worldTargetGetBodyPosition(actor->targetNode, targetPoint);
            yawMagnitude = playerActorGetTurnToPoint(task, targetPoint);
            retreatYaw   = actor->stateTimer;
            if (yawMagnitude < 0) {
                yawMagnitude = -yawMagnitude;
            }
            if (yawMagnitude >= retreatYaw) {
                actor->movementMode = ACTOR_800100_MOVEMENT_RUN;
                actor->statePhase  += 1;
                actor->stateTimer   = (rand() & ACTOR_800100_RETREAT_TICKS_RANDOM_MASK) + ACTOR_800100_RETREAT_TICKS_MIN;
                playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_RUN, 0, ACTOR_800100_MOVEMENT_BLEND_FRAMES);
            }
            break;
        case ACTOR_800100_RETREAT_RUN:
            actor->movementSign = forwardSign;
            ticksLeft           = actor->stateTimer - 1;
            actor->stateTimer   = ticksLeft;
            if (ticksLeft <= 0) {
                GameActor* decisionActor      = task->work;
                decisionActor->mode           = GAME_ACTOR_MODE_NORMAL;
                decisionActor->state          = ACTOR_800100_STATE_COMBAT_DECISION;
                decisionActor->animationState = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
                decisionActor->statePhase     = 0;
                decisionActor->movementSign   = 0;
                decisionActor->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_COMBAT_HOLD, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
            } else {
                probeDistance = _actor800100GetContactDistance(rootCoord, actor->companionWork->probe.contacts, NULL);
                if (probeDistance != 0) {
                    if (probeDistance < (ACTOR_800100_RETREAT_OBSTACLE_DISTANCE + 1)) {
                        s16        idleSet          = ACTOR_800100_ANIMATION_IDLE;
                        GameActor* scanActor        = task->work;
                        previousState               = scanActor->state;
                        scanActor->state            = ACTOR_800100_STATE_OBSTACLE_SCAN;
                        scanActor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
                        companion                   = scanActor->companionWork;
                        scanActor->mode             = GAME_ACTOR_MODE_NORMAL;
                        scanActor->turnRateIndex    = ACTOR_800100_TURN_RATE_64;
                        scanActor->animationState   = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
                        scanActor->statePhase       = 0;
                        scanActor->stateAux         = previousState;
                        companion->scanClearance    = COMPANION_SCAN_UNTESTED;
                        companion->scanAngle        = 0;
                        playerActorPlayChildSlotsWithBlend(task, idleSet, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
                    }
                } else {
                    actor->turnSign = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR));
}

/// Starts obstacle scanning from the target-approach behavior.
///
/// Requires live companion work, a bound probe and native playback. Saves the
/// prior state in stateAux, decays aim, selects 64/4096-turn body steps and
/// resets the relative probe yaw and clearance. Blends idle for six frames;
/// the next native behavior tick begins the full yaw sweep in state 10.
static inline void _actor800100ApproachStartScan(Task* task)
{
    GameActor*     scanActor;
    CompanionWork* companion;
    u16            previousState;
    scanActor                   = task->work;
    previousState               = scanActor->state;
    scanActor->state            = ACTOR_800100_STATE_OBSTACLE_SCAN;
    scanActor->turnRateIndex    = ACTOR_800100_TURN_RATE_64;
    scanActor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    companion                   = scanActor->companionWork;
    scanActor->mode             = GAME_ACTOR_MODE_NORMAL;
    scanActor->animationState   = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    scanActor->statePhase       = 0;
    scanActor->stateAux         = previousState;
    companion->scanClearance    = COMPANION_SCAN_UNTESTED;
    companion->scanAngle        = 0;
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_IDLE, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
}

/// Runs toward the selected target or player, then resumes combat decisions.
///
/// Requires live actor/companion work and model roots in their common room frame.
/// Stops within 2816 game units of the player, or a newly rolled 2816..3839 range
/// of a target. A forward contact within 768 accumulates obstruction ticks and
/// can start a full probe scan. Borrows one target scratch block; body and aim
/// turning still run on the tick that returns to decisions.
static void _actor800100ApproachState(Task* task)
{
    enum {
        ACTOR_800100_APPROACH_START             = 0,
        ACTOR_800100_APPROACH_MOVE              = 1,
        ACTOR_800100_APPROACH_TARGET_LOST       = 2,
        ACTOR_800100_APPROACH_OBSTACLE_DISTANCE = 768,
        ACTOR_800100_APPROACH_SCAN_TICKS_MASK   = 0x3F,
        ACTOR_800100_APPROACH_SCAN_MIN_TICKS    = 40,
        ACTOR_800100_APPROACH_DISTANCE_MASK     = 0x3FF,
        ACTOR_800100_APPROACH_MIN_DISTANCE      = 2816,
        ACTOR_800100_ANIMATION_APPROACH         = 12,
    };
    GameActor*                 actor;
    GameActor*                 decisionActor;
    GfxCoord*                  rootCoord;
    GfxCoord*                  playerCoord;
    _Actor800100TargetScratch* targetScratch;
    WorldTargetNode*           targetNode;
    u16                        obstructionTicks;
    u32                        randomState;
    s32                        planarDistance;
    s32                        approachDistance;

    rootCoord      = task->extra.tmd->coords;
    playerCoord    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor          = task->work;
    planarDistance = _actor800100GetContactDistance(rootCoord, actor->companionWork->probe.contacts, NULL);
    if (planarDistance != 0 && planarDistance < (ACTOR_800100_APPROACH_OBSTACLE_DISTANCE + 1)) {
        obstructionTicks   = actor->actionValue + 1;
        actor->actionValue = obstructionTicks;
        randomState        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = randomState;
        if ((s16)obstructionTicks >= (s32)(((randomState >> 16) & ACTOR_800100_APPROACH_SCAN_TICKS_MASK) + ACTOR_800100_APPROACH_SCAN_MIN_TICKS)) {
            _actor800100ApproachStartScan(task);
            return;
        }
    }
    targetScratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor800100TargetScratch);
    targetNode    = actor->targetNode;
    if (targetNode != NULL) {
        if ((targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) == 0) {
            worldTargetGetBodyPosition(targetNode, &targetScratch->targetPoint);
        } else {
            // Retained behavior: this tick still turns using the uninitialized scratch point.
            actor->statePhase = ACTOR_800100_APPROACH_TARGET_LOST;
        }
    } else {
        targetScratch->targetPoint.vx = playerCoord->coord.t[0];
        targetScratch->targetPoint.vy = playerCoord->coord.t[1];
        targetScratch->targetPoint.vz = playerCoord->coord.t[2];
    }
    switch (actor->statePhase) {
        case ACTOR_800100_APPROACH_START:
            actor->statePhase   = ACTOR_800100_APPROACH_MOVE;
            actor->stateTimer   = 0;
            actor->movementMode = ACTOR_800100_MOVEMENT_RUN;
            playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_APPROACH, 0, ACTOR_800100_MOVEMENT_BLEND_FRAMES);
            /* fallthrough */
        case ACTOR_800100_APPROACH_MOVE:
        case ACTOR_800100_APPROACH_TARGET_LOST:
            actor->movementSign = ACTOR_800100_MOVE_FORWARD;
            playerActorGetPointDelta(rootCoord, &targetScratch->targetPoint, &targetScratch->targetDelta);
            planarDistance = playerActorPlanarLength(targetScratch->targetDelta.vx, targetScratch->targetDelta.vz);
            if (actor->targetNode != NULL) {
                approachDistance = (rand() & ACTOR_800100_APPROACH_DISTANCE_MASK) + ACTOR_800100_APPROACH_MIN_DISTANCE;
            } else {
                approachDistance = ACTOR_800100_APPROACH_MIN_DISTANCE;
            }
            if (approachDistance >= planarDistance || actor->statePhase == ACTOR_800100_APPROACH_TARGET_LOST) {
                decisionActor                 = task->work;
                decisionActor->mode           = GAME_ACTOR_MODE_NORMAL;
                decisionActor->state          = ACTOR_800100_STATE_COMBAT_DECISION;
                decisionActor->animationState = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
                decisionActor->statePhase     = 0;
                decisionActor->movementSign   = 0;
                decisionActor->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_COMBAT_HOLD, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
            }
            break;
    }
    playerActorTurnBodyTowardPoint(task, &targetScratch->targetPoint);
    playerActorTurnAimTowardPoint(task, &targetScratch->targetPoint);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor800100TargetScratch);
}

/// Plays native reload cues and restores the companion's weapon attack allowance.
///
/// Requires native state 9, slot 1 playback, a live equipped weapon and saved
/// variant 0..4. M950 marks its phase on both cue bits; MM1 emits the effect once
/// on entry. Refreshes the attack allowance every tick and resumes combat
/// decisions once a non-NULL record accompanies a settled slot or control jump.
static void _actor800100ReloadState(Task* task)
{
    enum {
        ACTOR_800100_RELOAD_SLOT        = 1,
        ACTOR_800100_RELOAD_WEAPON_M950 = 3,
        ACTOR_800100_RELOAD_WEAPON_MM1  = 12,
        ACTOR_800100_RELOAD_WAIT_CUE    = 0,
        ACTOR_800100_RELOAD_CUE_STARTED = 1,
    };
    GameActor*             actor;
    GameActor*             decisionActor;
    CompanionWork*         companion;
    const AnimationRecord* reloadRecord;
    GfxCoord*              weaponCoord;
    s16                    weaponId;

    actor        = task->work;
    companion    = actor->companionWork;
    reloadRecord = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + ACTOR_800100_RELOAD_SLOT);
    weaponCoord  = actor->equipmentTasks[1]->extra.tmd->coords;
    weaponId     = D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant];

    switch (weaponId) {
        case ACTOR_800100_RELOAD_WEAPON_M950:
            if (reloadRecord != NULL) {
                if (reloadRecord != actor->lastCueRecord) {
                    actor->lastCueRecord = reloadRecord;
                    if ((reloadRecord->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                        if (actor->statePhase == ACTOR_800100_RELOAD_WAIT_CUE) {
                            actor->statePhase = ACTOR_800100_RELOAD_CUE_STARTED;
                        }
                    }
                }
            }
            break;
        case ACTOR_800100_RELOAD_WEAPON_MM1:
            if (actor->statePhase == ACTOR_800100_RELOAD_WAIT_CUE) {
                actor->statePhase = ACTOR_800100_RELOAD_CUE_STARTED;
                effectSpawn(EFFECT_RELOAD_EMITTER, weaponCoord, ACTOR_800100_RELOAD_WEAPON_MM1, NULL);
            }
            if (reloadRecord != NULL) {
                if (reloadRecord != actor->lastCueRecord) {
                    actor->lastCueRecord = reloadRecord;
                }
            }
            break;
        default:
            if (actor->statePhase == ACTOR_800100_RELOAD_WAIT_CUE) {
                actor->statePhase = ACTOR_800100_RELOAD_CUE_STARTED;
            } else {
                if (reloadRecord != NULL) {
                    if (reloadRecord != actor->lastCueRecord) {
                        actor->lastCueRecord = reloadRecord;
                    }
                }
            }
            break;
    }

    companion->activity.combat.attacksRemaining = D_actor_800100_80167230[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant];
    if (reloadRecord != NULL && playerActorIsSlotAdvancingLinearly(task, ACTOR_800100_RELOAD_SLOT, 0, 0) == 0) {
        decisionActor = task->work;
        _actor800100ResetAttackDecision(task, decisionActor);
    }
}

/// Sweeps the forward probe, turns to the clearest heading, then moves briefly.
///
/// Requires a bound probe and scan initialization with clearance -1 and yaw zero.
/// Samples relative yaw in 128-unit steps (4096 per turn), preferring clear zero
/// distance over the farthest contact. Once within 64 yaw units, snaps the heading
/// and runs for 20..83 ticks in combat or walks for 40..167 otherwise. A contact
/// within 768 game units or timer expiry returns to idle; aim follows the player.
static void _actor800100ObstacleScanState(Task* task)
{
    enum {
        ACTOR_800100_SCAN_SWEEP             = 0,
        ACTOR_800100_SCAN_TURN              = 1,
        ACTOR_800100_SCAN_MOVE              = 2,
        ACTOR_800100_SCAN_YAW_TOLERANCE     = 64,
        ACTOR_800100_SCAN_RUN_TICKS_MASK    = 0x3F,
        ACTOR_800100_SCAN_RUN_MIN_TICKS     = 20,
        ACTOR_800100_SCAN_WALK_TICKS_MASK   = 0x7F,
        ACTOR_800100_SCAN_WALK_MIN_TICKS    = 40,
        ACTOR_800100_SCAN_OBSTACLE_DISTANCE = 768,
        ACTOR_800100_SCAN_ANIMATION_WALK    = 2,
        ACTOR_800100_SCAN_MOVEMENT_WALK     = 1,
        ACTOR_800100_SCAN_BLEND_FRAMES      = 3,
    };
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      rootCoord;
    GfxCoord*      playerCoord;
    s32            probeDistance;
    u32            randomState;
    s32            turnSet;
    s32            moveSet;
    s32            yawMagnitude;
    s32            turnPhaseAndForward;
    s32            turnDelta;
    s32            phaseOrTargetHeading;
    s32            currentHeading;
    u16            targetHeading;

    rootCoord            = task->extra.tmd->coords;
    playerCoord          = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor                = task->work;
    companion            = actor->companionWork;
    probeDistance        = _actor800100GetContactDistance(rootCoord, companion->probe.contacts, NULL);
    phaseOrTargetHeading = actor->statePhase;
    turnPhaseAndForward  = ACTOR_800100_SCAN_TURN;

    switch (phaseOrTargetHeading) {
        case ACTOR_800100_SCAN_SWEEP:
            if (companion->scanAngle < ACTOR_TRANSFORM_ANGLE_TURN) {
                // Prefer the farthest contact, with a clear direction ending the search.
                if (companion->scanClearance != COMPANION_SCAN_CLEAR && (companion->scanClearance < probeDistance || probeDistance == COMPANION_SCAN_CLEAR)) {
                    companion->scanClearance = probeDistance;
                    companion->targetHeading = companion->scanAngle;
                }
                companion->scanAngle += COMPANION_SCAN_ANGLE_STEP;
            } else {
                actor->statePhase        = turnPhaseAndForward;
                targetHeading            = (companion->targetHeading + actor->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
                companion->targetHeading = targetHeading;
                turnDelta                = playerActorShortestTurn(actor->rotation.vy, targetHeading);
                turnSet                  = ACTOR_800100_ANIMATION_TURN_LEFT;
                if (turnDelta > 0) {
                    turnSet            = ACTOR_800100_ANIMATION_TURN_RIGHT;
                    companion->turnDir = turnPhaseAndForward;
                } else {
                    companion->turnDir = ACTOR_800100_TURN_NEGATIVE;
                }
                playerActorPlayChildSlotsWithBlend(task, turnSet, 0, ACTOR_800100_SCAN_BLEND_FRAMES);
            }
            break;
        case ACTOR_800100_SCAN_TURN:
            actor->turnSign = (u8)companion->turnDir;
            // The retained loop and temporary reuse preserve heading-load ordering.
            do {
                currentHeading       = actor->rotation.vy;
                phaseOrTargetHeading = companion->targetHeading;
                yawMagnitude         = currentHeading - phaseOrTargetHeading;
                if (yawMagnitude < 0) {
                    yawMagnitude = -yawMagnitude;
                }
            } while (0);
            if (yawMagnitude < ACTOR_800100_SCAN_YAW_TOLERANCE) {
                actor->statePhase++;
                actor->rotation.vy = companion->targetHeading;
                actor->turnSign    = 0;
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    moveSet             = ACTOR_800100_ANIMATION_RUN;
                    actor->movementMode = ACTOR_800100_MOVEMENT_RUN;
                    randomState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = randomState;
                    actor->stateTimer   = ((randomState >> 16) & ACTOR_800100_SCAN_RUN_TICKS_MASK) + ACTOR_800100_SCAN_RUN_MIN_TICKS;
                    playerActorPlayChildSlotsWithBlend(task, moveSet, 0, ACTOR_800100_SCAN_BLEND_FRAMES);
                } else {
                    moveSet             = ACTOR_800100_SCAN_ANIMATION_WALK;
                    actor->movementMode = ACTOR_800100_SCAN_MOVEMENT_WALK;
                    randomState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = randomState;
                    actor->stateTimer   = ((randomState >> 16) & ACTOR_800100_SCAN_WALK_TICKS_MASK) + ACTOR_800100_SCAN_WALK_MIN_TICKS;
                    playerActorPlayChildSlotsWithBlend(task, moveSet, 0, ACTOR_800100_SCAN_BLEND_FRAMES);
                }
            }
            break;
        case ACTOR_800100_SCAN_MOVE:
            if ((probeDistance < (ACTOR_800100_SCAN_OBSTACLE_DISTANCE + 1) && probeDistance != 0) || --actor->stateTimer <= 0) {
                companionEnterIdle(task, 0);
            } else {
                actor->movementSign = ACTOR_800100_MOVE_FORWARD;
            }
            break;
    }
    playerActorTurnAimTowardPoint(task, MATRIX_TRANS(&playerCoord->coord));
}

/// Starts obstacle scanning from the random-wander behavior.
///
/// Requires live companion work, a bound probe and native playback. Saves the
/// prior state in stateAux, decays aim, selects 64/4096-turn body steps and
/// resets relative probe yaw and clearance before the next state-10 tick.
/// Idle playback blends for six normal-rate frames.
static inline void _actor800100WanderStartScan(Task* task)
{
    GameActor*     scanActor;
    CompanionWork* scanCompanion;
    u16            previousState;
    s16            scanIdleSet;
    scanIdleSet                  = ACTOR_800100_ANIMATION_IDLE;
    scanActor                    = task->work;
    previousState                = scanActor->state;
    scanActor->state             = ACTOR_800100_STATE_OBSTACLE_SCAN;
    scanActor->aimTrackingState  = GAME_ACTOR_AIM_TRACKING_DECAY;
    scanCompanion                = scanActor->companionWork;
    scanActor->mode              = GAME_ACTOR_MODE_NORMAL;
    scanActor->turnRateIndex     = ACTOR_800100_TURN_RATE_64;
    scanActor->animationState    = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    scanActor->statePhase        = 0;
    scanActor->stateAux          = previousState;
    scanCompanion->scanClearance = COMPANION_SCAN_UNTESTED;
    scanCompanion->scanAngle     = 0;
    playerActorPlayChildSlotsWithBlend(task, scanIdleSet, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
}

/// Turns to a random heading, pauses, then walks until blocked or its timer ends.
///
/// Requires live native playback and a bound companion probe. Angles use 4096
/// units per turn; turning snaps within 64 units. Pauses for 30..157 behavior
/// ticks, then scans if a contact lies within 1024 game units or walks for
/// 60..123 ticks. A contact within 768 or timer expiry returns to idle.
static void _actor800100WanderState(Task* task)
{
    enum {
        ACTOR_800100_WANDER_START            = 0,
        ACTOR_800100_WANDER_TURN             = 1,
        ACTOR_800100_WANDER_PAUSE            = 2,
        ACTOR_800100_WANDER_WALK             = 3,
        ACTOR_800100_WANDER_YAW_TOLERANCE    = 64,
        ACTOR_800100_WANDER_PAUSE_TICKS_MASK = 0x7F,
        ACTOR_800100_WANDER_PAUSE_MIN_TICKS  = 30,
        ACTOR_800100_WANDER_SCAN_DISTANCE    = 1024,
        ACTOR_800100_WANDER_STOP_DISTANCE    = 768,
        ACTOR_800100_WANDER_WALK_TICKS_MASK  = 0x3F,
        ACTOR_800100_WANDER_WALK_MIN_TICKS   = 60,
        ACTOR_800100_WANDER_ANIMATION_WALK   = 2,
        ACTOR_800100_WANDER_MOVEMENT_WALK    = 1,
        ACTOR_800100_WANDER_BLEND_FRAMES     = 3,
    };
    GameActor*     actor;
    CompanionWork* companion;
    s32            turnPhaseAndForward;
    s32            turnSet;
    s32            turnAndTimingValue;
    s32            yawMagnitude;
    s32            probeDistance;
    s32            turnDelta;
    s32            phaseOrTargetHeading;
    s16            targetHeading;

    actor                = task->work;
    companion            = actor->companionWork;
    probeDistance        = _actor800100GetContactDistance(task->extra.tmd->coords, companion->probe.contacts, NULL);
    phaseOrTargetHeading = actor->statePhase;
    turnPhaseAndForward  = ACTOR_800100_WANDER_TURN;
    switch (phaseOrTargetHeading) {
        case ACTOR_800100_WANDER_START:
            actor->statePhase        = turnPhaseAndForward;
            targetHeading            = (actor->rotation.vy + (rand() & ACTOR_TRANSFORM_ANGLE_MASK)) & ACTOR_TRANSFORM_ANGLE_MASK;
            companion->targetHeading = targetHeading;
            turnDelta                = playerActorShortestTurn(actor->rotation.vy, targetHeading);
            turnSet                  = ACTOR_800100_ANIMATION_TURN_LEFT;
            if (turnDelta << 16 > 0) {
                turnSet            = ACTOR_800100_ANIMATION_TURN_RIGHT;
                companion->turnDir = turnPhaseAndForward;
            } else {
                companion->turnDir = ACTOR_800100_TURN_NEGATIVE;
            }
            playerActorPlayChildSlotsWithBlend(task, turnSet, 0, ACTOR_800100_WANDER_BLEND_FRAMES);
            /* fallthrough */
        case ACTOR_800100_WANDER_TURN:
            // Byte, heading and timer values share a temporary to preserve load ordering.
            turnAndTimingValue   = (u8)companion->turnDir;
            actor->turnSign      = turnAndTimingValue;
            turnAndTimingValue   = actor->rotation.vy;
            phaseOrTargetHeading = companion->targetHeading;
            yawMagnitude         = turnAndTimingValue - phaseOrTargetHeading;
            if (yawMagnitude < 0) {
                yawMagnitude = -yawMagnitude;
            }
            if (yawMagnitude < ACTOR_800100_WANDER_YAW_TOLERANCE) {
                actor->statePhase  += 1;
                actor->rotation.vy  = (u16)companion->targetHeading;
                actor->turnSign     = 0;
                actor->movementMode = ACTOR_800100_WANDER_MOVEMENT_WALK;
                turnAndTimingValue  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = turnAndTimingValue;
                turnAndTimingValue  = (((u32)turnAndTimingValue >> 16) & ACTOR_800100_WANDER_PAUSE_TICKS_MASK) + ACTOR_800100_WANDER_PAUSE_MIN_TICKS;
                actor->stateTimer   = turnAndTimingValue;
                playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_IDLE, 0, ACTOR_800100_WANDER_BLEND_FRAMES);
                break;
            }
            break;
        case ACTOR_800100_WANDER_PAUSE:
            turnAndTimingValue = actor->stateTimer - 1;
            actor->stateTimer  = turnAndTimingValue;
            if (turnAndTimingValue != 0) {
                break;
            }
            if (probeDistance < (ACTOR_800100_WANDER_SCAN_DISTANCE + 1) && probeDistance != 0) {
                _actor800100WanderStartScan(task);
                break;
            }
            actor->statePhase += 1;
            actor->stateTimer  = (rand() & ACTOR_800100_WANDER_WALK_TICKS_MASK) + ACTOR_800100_WANDER_WALK_MIN_TICKS;
            playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_WANDER_ANIMATION_WALK, 0, ACTOR_800100_WANDER_BLEND_FRAMES);
            break;
        case ACTOR_800100_WANDER_WALK:
            if (probeDistance < (ACTOR_800100_WANDER_STOP_DISTANCE + 1) && probeDistance != 0) {
                companionEnterIdle(task, 0);
                break;
            }
            turnAndTimingValue = actor->stateTimer - 1;
            actor->stateTimer  = turnAndTimingValue;
            if (turnAndTimingValue <= 0) {
                companionEnterIdle(task, 0);
                break;
            }
            actor->movementSign = turnPhaseAndForward;
            break;
    }
}

/// Ticks attack/recovery timers and dispatches the companion's current mode.
///
/// Requires live actor work with mode 0 native, 1 damage or 2 scripted. Clears
/// this tick's movement and turn requests before dispatch and the pushback-
/// direction override afterward. Timers decrease only while positive; recovery
/// uses a signed-byte test. The caller gates this whole update while frozen.
static void _actor800100TickBehavior(Task* task)
{
    GameActor*     actor;
    TaskFuncTable3 handlers;

    handlers = D_actor_800100_80161E4C;
    actor    = task->work;
    if (actor->attackControl.cooldownTicks > 0) {
        actor->attackControl.cooldownTicks--;
    }
    if ((s8)actor->recoveryTicks > 0) {
        actor->recoveryTicks--;
    }
    actor->movementSign = 0;
    actor->turnSign     = 0;
    handlers.funcs[actor->mode](task);
    actor->usesPushbackDirection = 0;
}

/// Selects a lock target and starts the companion's combat-entry behavior.
///
/// Requires live actor/companion work and native animation resources. Clears the
/// repeat budget, starts target aim tracking and blends idle for six frames.
/// Retained standalone entry with no callers; the idle state carries the same
/// transition when combat begins.
static void _actor800100EnterCombatEntry(Task* task)
{
    GameActor* actor;

    actor                                                  = task->work;
    actor->state                                           = ACTOR_800100_STATE_COMBAT_ENTRY;
    actor->mode                                            = GAME_ACTOR_MODE_NORMAL;
    actor->animationState                                  = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    actor->statePhase                                      = 0;
    actor->companionWork->activity.combat.repeatsRemaining = 0;
    actor->aimTrackingState                                = GAME_ACTOR_AIM_TRACKING_TARGET;
    actor->targetNode                                      = worldTargetFindLockNode(task);
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_IDLE, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
}

/// Starts the combat approach behavior with a fresh obstruction counter.
///
/// Requires live companion work and model/native playback for the next tick.
/// Preserves the selected target, stops this tick's motion and decays tracked
/// aim. The behavior chooses its approach animation on its next update.
static void _actor800100EnterApproach(Task* task)
{
    enum {
        ACTOR_800100_STATE_APPROACH = 7,
    };
    GameActor* actor;

    actor                   = task->work;
    actor->state            = ACTOR_800100_STATE_APPROACH;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->turnRateIndex    = ACTOR_800100_TURN_STOPPED;
    actor->animationState   = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    actor->statePhase       = 0;
    actor->actionValue      = 0;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    actor->movementSign     = 0;
    actor->turnSign         = 0;
}

/// Starts a full probe sweep to choose an unobstructed movement heading.
///
/// Requires live companion work, a bound forward probe and native playback.
/// Saves the prior behavior in `stateAux`, resets clearance to untested and
/// starts the relative yaw sweep at zero. Heading turns by 64/4096 per tick.
static void _actor800100EnterObstacleScan(Task* task)
{
    GameActor*     actor;
    CompanionWork* companion;
    u16            previousState;

    actor                    = task->work;
    previousState            = actor->state;
    actor->state             = ACTOR_800100_STATE_OBSTACLE_SCAN;
    actor->turnRateIndex     = ACTOR_800100_TURN_RATE_64;
    actor->aimTrackingState  = GAME_ACTOR_AIM_TRACKING_DECAY;
    companion                = actor->companionWork;
    actor->mode              = GAME_ACTOR_MODE_NORMAL;
    actor->animationState    = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    actor->statePhase        = 0;
    actor->stateAux          = previousState;
    companion->scanClearance = COMPANION_SCAN_UNTESTED;
    companion->scanAngle     = 0;
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_IDLE, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
}

/// Clears animation phase and idle-decision progress when a behavior starts.
///
/// Requires live actor work. Follow, turn-to-player and wander entries retain
/// their own movement and turn choices while restarting this common progress.
static inline void _actor800100ResetBehaviorProgress(GameActor* actor)
{
    actor->animationState = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
}

/// Starts following the player and seeds a 60-tick movement-speed reconsideration delay.
///
/// Requires live `GameActor` work. The behavior chooses its movement animation
/// on its next tick and selects a 64-angle-unit turn step (4096 per turn).
static void _actor800100EnterFollowPlayer(Task* task)
{
    enum {
        ACTOR_800100_STATE_FOLLOW_PLAYER         = 1,
        ACTOR_800100_FOLLOW_RESELECT_DELAY_TICKS = 60
    };
    GameActor* actor;

    actor                = task->work;
    actor->state         = ACTOR_800100_STATE_FOLLOW_PLAYER;
    actor->turnRateIndex = ACTOR_800100_TURN_RATE_64;
    actor->mode          = GAME_ACTOR_MODE_NORMAL;
    _actor800100ResetBehaviorProgress(actor);
    actor->actionValue = ACTOR_800100_FOLLOW_RESELECT_DELAY_TICKS;
}

/// Starts an in-place turn toward the player after the idle decision clears the target node.
///
/// Requires live `GameActor` work with `targetNode == NULL`. The next behavior
/// tick selects the left/right turn animation; yaw steps by 64/4096 turns.
static void _actor800100EnterTurnToPlayer(Task* task)
{
    enum {
        ACTOR_800100_STATE_TURN_TO_PLAYER = 2,
        ACTOR_800100_MOVEMENT_STOPPED     = 0
    };
    GameActor* actor;

    actor                = task->work;
    actor->state         = ACTOR_800100_STATE_TURN_TO_PLAYER;
    actor->mode          = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode  = ACTOR_800100_MOVEMENT_STOPPED;
    actor->turnRateIndex = ACTOR_800100_TURN_RATE_64;
    _actor800100ResetBehaviorProgress(actor);
}

/// Starts the idle wander behavior: choose a heading, pause, then walk briefly.
///
/// Requires live `GameActor` work with companion probe storage. The next tick
/// chooses the heading and turn animation; yaw steps by 64/4096 turns.
static void _actor800100EnterWander(Task* task)
{
    enum { ACTOR_800100_STATE_WANDER = 11 };
    GameActor* actor;

    actor                = task->work;
    actor->state         = ACTOR_800100_STATE_WANDER;
    actor->mode          = GAME_ACTOR_MODE_NORMAL;
    actor->turnRateIndex = ACTOR_800100_TURN_RATE_64;
    _actor800100ResetBehaviorProgress(actor);
}

/// Starts combat entry when a battle engages, otherwise chooses idle behavior.
///
/// Requires live companion work, native animation resources and a live player.
/// Combat entry selects a target and resets the attack-repeat budget; the idle
/// decision timer controls following, turning, wandering and idle variations.
static void _actor800100IdleState(Task* task)
{
    GameActor* actor;

    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        actor                                                  = task->work;
        actor->state                                           = ACTOR_800100_STATE_COMBAT_ENTRY;
        actor->mode                                            = GAME_ACTOR_MODE_NORMAL;
        actor->animationState                                  = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
        actor->statePhase                                      = 0;
        actor->companionWork->activity.combat.repeatsRemaining = 0;
        actor->aimTrackingState                                = GAME_ACTOR_AIM_TRACKING_TARGET;
        actor->targetNode                                      = worldTargetFindLockNode(task);
        playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_IDLE, 0, ACTOR_800100_COMBAT_BLEND_FRAMES);
        return;
    }
    _actor800100DecideIdleBehavior(task);
}

/// Chooses the next combat behavior or starts combat exit when battle ends.
///
/// Requires live native actor/companion resources. The battle gate is checked
/// before the target-range and HP decisions, so combat exit wins that tick.
static void _actor800100CombatDecisionState(Task* task)
{
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
        _actor800100EnterCombatExit(task);
        return;
    }
    _actor800100DecideCombatBehavior(task);
}

/// Returns to idle once the combat-exit animation has advanced the behavior phase.
///
/// Requires the live actor and animation resources used by `companionEnterIdle`.
static void _actor800100CombatExitState(Task* task)
{
    enum { ACTOR_800100_COMBAT_EXIT_WAITING = 0 };
    GameActor* actor;

    actor = task->work;
    if (actor->statePhase != ACTOR_800100_COMBAT_EXIT_WAITING) {
        companionEnterIdle(task, false);
    }
}

/// Handlers `_actor800100DamageMode` runs, indexed by `hitRegion`.
static const TaskFuncTable4 D_actor_800100_80161E88 = { {
    _actor800100DamageRecoveryState,
    _actor800100DamageRecoveryState,
    _actor800100DamageRecoveryState,
    _actor800100StoppedDamageState,
} };

/// Advances damage playback, dispatches the hit reaction and applies motion.
///
/// Requires live native playback and a hit selector in 0..3: 0..2 recover at
/// phase 1, while 3 keeps the stopped pose. Animation control runs before slot
/// playback and recovery dispatch. Turning and movement still run afterward.
static void _actor800100DamageMode(Task* task)
{
    GameActor*     actor;
    TaskFuncTable4 handlers;

    handlers = D_actor_800100_80161E88;
    actor    = task->work;
    playerActorTickAnimationState(task);
    playerActorTickChildSlots(task);
    handlers.funcs[(u16)actor->hitRegion](task);
    playerActorUpdateFacing(task);
    playerActorStepMovement(task);
}

/// Returns damage selectors 0..2 to idle when their clip reaches phase 1.
///
/// Requires live actor/native playback. Phase 0 keeps waiting; every phase
/// other than 1 is unchanged. Recovery clears the pending hit and arms its
/// immunity timer through `companionRecoverToIdle`.
static void _actor800100DamageRecoveryState(Task* task)
{
    enum {
        ACTOR_800100_DAMAGE_WAITING  = 0,
        ACTOR_800100_DAMAGE_COMPLETE = 1,
    };
    GameActor* actor;
    u16        phase;

    actor = task->work;
    phase = actor->statePhase;
    if (phase == ACTOR_800100_DAMAGE_WAITING) {
        return;
    }
    if (phase == ACTOR_800100_DAMAGE_COMPLETE) {
        companionRecoverToIdle(task);
    }
}

/// Keeps the stopped pose for damage-mode selector 3 without returning to idle.
///
/// The dispatcher still ticks animation, turning and movement. This callback
/// ignores its task and has no side effects.
static void _actor800100StoppedDamageState(Task* unusedTask)
{
}

/// Gameplay's player-mode handlers `_actor800100TickScriptedMode` runs,
/// indexed by `state`.
static const TaskFuncTable7 D_actor_800100_80161E98 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    playerActorMode2State2,
    Gp_PlayerMode2State1,
    playerActorTickScriptedMoveTo,
    Gp_PlayerMode2State1,
    playerActorMode2State6,
} };

/// Dispatches the companion's scripted state, updates facing and handles death.
///
/// Requires live actor/model resources and a scripted state in 0..6. States
/// 0/1 advance playback, 2 turns, 4 walks and 6 waits for button presses;
/// 3 and 5 use ordinary playback. Saved companion HP at most zero restores
/// native animation and enters the stopped pose after this tick's dispatch.
static void _actor800100TickScriptedMode(Task* task)
{
    GameActor*     actor;
    TaskFuncTable7 handlers;

    handlers = D_actor_800100_80161E98;
    actor    = task->work;
    handlers.funcs[(u16)actor->state](task);
    playerActorUpdateFacing(task);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        companionInitNativeAnimation(task);
        playerActorEnterStoppedPose(task, 0);
    }
}

/// Chooses an attack, retreat, approach or obstacle scan from target range and HP.
///
/// Requires live actor/companion work, native playback and target-list state.
/// Range is planar game units; targets within 896 always select retreat. Other
/// targets use three health rows of sixteen outcomes per distance band, with
/// the 3072..5119 bands sharing the last row; 5120 or farther selects approach.
/// Without a target, an eight-outcome table selects waiting, approach or scan.
/// Borrows 16 scratch bytes; the XYZ point becomes an in-place displacement.
static void _actor800100DecideCombatBehavior(Task* task)
{
    enum {
        ACTOR_800100_COMBAT_CHOICE_WAIT          = 0,
        ACTOR_800100_COMBAT_CHOICE_ATTACK        = 1,
        ACTOR_800100_COMBAT_CHOICE_RETREAT       = 2,
        ACTOR_800100_COMBAT_CHOICE_APPROACH      = 3,
        ACTOR_800100_COMBAT_CHOICE_SCAN          = 4,
        ACTOR_800100_COMBAT_RETREAT_DISTANCE     = 896,
        ACTOR_800100_COMBAT_DISTANCE_ROUND_MASK  = 0x3FF,
        ACTOR_800100_COMBAT_DISTANCE_SHIFT       = 10,
        ACTOR_800100_COMBAT_LAST_TABLE_BAND      = 3,
        ACTOR_800100_COMBAT_APPROACH_BAND        = 5,
        ACTOR_800100_COMBAT_TABLE_BAND_COUNT     = 4,
        ACTOR_800100_COMBAT_HEALTH_ROW_OUTCOMES  = 16,
        ACTOR_800100_COMBAT_SHORT_TABLE_OUTCOMES = 8,
        ACTOR_800100_COMBAT_COOLDOWN_RANDOM_MASK = 0x1F,
        ACTOR_800100_COMBAT_COOLDOWN_MIN_TICKS   = 15,
        ACTOR_800100_COMBAT_DECISION_BASE_TICKS  = 20,
        ACTOR_800100_COMBAT_DECISION_RANDOM_MASK = 0x3F,
    };
    GameActor*       actor;
    CompanionWork*   companion;
    WorldTargetNode* node;
    GfxCoord*        coord;
    VECTOR3*         targetDelta;
    const u8*        rangeOutcomes;
    const u8*        healthOutcomes;
    s32              targetDistance;
    s32              distanceBand;
    s32              distanceForShift;
    s32              hasOutcomeTable;
    s32              behavior;

    targetDelta       = SCRATCH_STACK_RESERVE_BYTES(sizeof(VECTOR));
    actor             = task->work;
    companion         = actor->companionWork;
    coord             = task->extra.tmd->coords;
    node              = worldTargetFindLockNode(task);
    actor->targetNode = node;
    if (node != NULL) {
        worldTargetGetBodyPosition(node, targetDelta);
        playerActorGetPointDelta(coord, targetDelta, targetDelta);
        targetDistance = playerActorPlanarLength(targetDelta->vx, targetDelta->vz);
        behavior       = ACTOR_800100_COMBAT_CHOICE_RETREAT;
        if (targetDistance >= (ACTOR_800100_COMBAT_RETREAT_DISTANCE + 1)) {
            if (targetDistance < 0) {
                distanceForShift  = targetDistance;
                distanceForShift += ACTOR_800100_COMBAT_DISTANCE_ROUND_MASK;
            } else {
                distanceForShift = targetDistance;
            }
            distanceBand = distanceForShift >> ACTOR_800100_COMBAT_DISTANCE_SHIFT;
            if (distanceBand >= ACTOR_800100_COMBAT_LAST_TABLE_BAND) {
                if (distanceBand < ACTOR_800100_COMBAT_APPROACH_BAND) {
                    distanceBand = ACTOR_800100_COMBAT_LAST_TABLE_BAND;
                }
            }
            hasOutcomeTable = distanceBand < ACTOR_800100_COMBAT_TABLE_BAND_COUNT;
            if (hasOutcomeTable != 0) {
                rangeOutcomes  = D_actor_800100_801672F8[distanceBand];
                healthOutcomes = rangeOutcomes + (companionGetHealthBand() * ACTOR_800100_COMBAT_HEALTH_ROW_OUTCOMES);
                behavior       = healthOutcomes[rand() & (ACTOR_800100_COMBAT_HEALTH_ROW_OUTCOMES - 1)];
            } else {
                behavior = ACTOR_800100_COMBAT_CHOICE_APPROACH;
            }
        } else {
            behavior = ACTOR_800100_COMBAT_CHOICE_RETREAT;
        }
    } else {
        if ((s8)actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
            actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        }
        behavior = D_actor_800100_80167308[rand() & (ACTOR_800100_COMBAT_SHORT_TABLE_OUTCOMES - 1)];
        if (behavior == ACTOR_800100_COMBAT_CHOICE_APPROACH) {
            actor->targetNode = NULL;
        }
    }
    // Preserve signed byte budgets: exhaustion starts reload instead of an attack.
    switch (behavior) {
        case ACTOR_800100_COMBAT_CHOICE_WAIT:
            break;
        case ACTOR_800100_COMBAT_CHOICE_ATTACK:
            if ((s8)companion->activity.combat.attacksRemaining <= 0) {
                actor800100EnterReload(task, 0);
            } else {
                actor->aimTrackingState                     = GAME_ACTOR_AIM_TRACKING_TARGET;
                actor->attackControl.cooldownTicks          = (rand() & ACTOR_800100_COMBAT_COOLDOWN_RANDOM_MASK) + ACTOR_800100_COMBAT_COOLDOWN_MIN_TICKS;
                companion->activity.combat.repeatsRemaining = D_actor_800100_80167310[rand() & (ACTOR_800100_COMBAT_SHORT_TABLE_OUTCOMES - 1)];
                _actor800100EnterAttackLoop(task);
            }
            break;
        case ACTOR_800100_COMBAT_CHOICE_RETREAT:
            _actor800100EnterRetreat(task);
            companionSetDecisionDelay(task, ACTOR_800100_COMBAT_DECISION_BASE_TICKS, ACTOR_800100_COMBAT_DECISION_RANDOM_MASK);
            break;
        case ACTOR_800100_COMBAT_CHOICE_APPROACH:
            _actor800100EnterApproach(task);
            break;
        case ACTOR_800100_COMBAT_CHOICE_SCAN:
            _actor800100EnterObstacleScan(task);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR));
}

static void func_actor_800100_80165C38(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      place;
    u16            state;

    place = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode                                  = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode                          = 0;
            actor->turnRateIndex                         = 0;
            actor->animationState                        = 0;
            actor->stateAux                              = 1;
            companion->activity.combat.attacksRemaining -= 1;
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 3);
            playerActorSetWeaponAttackFlags(arg0, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCoordPlaySound(arg0->extra.tmd->coords, 0x40650001, 1);
            effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, coord, 0x21, NULL);
            break;

        case 1:
            actor->stateAux                                       = 2;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                worldCoordPlaySound(place, 0x17, 1);
            }
            /* fallthrough */

        case 2:
            if (playerActorIsSlotAdvancingLinearly(arg0, 8, 0, 0) == 0) {
                actor->attackControl.cooldownTicks           = 0xA;
                companion->activity.combat.repeatsRemaining -= 1;
                _actor800100EnterAttackLoop(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

static void func_actor_800100_80165DE8(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    u16            state;
    GfxCoord*      coord;

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = 0;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            /* fallthrough */
        case 1:
            actor->stateAux                              = 2;
            companion->activity.combat.attacksRemaining -= 1;
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 3);
            worldCoordPlaySound(coord, 0x40660001, 1);
            if (coord != NULL) {
                enum {
                    ACTOR_800100_MM1_WEAPON            = 12,
                    ACTOR_800100_MM1_MUZZLE_ROW        = 1,
                    ACTOR_800100_MM1_FRAGMENTATION_ARG = PLAYER_ACTOR_GRENADE_COMPANION_SHOT |
                                                         (ACTOR_800100_MM1_MUZZLE_ROW << PLAYER_ACTOR_GRENADE_MUZZLE_ROW_SHIFT) |
                                                         (ACTOR_800100_MM1_WEAPON << PLAYER_ACTOR_GRENADE_WEAPON_SHIFT) | GRENADE_ROUND_FRAGMENTATION,
                };
                actor->attackControl.cooldownTicks = 0x28;
                effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH, coord, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] | 0x10000, NULL);
                playerActorSpawnGrenadeProjectile(arg0, PLAYER_ACTOR_GRENADE_COMPANION, PLAYER_ACTOR_GRENADE_MM1, ACTOR_800100_MM1_FRAGMENTATION_ARG);
                return;
            }
            return;
        case 2:
            if (playerActorIsSlotAdvancingLinearly(arg0, 8, 0, 0) == 0) {
                actor->attackControl.cooldownTicks           = 0x12;
                companion->activity.combat.repeatsRemaining -= 1;
                _actor800100EnterAttackLoop(arg0);
            }
            break;
    }
}

/// Handlers `_actor800100TickWeaponAttack` runs, indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant`.
static const TaskFuncTable5 D_actor_800100_80161EC8 = { {
    func_actor_800100_80165C38,
    func_actor_800100_80165C38,
    func_actor_800100_80165DE8,
    func_actor_800100_80165F50,
    func_actor_800100_80166190,
} };

static void func_actor_800100_80165F50(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      place;
    u16            state;
    void**         scratch;
    GfxCoord*      head;

    scratch                            = SCRATCH_HEAD_ADDR;
    head                               = SCRATCH_HEAD_AT(scratch, GfxCoord);
    SCRATCH_HEAD_AT(scratch, GfxCoord) = head - 1;
    place                              = head - 1;

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode                                   = 0;
            actor->turnRateIndex                                  = 2;
            actor->animationState                                 = 0;
            companion->activity.combat.repeatsRemaining           = (rand() & 7) + 3;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            /* fallthrough */

        case 1:
        block_4:
            actor->stateAux                    = 2;
            actor->attackControl.cooldownTicks = 0;
            actor->stateTimer                  = 3;
            playerActorSetWeaponAttackFlags(arg0, 0, 0);
            /* fallthrough */

        case 2:
            actor->stateTimer -= 1;
            if (actor->stateTimer == 0) {
                actor->stateAux                                      += 1;
                companion->activity.combat.attacksRemaining          -= 1;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                worldCoordPlaySound(arg0->extra.tmd->coords, 0x40670001, 1);
                effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, coord, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] | 0x10000, NULL);
                playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 2);
            }
            break;

        case 3:
            actor->stateAux                                      += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                worldCoordPlaySound(place, 0x17, 1);
            }
            /* fallthrough */

        case 4:
            companion->activity.combat.repeatsRemaining -= 1;
            if ((s8)companion->activity.combat.repeatsRemaining > 0) {
                if ((s8)companion->activity.combat.attacksRemaining > 0) {
                    goto block_4;
                }
            }
            if (playerActorIsSlotAdvancingLinearly(arg0, 8, 0, 0) == 0) {
                _actor800100EnterAttackLoop(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

static void func_actor_800100_80166190(Task* arg0)
{
    void**         scratch;
    GfxCoord*      head;
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      place;
    u16            state;

    scratch                            = SCRATCH_HEAD_ADDR;
    head                               = SCRATCH_HEAD_AT(scratch, GfxCoord);
    SCRATCH_HEAD_AT(scratch, GfxCoord) = head - 1;
    place                              = head - 1;

    actor     = arg0->work;
    companion = actor->companionWork;
    state     = actor->stateAux;
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;

    switch (state) {
        case 0:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = 0;
            actor->animationState = 0;
            actor->stateAux      += 1;
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 1);
            break;

        case 1:
            if (animationGetCurrentRecord(&actor->animationContext,
                                          actor->animationSlots + 1) != NULL) {
                actor->stateAux += 1;
            }
            break;

        case 2:
            gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0xFF) < 0x3F) {
                actor->stateAux                    = 5;
                actor->turnRateIndex               = 2;
                actor->attackControl.cooldownTicks = 0x28;
                actor->attackCancelTicks           = 0x1C;
                actor->actionValue                 = 0x14;
                worldCoordPlaySound(coord, 0x40680002, 1);
                if (actor->weaponEffectTask != NULL) {
                    actor->weaponEffectTask->spawnArg1.value = ACTOR_800100_PYKE_FIRE;
                }
                break;
            }
            actor->stateAux          = 3;
            actor->turnRateIndex     = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = 9;
            actor->actionValue       = 3;
            playerActorSetWeaponAttackFlags(arg0, 0, 1);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            /* fallthrough */

        case 3:
            if (actor->actionValue != 0) {
                if (actor->stateTimer == 0) {
                    actor->actionValue                                    = (u16)actor->actionValue - 1;
                    actor->stateTimer                                     = 3;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    companion->activity.combat.attacksRemaining          -= 1;
                    if ((s8)companion->activity.combat.attacksRemaining == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(coord, 0x40680001, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH, coord, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] | 0x10000, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                    break;
                } else {
                    actor->stateTimer -= 1;
                    if (actor->stateTimer != 0) {
                        break;
                    }
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                    worldCoordPlaySound(place, 0x17, 1);
                }
                break;
            }
            /* fallthrough */

        case 4:
            actor->stateAux                                       = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (func_actor_800100_80166B40(actor->weaponContacts, coord, place) != 0) {
                worldCoordPlaySound(place, 0x17, 1);
            }
            break;

        case 5:
            if (actor->actionValue == 0) {
                actor->stateAux = 6;
                if (actor->weaponEffectTask != NULL) {
                    actor->weaponEffectTask->spawnArg1.value = ACTOR_800100_PYKE_RESET_IDLE;
                }
                sndEvtRequestScriptStop(SOUND_COMPANION_PYKE_FIRE_TAIL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 2);
            } else {
                actor->actionValue = (u16)actor->actionValue - 1;
            }
            break;

        case 6:
            if (playerActorIsSlotAdvancingLinearly(arg0, 8, 0, 0) == 0) {
                actor->attackControl.cooldownTicks          = 0xF;
                companion->activity.combat.repeatsRemaining = (actor->attackButton == 1) ? companion->activity.combat.repeatsRemaining - 1 : 0;
                _actor800100EnterAttackLoop(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}

/// Draws the retained companion aim beam and its textured end square.
///
/// No code or dispatch table calls this routine. Requires an equipped weapon,
/// initialized aim capsule/contacts, GPU packet space and nested scratch space.
/// Places the beam from the weapon, ranges against the first aim contact, draws
/// a line along local Y and places the square 56 units past the stored distance.
/// Placement includes view ancestors in the composed cache. The contact query
/// measures X/Z in that composition frame, rather than along the beam axis.
static void _actor800100DrawAimBeam(Task* task)
{
    enum {
        ACTOR_800100_BEAM_WEAPON_PITCH = 1024,
        ACTOR_800100_BEAM_START_Y      = 288,
        ACTOR_800100_BEAM_START_Z      = 32,
        ACTOR_800100_BEAM_TIP_BIAS     = 56,
    };
    void**                      scratchCursor;
    GameActor*                  actor;
    GfxCoord                    weaponCoord;
    GfxCoord*                   weaponRoot;
    WorldCollisionBody*         aimBody;
    _Actor800100AimBeamScratch* beamScratch;
    s16                         contactDistance;

    actor           = task->work;
    weaponRoot      = actor->equipmentTasks[1]->extra.tmd->coords;
    aimBody         = &actor->collisionBodies[GAME_ACTOR_BODY_AIM];
    weaponCoord     = *weaponRoot;
    aimBody->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    scratchCursor = SCRATCH_HEAD_ADDR;
    beamScratch   = SCRATCH_PUSH_AT(scratchCursor, _Actor800100AimBeamScratch);

    worldCollisionFindContactIndex(aimBody->context.capsule->contacts, WORLD_COLLISION_FIND_ANY_KEY);
    // Placement recomposes weaponCoord and overwrites this retained cache-only rotation.
    gfxRotMatrixX(&weaponCoord.workm, ACTOR_800100_BEAM_WEAPON_PITCH, GRAPHICS_ROTATION_COMPOSE);
    beamScratch->offset.vx = 0;
    beamScratch->offset.vy = ACTOR_800100_BEAM_START_Y;
    beamScratch->offset.vz = ACTOR_800100_BEAM_START_Z;
    actorRenderPlaceCoordOffset(&weaponCoord, &beamScratch->coord, &beamScratch->offset);
    contactDistance              = _actor800100GetContactDistance(&beamScratch->coord, actor->aimContacts, NULL);
    beamScratch->contactDistance = contactDistance;
    _actor800100DrawAimBeamLine(&beamScratch->coord, contactDistance);
    beamScratch->offset.vx = 0;
    beamScratch->offset.vz = 0;
    beamScratch->offset.vy = beamScratch->contactDistance + ACTOR_800100_BEAM_TIP_BIAS;
    actorRenderPlaceCoordOffset(&beamScratch->coord, &beamScratch->coord, &beamScratch->offset);
    _actor800100DrawAimBeamQuad(&beamScratch->coord);
    worldCollisionClearContacts(actor->aimContacts);
    SCRATCH_POP_AT(scratchCursor, _Actor800100AimBeamScratch);
}

/// Draws a pulsing additive red line along the beam coordinate's local Y axis.
///
/// Requires a current full-chain cache, player weapon index 0..32 and GPU/scratch
/// space and a table with 1024 depth tags. A zero contactDistance uses the
/// player's weapon reach narrowed to s16;
/// other s16 values become the local-Y length in game units. Projects directly
/// through the composed cache and emits the line only at tip SZ3/4 >= 32.
static void _actor800100DrawAimBeamLine(const GfxCoord* beamCoord, s16 contactDistance)
{
    enum {
        ACTOR_800100_BEAM_LINE_MIN_DEPTH       = 32,
        ACTOR_800100_BEAM_LINE_PULSE_MASK      = 0x1F,
        ACTOR_800100_BEAM_LINE_RED_BIAS        = 128,
        ACTOR_800100_BEAM_LINE_TIP_FADE        = 80,
        ACTOR_800100_BEAM_LINE_NEAR_GREEN_BLUE = 32,
    };
    void**                          scratchCursor;
    _Actor800100AimBeamLineScratch* previousHead;
    _Actor800100AimBeamLineScratch* reservedBlock;
    _Actor800100AimBeamLineScratch* projectionScratch;
    LINE_G2*                        line;
    s16                             beamLength;
    s32                             originScreenY;
    s32                             tipScreenY;

    scratchCursor                                                  = SCRATCH_HEAD_ADDR;
    previousHead                                                   = SCRATCH_HEAD_AT(scratchCursor, _Actor800100AimBeamLineScratch);
    reservedBlock                                                  = previousHead - 1;
    projectionScratch                                              = reservedBlock;
    SCRATCH_HEAD_AT(scratchCursor, _Actor800100AimBeamLineScratch) = reservedBlock;

    beamLength = contactDistance;
    if (contactDistance == 0) {
        beamLength = D_80112F60[gPlayerStatus.weapon];
    }
    projectionScratch->tip.vy    = beamLength;
    projectionScratch->origin.vx = 0;
    projectionScratch->origin.vy = 0;
    projectionScratch->origin.vz = 0;
    projectionScratch->tip.vx    = 0;
    projectionScratch->tip.vz    = 0;

    gte_SetTransMatrix(&beamCoord->workm);
    gte_SetRotMatrix(&beamCoord->workm);
    gte_ldv0(&projectionScratch->origin);
    gte_rtps();
    gte_stsxy(&projectionScratch->originScreen);
    gte_ldv0(&projectionScratch->tip);
    gte_rtps();
    gte_stsxy(&projectionScratch->tipScreen);
    gte_stszotz(&projectionScratch->otz);

    if (reservedBlock->otz >= ACTOR_800100_BEAM_LINE_MIN_DEPTH) {
        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineG2(line);
        line->x0 = (previousHead - 1)->originScreen.vx;
        /* Both `vy` loads sign-extend, which needs the `s32` locals: a direct
           16-bit field copy assembles to `lhu` for either of them. */
        originScreenY = (previousHead - 1)->originScreen.vy;
        line->y0      = originScreenY;
        line->x1      = reservedBlock->tipScreen.vx;
        tipScreenY    = reservedBlock->tipScreen.vy;
        line->y1      = tipScreenY;
        /* Both ends pulse with the frame counter, the far one 0x50 darker. */
        line->r0 = (rcos(gDisplayState.gameTick) & ACTOR_800100_BEAM_LINE_PULSE_MASK) - ACTOR_800100_BEAM_LINE_RED_BIAS;
        line->g0 = ACTOR_800100_BEAM_LINE_NEAR_GREEN_BLUE;
        line->b0 = ACTOR_800100_BEAM_LINE_NEAR_GREEN_BLUE;
        line->r1 = line->r0 - ACTOR_800100_BEAM_LINE_TIP_FADE;
        line->g1 = 0;
        line->b1 = 0;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)reservedBlock->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), line);
        gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, reservedBlock->otz);
    }
    SCRATCH_POP_AT(scratchCursor, _Actor800100AimBeamLineScratch);
}

/// The four (y, z) corners of the quad `_actor800100DrawAimBeamQuad` draws,
/// offset off the placed coordinate's world translation. The table sits in the
/// unit's .rodata right after the jump tables, so it is written here rather
/// than left to the split: nothing else refers to it.
static const _Actor800100AimBeamQuadCorner D_actor_800100_80161F10[4] = {
    { -62, 0 },
    { -62, 124 },
    { 62, 0 },
    { 62, 124 },
};

/// Draws the aim beam's fixed-orientation additive textured end square.
///
/// Requires the placed coordinate's current full-chain cache, GPU packet space
/// and scratch space. Adds four Y/Z offsets to its composed translation, narrows
/// each component to s16 and projects through GsWSMATRIX. Rotation is not applied
/// to the offsets; the final corner's SZ3/4 selects one of 1024 depth tags,
/// which the current ordering table must provide.
static void _actor800100DrawAimBeamQuad(const GfxCoord* beamCoord)
{
    enum {
        ACTOR_800100_BEAM_QUAD_TEXTURE_PAGE        = 0x27,
        ACTOR_800100_BEAM_QUAD_CLUT                = 0x3CCE,
        ACTOR_800100_BEAM_QUAD_RAW_SEMITRANSPARENT = 3,
        ACTOR_800100_BEAM_QUAD_U_MIN               = 32,
        ACTOR_800100_BEAM_QUAD_U_MAX               = 63,
        ACTOR_800100_BEAM_QUAD_V_MIN               = 128,
        ACTOR_800100_BEAM_QUAD_V_MAX               = 159,
        ACTOR_800100_BEAM_QUAD_DEPTH_TAG_SHIFT     = 4,
    };
    void**                          scratchCursor;
    _Actor800100AimBeamQuadScratch* projectionScratch;
    POLY_FT4*                       quad;
    s32                             cornerIndex;
    s32                             cornerY;
    s32                             cornerZ;
    s32                             screenY;

    scratchCursor     = SCRATCH_HEAD_ADDR;
    projectionScratch = SCRATCH_PUSH_AT(scratchCursor, _Actor800100AimBeamQuadScratch);

    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_actor_800100_80161F10); cornerIndex++) {
        cornerY                                         = D_actor_800100_80161F10[cornerIndex].vy;
        cornerZ                                         = D_actor_800100_80161F10[cornerIndex].vz;
        projectionScratch->cornerOffset.vx              = 0;
        projectionScratch->cornerOffset.vy              = cornerY;
        projectionScratch->cornerOffset.vz              = cornerZ;
        projectionScratch->worldCorners[cornerIndex].vx = (u16)projectionScratch->cornerOffset.vx + (u16)beamCoord->workm.t[0];
        projectionScratch->worldCorners[cornerIndex].vy = (u16)projectionScratch->cornerOffset.vy + (u16)beamCoord->workm.t[1];
        projectionScratch->worldCorners[cornerIndex].vz = (u16)projectionScratch->cornerOffset.vz + (u16)beamCoord->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);

    gte_ldv0(&projectionScratch->worldCorners[0]);
    gte_rtps();

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);

    gte_stsxy2(&projectionScratch->screenCorners[0]);

    gte_ldv3(&projectionScratch->worldCorners[1], &projectionScratch->worldCorners[2], &projectionScratch->worldCorners[3]);
    gte_rtpt();
    quad->tpage = ACTOR_800100_BEAM_QUAD_TEXTURE_PAGE;
    quad->clut  = ACTOR_800100_BEAM_QUAD_CLUT;
    setUV4(quad, ACTOR_800100_BEAM_QUAD_U_MIN, ACTOR_800100_BEAM_QUAD_V_MIN, ACTOR_800100_BEAM_QUAD_U_MAX, ACTOR_800100_BEAM_QUAD_V_MIN, ACTOR_800100_BEAM_QUAD_U_MIN, ACTOR_800100_BEAM_QUAD_V_MAX, ACTOR_800100_BEAM_QUAD_U_MAX, ACTOR_800100_BEAM_QUAD_V_MAX);
    quad->code |= ACTOR_800100_BEAM_QUAD_RAW_SEMITRANSPARENT;

    gte_stsxy3(&projectionScratch->screenCorners[1], &projectionScratch->screenCorners[2], &projectionScratch->screenCorners[3]);
    gte_stszotz(&projectionScratch->otz);

    /* Both `vy` loads sign-extend, which the `s32` locals keep: a direct
       16-bit field copy assembles to `lhu` for either of them. */
    quad->x0 = projectionScratch->screenCorners[0].vx;
    screenY  = projectionScratch->screenCorners[0].vy;
    quad->y0 = screenY;
    quad->x1 = projectionScratch->screenCorners[1].vx;
    screenY  = projectionScratch->screenCorners[1].vy;
    quad->y1 = screenY;
    quad->x2 = projectionScratch->screenCorners[2].vx;
    screenY  = projectionScratch->screenCorners[2].vy;
    quad->y2 = screenY;
    quad->x3 = projectionScratch->screenCorners[3].vx;
    screenY  = projectionScratch->screenCorners[3].vy;
    quad->y3 = screenY;

    addPrim(&gGpuCurrentOt[projectionScratch->otz >> ACTOR_800100_BEAM_QUAD_DEPTH_TAG_SHIFT], quad);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor800100AimBeamQuadScratch);
}

static s32 func_actor_800100_80166B40(WorldCollisionContact* arg0, GfxCoord* arg1, GfxCoord* arg2)
{
    s32                             minDist;
    s32                             idx;
    PlayerActorWeaponImpactScratch* block;
    WorldCollisionContact*          rec;
    s32                             i;
    s32                             bestIdx;
    s32                             dist;

    minDist = 0x7FFFFFFF;
    if (worldCollisionCountContactsByKind(arg0, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
        return 0;
    }
    block = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorWeaponImpactScratch);
    for (i = 0, bestIdx = 0; i < 6; i++) {
        rec = &arg0[i];
        if (rec->key.value & 0x100000) {
            dist  = abs(arg1->workm.t[0] - rec->point.vx);
            dist += abs(arg1->workm.t[1] - rec->point.vy);
            dist += abs(arg1->workm.t[2] - rec->point.vz);
            if (dist < minDist) {
                worldCollisionResolveResponsePushback(rec, &block->pushback, 1, &idx);
                idx = worldCollisionSurfaceClassFromMask((const u8*)&idx);
                if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][idx]->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
                    minDist = dist;
                    bestIdx = i;
                }
            }
        }
    }
    if (minDist != 0x7FFFFFFF) {
        i                               = 1;
        block->impactCoord.parent       = NULL;
        block->impactCoord.composeStamp = GRAPHICS_COORD_SUPPLIED_CACHE;
        block->impactCoord.workm.t[0]   = arg0[bestIdx].point.vx;
        block->impactCoord.workm.t[1]   = arg0[bestIdx].point.vy;
        block->impactCoord.workm.t[2]   = arg0[bestIdx].point.vz;
        block->jitter.vx                = rand() & 7;
        block->jitter.vy                = rand() & 7;
        block->jitter.vz                = rand() & 7;
        if (arg2 != NULL) {
            arg2->workm.t[0] = block->impactCoord.workm.t[0] + block->jitter.vx;
            arg2->workm.t[1] = block->impactCoord.workm.t[1] + block->jitter.vy;
            arg2->workm.t[2] = block->impactCoord.workm.t[2] + block->jitter.vz;
        }
        effectSpawn(EFFECT_IMPACT_SPARK, &block->impactCoord, 0, &block->jitter);
    } else {
        i = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorWeaponImpactScratch);
    return i;
}

/// Restarts the attack loop that waits for cooldown and dispatches the equipped weapon.
///
/// Requires live `GameActor` work. Clears both behavior phases while preserving
/// the target, cooldown and companion attack/repetition budgets.
static void _actor800100EnterAttackLoop(Task* task)
{
    enum { ACTOR_800100_STATE_ATTACK_LOOP = 5 };
    GameActor* actor;

    actor                 = task->work;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = ACTOR_800100_STATE_ATTACK_LOOP;
    actor->animationState = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
    actor->stateAux       = 0;
}

/// Starts the turn-away-and-run combat behavior while decaying tracked aim.
///
/// Requires live actor work and a selected target that remains valid through
/// the retreat behavior's turn phase. Clears its phases; heading, turn animation
/// and run duration are selected on the next behavior update.
static void _actor800100EnterRetreat(Task* task)
{
    enum {
        ACTOR_800100_STATE_RETREAT = 6,
    };
    GameActor* actor;

    actor                   = task->work;
    actor->state            = ACTOR_800100_STATE_RETREAT;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->animationState   = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    actor->statePhase       = 0;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
}

/// Lowers the companion's weapon after combat and waits for the exit clip.
///
/// Requires live actor/native weapon playback and valid saved weapon selectors.
/// Clears lock-on, decays aim and resets the weapon attack. Clip-end controller
/// 7 advances phase 0; the combat-exit handler then returns to idle.
static void _actor800100EnterCombatExit(Task* task)
{
    enum {
        ACTOR_800100_STATE_COMBAT_EXIT     = 8,
        ACTOR_800100_ANIMATION_COMBAT_EXIT = 8,
    };
    GameActor* actor;

    actor                   = task->work;
    actor->state            = ACTOR_800100_STATE_COMBAT_EXIT;
    actor->animationState   = ACTOR_800100_ANIMATION_CONTROLLER_CLIP_END;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->statePhase       = 0;
    actor->targetNode       = NULL;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    playerActorResetWeaponAttack(task, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
    playerActorPlayChildSlotsWithBlend(task, ACTOR_800100_ANIMATION_COMBAT_EXIT, 1, ACTOR_800100_COMBAT_BLEND_FRAMES);
}

void actor800100EnterReload(Task* task, s32 animationVariant)
{
    enum {
        ACTOR_800100_STATE_RELOAD          = 9,
        ACTOR_800100_MOVEMENT_STOPPED      = 0,
        ACTOR_800100_ANIMATION_RELOAD_BASE = 14,
        ACTOR_800100_RELOAD_BLEND_FRAMES   = 1,
    };
    GameActor* actor;

    actor                   = task->work;
    actor->state            = ACTOR_800100_STATE_RELOAD;
    actor->stateAux         = animationVariant;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode     = ACTOR_800100_MOVEMENT_STOPPED;
    actor->turnRateIndex    = ACTOR_800100_TURN_STOPPED;
    actor->animationState   = ACTOR_800100_ANIMATION_CONTROLLER_NONE;
    actor->statePhase       = 0;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    playerActorPlayChildSlotsWithBlend(task, animationVariant + ACTOR_800100_ANIMATION_RELOAD_BASE, 0, ACTOR_800100_RELOAD_BLEND_FRAMES);
}

/// Advances the armed companion's weapon-specific attack handler.
///
/// Requires a saved companion variant 0..4, live actor/companion work and the
/// variant's weapon/native playback resources. Variants 0/1 share the first
/// handler; 2, 3 and 4 select grenade, handgun and rifle/Pyke attack sequences.
/// The attack-loop state calls this only once its cooldown reaches zero.
static void _actor800100TickWeaponAttack(Task* task)
{
    TaskFuncTable5 handlers;

    handlers = D_actor_800100_80161EC8;
    handlers.funcs[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant](task);
}

/// Initializes and links the retained weapon-relative aiming capsule.
///
/// No code or dispatch table calls this routine. With an equipped weapon,
/// borrows its root cache and uses player weapon index 0..32's reach in game units.
/// Links the actor-owned capsule to player attacks with one retained contact;
/// the actor and its contact storage must remain live until that body is unlinked.
/// The cached quarter turn is preserved without changing the copied local matrix.
static void _actor800100InitAimCollision(Task* actorTask)
{
    enum {
        ACTOR_800100_AIM_WEAPON_PITCH   = 1024,
        ACTOR_800100_AIM_COLLISION_KEY  = 0x60000,
        ACTOR_800100_AIM_CAPSULE_Y      = -16,
        ACTOR_800100_AIM_CAPSULE_Z      = 32,
        ACTOR_800100_AIM_CAPSULE_RADIUS = 1,
    };
    GameActor*             actor;
    WorldCollisionBody*    aimBody;
    WorldCollisionCapsule* aimCapsule;
    GfxCoord*              weaponRoot;
    Task*                  weaponTask;

    actor      = actorTask->work;
    weaponTask = actor->equipmentTasks[1];
    if (weaponTask != NULL) {
        aimBody                     = &actor->collisionBodies[GAME_ACTOR_BODY_AIM];
        aimCapsule                  = &actor->aimShape;
        weaponRoot                  = weaponTask->extra.tmd->coords;
        actor->weaponCollisionCoord = *weaponRoot;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, ACTOR_800100_AIM_WEAPON_PITCH, GRAPHICS_ROTATION_COMPOSE);
        aimBody->coord           = &actor->weaponCollisionCoord;
        aimBody->context.capsule = &actor->aimShape;
        aimBody->key             = ACTOR_800100_AIM_COLLISION_KEY;
        aimBody->flags           = WORLD_COLLISION_BODY_CAPSULE;
        aimBody->pos.vx          = 0;
        aimBody->pos.vy          = 0;
        aimBody->pos.vz          = 0;
        aimCapsule->ends[1].vx   = 0;
        aimCapsule->ends[1].vy   = ACTOR_800100_AIM_CAPSULE_Y;
        aimCapsule->ends[0].vx   = aimCapsule->ends[1].vx;
        aimCapsule->ends[1].vz   = ACTOR_800100_AIM_CAPSULE_Z;
        aimCapsule->ends[0].vy   = aimCapsule->ends[1].vy;
        aimCapsule->ends[0].vz   = aimCapsule->ends[1].vz + D_80112F60[gPlayerStatus.weapon];
        aimCapsule->end1Radius   = ACTOR_800100_AIM_CAPSULE_RADIUS;
        aimCapsule->end0Radius   = ACTOR_800100_AIM_CAPSULE_RADIUS;
        aimCapsule->contacts     = actor->aimContacts;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, aimBody);
        worldCollisionInitContacts(aimCapsule->contacts, ARRAY_SIZE(actor->aimContacts), 0);
        aimBody->flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
    }
}

/// Returns the XZ distance from a cached coordinate origin to one keyed contact.
///
/// Requires non-NULL inputs in the same composed frame and integer game units;
/// the origin's `workm` must already be current. Differences, absolute values,
/// squares and their sum must fit s32. Y is ignored. A zero key returns 0,
/// indistinguishable from a contact at the same XZ position.
///
/// Optional `contactZY` provides two writable halfwords. For a nonzero key it
/// receives the signed coordinate bits (Z, Y); an empty contact leaves it alone.
/// The X, Y, Z stores retain their order, with Z overwriting X. No pointer is retained.
static s32 _actor800100GetContactDistance(const GfxCoord* originCoord, const WorldCollisionContact* contact, u16 contactZY[2])
{
    s32 distance;

    if (contact->key.value != 0) {
        distance = playerActorPlanarLength(originCoord->workm.t[0] - contact->point.vx, originCoord->workm.t[2] - contact->point.vz);
        if (contactZY != NULL) {
            // Retain the original repeated first-halfword write.
            contactZY[0] = contact->point.vx;
            contactZY[1] = contact->point.vy;
            contactZY[0] = contact->point.vz;
        }
    } else {
        distance = 0;
    }
    return distance;
}
