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

static void func_actor_800100_801635F4(Task* arg0);
static void func_actor_800100_80163A58(Task* arg0);
static void func_actor_800100_80165528(Task* arg0);

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

/// NULL-terminated `GpuImageUpload*` frame lists for `func_actor_800100_80163A58`,
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
static void func_actor_800100_80163F04(Task* arg0);
static void _actor800100FollowPlayerState(Task* task);
static void _actor800100TurnToTargetState(Task* task);
static void _actor800100CombatEntryState(Task* task);
static void func_actor_800100_80164710(Task* arg0);
static void _actor800100RetreatState(Task* task);
static void func_actor_800100_80164B9C(Task* arg0);
static void func_actor_800100_80164E60(Task* arg0);
static void func_actor_800100_80165010(Task* arg0);
static void func_actor_800100_801652B0(Task* arg0);
static void func_actor_800100_801655C0(Task* arg0);
static void _actor800100EnterApproach(Task* task);
static void _actor800100EnterObstacleScan(Task* task);
static void _actor800100EnterFollowPlayer(Task* task);
static void _actor800100EnterTurnToPlayer(Task* task);
static void _actor800100EnterWander(Task* task);
static void func_actor_800100_80165748(Task* arg0);
static void func_actor_800100_801657D8(Task* arg0);
static void _actor800100CombatExitState(Task* task);
static void _actor800100DamageMode(Task* task);
static void _actor800100DamageRecoveryState(Task* task);
static void _actor800100StoppedDamageState(Task* unusedTask);
static void func_actor_800100_80165930(Task* arg0);
static void _actor800100DecideCombatBehavior(Task* task);
static void func_actor_800100_80165C38(Task* arg0);
static void func_actor_800100_80165DE8(Task* arg0);
static void func_actor_800100_80165F50(Task* arg0);
static void func_actor_800100_80166190(Task* arg0);
static void func_actor_800100_8016666C(GfxCoord* arg0, s16 arg1);
static void func_actor_800100_801668C0(GfxCoord* arg0);
static s32  func_actor_800100_80166B40(WorldCollisionContact* arg0, GfxCoord* arg1, GfxCoord* arg2);
static void _actor800100EnterAttackLoop(Task* task);
static void _actor800100EnterRetreat(Task* task);
static void _actor800100EnterCombatExit(Task* task);
static void func_actor_800100_80166EE8(Task* arg0);
static s32  _actor800100GetContactDistance(const GfxCoord* originCoord, const WorldCollisionContact* contact, u16 contactZY[2]);

/// Animation controller 0 leaves the behavior phase alone; turn-rate row 1 steps 64/4096 turns per tick.
enum {
    ACTOR_800100_ANIMATION_CONTROLLER_NONE = 0,
    ACTOR_800100_TURN_RATE_64              = 1
};

/// Native armed-companion behavior, motion, animation-set and blend selectors.
enum {
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
static void func_actor_800100_80166514(Task* arg0);
static void func_actor_800100_80166F50(Task* arg0);

/// Per-frame flare task of the actor: while the player model is visible
/// (`field_C & 0x80` clear) and effects are visible
/// (`gRoomEffectState->effectControl < 2`) it refreshes transient point-light slot 3.
/// State 0 hangs the flare's coordinate off the actor's own at the fixed
/// offset and zeroes its `age`; state 1 then dispatches on `spawnArg1`:
///
/// - 1 draws the flare at the coordinate's `workm.t` every frame and varies
///   the light's red intensity randomly in `0x400..0xB00`, arming the flare width in
///   `scale`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x60181`
///   as a child of this task, and refreshes the light with a much wider
///   (`0x400` / `0x4000`) falloff and red intensity in `0x800..0xF00`.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `gRoomEffectState->effectControl` is non-zero the two drawing sub-states wind
/// `age` back down instead of advancing.
void func_actor_800100_80161F20(Task* task)
{
    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    GfxCoord*                      light;
    EffectWork*                    eff;
    u32                            ang;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[3];
    light     = &lightSlot->light.head.transform.coord;
    slot      = &lightSlot->light;
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return;
    }
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = D_actor_800100_80167128.vx;
            coord->coord.t[1]   = D_actor_800100_80167128.vy;
            coord->coord.t[2]   = D_actor_800100_80167128.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state = 1;
            break;
        case 1:
            actorRenderComposeCoord(coord);
            switch (task->spawnArg1.value) {
                case 0:
                    break;
                case 1:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        _pykeFlameDrawNozzle(
                            MATRIX_TRANS(&coord->workm), work->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                        break;
                    }
                    _pykeFlameDrawNozzle(
                        MATRIX_TRANS(&coord->workm), work->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                    lightSlot->framesLeft = 4;
                    slot->inner           = 0x80;
                    slot->outer           = 0x400;
                    ang                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState       = ang;
                    slot->head.color.r    = ((ang >> 16) & 0x700) + 0x400;
                    slot->head.color.g    = (u16)slot->head.color.r >> 1;
                    slot->head.color.b    = slot->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    work->scale         = 0x40;
                    break;
                case 2:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        break;
                    }
                    if (work->scale < 0x180) {
                        work->scale = work->scale + 0x40;
                    }
                    eff = effectSpawn(EFFECT_ACTOR_800100_PYKE_FLAME, coord, (s32)(work->scale), NULL);
                    if (eff != NULL) {
                        taskReparent(task, eff->task);
                    }
                    lightSlot->framesLeft = 4;
                    slot->inner           = 0x400;
                    slot->outer           = 0x4000;
                    ang                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState       = ang;
                    slot->head.color.r    = ((ang >> 16) & 0x700) + 0x800;
                    slot->head.color.g    = (u16)slot->head.color.r >> 1;
                    slot->head.color.b    = slot->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    break;
                case 3:
                    task->spawnArg1.value = 1;
                    break;
                case 4:
                    task->spawnArg1.value = 0;
                    break;
                case 5:
                    effectKillTask(work, task);
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
    actor->attachmentTasks[0]     = func_80104258(arg0, 0, 5, 1);
    actor->attachmentTasks[1]     = func_80104258(arg0, 1, 5, 1);
    gPlayerStatus.resourceVariant = savedResourceVariant;
    if (actor->attachmentTasks[1] != NULL) {
        task                     = func_80104364(actor->attachmentTasks[1], save->state.companionType + 1, save->state.companionVariant, 0);
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

static void func_actor_800100_801635F4(Task* arg0)
{
    CompanionMoveScratch* scratch;
    void**                scratchHead;
    CompanionMoveScratch* head;
    GameActor*            actor;
    TmdObject*            work;
    TmdObject*            extra;
    GfxCoord*             coord;
    GfxCoord*             ground;
    CompanionWork*        companion;
    Task*                 task;
    WorldCollisionBody*   objs[2];
    s32                   dy;
    s32                   i;
    s8                    bits;

    scratchHead                                        = SCRATCH_HEAD_ADDR;
    head                                               = SCRATCH_HEAD_AT(scratchHead, CompanionMoveScratch);
    extra                                              = arg0->extra.tmd;
    SCRATCH_HEAD_AT(scratchHead, CompanionMoveScratch) = head - 1;
    work                                               = extra;
    scratch                                            = head - 1;
    coord                                              = work->coords;
    actor                                              = arg0->work;
    companion                                          = actor->companionWork;

    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        (dy = coord->coord.t[1], dy = dy - actor->previousPosition.vy, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->previousPosition.vx;
        coord->coord.t[1] = actor->previousPosition.vy;
        coord->coord.t[2] = actor->previousPosition.vz;
    } else {
        actor->previousPosition.vx = coord->coord.t[0];
        actor->previousPosition.vy = coord->coord.t[1];
        actor->previousPosition.vz = coord->coord.t[2];
        if (actor->collisionEnableMask & 1) {
            actor->gridResponse = worldCollisionApplyResponsePushback(coord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
        } else {
            actor->gridResponse = 0;
        }
    }

    task = actor->equipmentTasks[1];
    if (task != NULL) {
        actor->weaponCollisionCoord = *task->extra.tmd->coords;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, -0x400, GRAPHICS_ROTATION_COMPOSE);
    }

    companion->probe.coord = *arg0->extra.tmd->coords;
    gfxRotMatrixY(&companion->probe.coord.workm, companion->scanAngle, 0);

    objs[0] = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    objs[1] = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    for (i = 0; i < 2; i++) {
        bits = actor->pendingCollisionUpdates;
        if ((bits >> i) & 1) {
            actor->collisionEnableMask |= 1 << i;
            objs[i]->flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (bits & (8 << i)) {
            actor->collisionEnableMask &= ~(1 << i);
            objs[i]->flags             &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;

    if (D_80115768 == 0 && gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        func_actor_800100_80165528(arg0);
    }
    func_actor_800100_80163A58(arg0);

    worldCollisionClearContacts(actor->collisionContacts);
    worldCollisionClearContacts(actor->companionWork->probe.contacts);
    if (actor->equipmentTasks[1] != NULL) {
        worldCollisionClearContacts(actor->weaponContacts);
    }
    if (actor->collisionEnableMask & 1) {
        coord->coord.t[1] += 8;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    if ((s8)actor->usesPushbackDirection != 0) {
        scratch->motionDirection.vx = actor->pushbackDirection.vx;
        scratch->motionDirection.vy = actor->pushbackDirection.vy;
        scratch->motionDirection.vz = actor->pushbackDirection.vz;
    } else {
        scratch->motionDirection.vx = (u16)coord->workm.m[0][2] * (s8) * (volatile u8*)&actor->movementSign;
        scratch->motionDirection.vy = (u16)coord->workm.m[1][2] * (s8) * (volatile u8*)&actor->movementSign;
        scratch->motionDirection.vz = (u16)coord->workm.m[2][2] * (s8) * (volatile u8*)&actor->movementSign;
    }
    actor->collisionMotionContexts[0].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[0].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[0].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[1].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[1].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[1].motionDirection.vz = scratch->motionDirection.vz;
    actor->collisionMotionContexts[2].motionDirection.vx = scratch->motionDirection.vx;
    actor->collisionMotionContexts[2].motionDirection.vy = scratch->motionDirection.vy;
    actor->collisionMotionContexts[2].motionDirection.vz = scratch->motionDirection.vz;

    if (!(work->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        ground               = arg0->extra.tmd->coords + 1;
        ground->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(ground);
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&ground->workm), &scratch->shadowCentre) != 0) {
            effectDrawGroundShadow(&scratch->shadowCentre, 0x200, gRoomEffectState->groundShadowShade);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(CompanionMoveScratch);
}

/// Texture-upload state of the actor: runs two independent sequences, each a
/// countdown (`textureDelayA` / `textureDelayB`, reloaded with 4 / 8) that advances a
/// frame index (`textureFrameA` / `textureFrameB`) through the NULL-terminated image
/// list `D_actor_800100_80167200` / `D_actor_800100_80167210` named by the
/// sequence number (`textureSequenceA` / `textureSequenceB`), ending the sequence when its
/// list runs out. Every upload posts its image over an 8-byte `RECT` borrowed
/// from the scratch stack and gives it back at the end of the call.
static void func_actor_800100_80163A58(Task* arg0)
{
    void**            scratch;
    u8*               head;
    u8*               temp;
    RECT*             rect;
    GameActor*        actor;
    GpuImageUpload*** frameLists;
    s32               idx;
    u32               row;
    GpuImageUpload*   uploadList;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    actor                          = arg0->work;
    temp                           = head - 8;
    SCRATCH_HEAD_AT(scratch, void) = temp;
    rect                           = (RECT*)temp;

    if ((s8)actor->textureSequenceA != 0) {
        actor->textureDelayA--;
        if ((s8)actor->textureDelayA <= 0) {
            frameLists = D_actor_800100_80167200;
            idx        = (s8)actor->textureSequenceA - 1;
            uploadList = frameLists[idx][(s8)actor->textureFrameA];
            if (uploadList != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x28;
                rect->w             = 0x15;
                rect->h             = 8;
                actorRenderUploadTexture(arg0, uploadList, rect);
                actor->textureDelayA = 4;
                actor->textureFrameA++;
            } else {
                actor->textureSequenceA = 0;
            }
        }
    }

    if ((s8)actor->textureSequenceB != 0) {
        actor->textureDelayB--;
        if ((s8)actor->textureDelayB <= 0) {
            frameLists = D_actor_800100_80167210;
            idx        = (row = (s8)actor->textureSequenceB - 1);
            uploadList = frameLists[row][(s8)actor->textureFrameB];
            if (uploadList != NULL) {
                rect->x = 8;
                rect->y = 0x40;
                rect->w = 0xD;
                rect->h = 0xC;
                actorRenderUploadTexture(arg0, uploadList, rect);
                actor->textureDelayB = 8;
                actor->textureFrameB++;
            } else {
                actor->textureSequenceB = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(8);
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
    func_actor_800100_801635F4,
    _actor800100ScheduleTeardown,
    _actor800100Teardown,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects from the four-entry state table - set-up, the per-frame update, a
/// step that only advances to the last state, and the teardown that kills the
/// actor's child tasks and unlinks its objects. The table is a local, so it is
/// copied from `.rodata` onto the stack on every call.
void func_actor_800100_80163CF0(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800100_80161E3C;
    states.funcs[task->state](task);
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

/// Handlers `func_actor_800100_80165528` runs, indexed by `mode`.
static const TaskFuncTable3 D_actor_800100_80161E4C = { {
    func_actor_800100_80163F04,
    _actor800100DamageMode,
    func_actor_800100_80165930,
} };

/// Handlers `func_actor_800100_80163F04` runs, indexed by `state`.
static const TaskFuncTable12 D_actor_800100_80161E58 = { {
    func_actor_800100_80165748,
    _actor800100FollowPlayerState,
    _actor800100TurnToTargetState,
    _actor800100CombatEntryState,
    func_actor_800100_801657D8,
    func_actor_800100_80164710,
    _actor800100RetreatState,
    func_actor_800100_80164B9C,
    _actor800100CombatExitState,
    func_actor_800100_80164E60,
    func_actor_800100_80165010,
    func_actor_800100_801652B0,
} };

/// Drives the actor's `mode`/`state` callback tables while the
/// `effectTimer.waterDripTicks` countdown runs, spawning the drip effect every tenth frame.
/// `sp40` / `sp48` hold the effect position: it rides the water surface
/// (`gGameSession.waterY`) minus the actor coordinate's world Y.
static void func_actor_800100_80163F04(Task* arg0)
{
    TaskFuncTable12 sp;
    SVECTOR         sp40;
    SVECTOR         sp48;
    GameActor*      actor;
    CompanionWork*  companion;
    GfxCoord*       coord;
    s16             temp;
    s16             rem;
    s32             pan;

    sp        = D_actor_800100_80161E58;
    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    companion = actor->companionWork;
    if (companion->decisionTimer > 0) {
        companion->decisionTimer--;
    }
    sp.funcs[actor->state](arg0);
    if ((u32)(playerActorPlayFootstepCue(arg0) + 0xEFFFFF77) < 4) {
        actor->effectTimer.waterDripTicks = 0x78;
        sp40.vx                           = 0;
        sp40.vy                           = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
        sp40.vz                           = 0;
        effectSpawn(gRoomEffectWaterSprayId, coord, 0x1202180, &sp40);
        effectSpawn(gRoomEffectWaterRippleId, coord, (rand() & 0x1F) | 0x40, &sp40);
    }
    temp = (u16)actor->effectTimer.waterDripTicks;
    if (temp != 0) {
        actor->effectTimer.waterDripTicks--;
        rem = temp % 10;
        if (rem == 0) {
            sp48.vx = 0;
            sp48.vy = (u16)gGameSession->waterY - (u16)coord->coord.t[1];
            sp48.vz = 0;
            effectSpawn(gRoomEffectWaterRippleId, coord, (rand() & 0x1F) | 0x40, &sp48);
        }
    }
    if ((s8)actor->recoveryTicks == 0) {
        playerActorResolveBodyContacts(arg0, actor->collisionContacts);
        if ((u16)actor->hitRegion != 0) {
            companionEnterDamageReaction(arg0);
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant - 1) << 16) + 0x4065000A, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
    }
    playerActorTickAnimationState(arg0);
    playerActorTickChildSlots(arg0);
    playerActorUpdateFacing(arg0);
    playerActorStepMovement(arg0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        playerActorEnterStoppedPose(arg0, 0);
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

static void func_actor_800100_80164710(Task* arg0)
{
    GameActor*                 actor;
    GameActor*                 actor2;
    GameActor*                 actor3;
    CompanionWork*             companion;
    WorldTargetNode*           node;
    WorldTargetNode*           lock;
    GfxCoord*                  coord;
    _Actor800100TargetScratch* block;
    s32                        turnDelta;

    actor     = arg0->work;
    block     = SCRATCH_STACK_RESERVE_BLOCK(_Actor800100TargetScratch);
    companion = actor->companionWork;
    companionTrackLockTarget(arg0, COMPANION_LOCK_TRACK_YAW | COMPANION_LOCK_TRACK_PITCH);
    switch (actor->statePhase) {
        case 0:
            lock = actor->targetNode;
            if ((lock == NULL) || (coord = arg0->extra.tmd->coords, worldTargetGetBodyPosition(lock, &block->targetPoint), playerActorGetPointDelta(coord, &block->targetPoint, &block->targetDelta), ((playerActorPlanarLength(block->targetDelta.vx, block->targetDelta.vz) < 0x301) != 0))) {
                actor2                 = arg0->work;
                actor2->mode           = GAME_ACTOR_MODE_NORMAL;
                actor2->state          = 4;
                actor2->animationState = 0;
                actor2->statePhase     = 0;
                actor2->movementSign   = 0;
                actor2->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
                break;
            }
            turnDelta = playerActorGetTurnToPoint(arg0, &block->targetPoint);
            if (turnDelta < 0) {
                turnDelta = -turnDelta;
            }
            if (turnDelta >= 0x181) {
                break;
            }
            actor->statePhase += 1;
            /* fallthrough */
        case 1:
            if (((s8)companion->activity.combat.repeatsRemaining <= 0) || (node = actor->targetNode, node == NULL) || (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
                // Stored through a plain pointer: the member-access spelling schedules differently.
                *&actor->targetNode                                   = NULL;
                actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_DECAY;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if ((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant == 4) {
                    playerActorResetWeaponAttack(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
                }
                actor3                 = arg0->work;
                actor3->mode           = GAME_ACTOR_MODE_NORMAL;
                actor3->state          = 4;
                actor3->animationState = 0;
                actor3->statePhase     = 0;
                actor3->movementSign   = 0;
                actor3->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
            } else if (actor->attackControl.cooldownTicks == 0) {
                func_actor_800100_80166EE8(arg0);
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

/// Aim/lock drive for the actor's `statePhase` phase machine. While the planar distance
/// to what the ally block's `companionWork->probe.contacts` recorded is nonzero and under
/// `0x301`, `actionValue` counts up and the LCG decides the next aim window:
/// once the step passes `((gRandomLcgState >> 16) & 0x3F) + 0x28` the actor
/// latches into the `0xA` / child-slot-1 chain, keeping the old `state` in
/// `stateAux` and clearing the aim offset on `companionWork`. Otherwise it reserves
/// a `_Actor800100TargetScratch` on the scratch stack, fills `targetPoint`
/// either from the lock node (`worldTargetGetBodyPosition`) or from the player's model
/// coordinate, runs the `statePhase` switch, measures the planar length of
/// `targetDelta`, and drops back to child slot 9 once the target is within the
/// rolled reach. Both arms end by handing `targetPoint` to `playerActorTurnBodyTowardPoint` /
/// `playerActorTurnAimTowardPoint` and releasing the block.
static void func_actor_800100_80164B9C(Task* arg0)
{
    GameActor*                 actor;
    GameActor*                 actor2;
    GameActor*                 actor3;
    CompanionWork*             companion;
    GfxCoord*                  coord;
    GfxCoord*                  target;
    _Actor800100TargetScratch* block;
    WorldTargetNode*           node;
    u16                        step;
    u16                        old;
    u32                        random;
    s32                        distance;
    s32                        val;

    coord    = arg0->extra.tmd->coords;
    target   = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor    = arg0->work;
    distance = _actor800100GetContactDistance(coord, actor->companionWork->probe.contacts, NULL);
    if (distance != 0 && distance < 0x301) {
        step               = actor->actionValue + 1;
        actor->actionValue = step;
        random             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = random;
        if ((s16)step >= (s32)(((random >> 16) & 0x3F) + 0x28)) {
            actor2                   = arg0->work;
            old                      = actor2->state;
            actor2->state            = 0xA;
            actor2->turnRateIndex    = 1;
            actor2->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
            companion                = actor2->companionWork;
            actor2->mode             = GAME_ACTOR_MODE_NORMAL;
            actor2->animationState   = 0;
            actor2->statePhase       = 0;
            actor2->stateAux         = old;
            companion->scanClearance = COMPANION_SCAN_UNTESTED;
            companion->scanAngle     = 0;
            playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 6);
            return;
        }
    }
    block = SCRATCH_STACK_RESERVE_BLOCK(_Actor800100TargetScratch);
    node  = actor->targetNode;
    if (node != NULL) {
        if ((node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) == 0) {
            worldTargetGetBodyPosition(node, &block->targetPoint);
        } else {
            actor->statePhase = 2;
        }
    } else {
        block->targetPoint.vx = target->coord.t[0];
        block->targetPoint.vy = target->coord.t[1];
        block->targetPoint.vz = target->coord.t[2];
    }
    switch (actor->statePhase) {
        case 0:
            actor->statePhase   = 1;
            actor->stateTimer   = 0;
            actor->movementMode = 3;
            playerActorPlayChildSlotsWithBlend(arg0, 0xC, 0, 5);
            /* fallthrough */
        case 1:
        case 2:
            actor->movementSign = 1;
            playerActorGetPointDelta(coord, &block->targetPoint, &block->targetDelta);
            distance = playerActorPlanarLength(block->targetDelta.vx, block->targetDelta.vz);
            if (actor->targetNode != NULL) {
                val = (rand() & 0x3FF) + 0xB00;
            } else {
                val = 0xB00;
            }
            if (val >= distance || actor->statePhase == 2) {
                actor3                 = arg0->work;
                actor3->mode           = GAME_ACTOR_MODE_NORMAL;
                actor3->state          = 4;
                actor3->animationState = 0;
                actor3->statePhase     = 0;
                actor3->movementSign   = 0;
                actor3->turnSign       = 0;
                playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
            }
            break;
    }
    playerActorTurnBodyTowardPoint(arg0, &block->targetPoint);
    playerActorTurnAimTowardPoint(arg0, &block->targetPoint);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor800100TargetScratch);
}

static void func_actor_800100_80164E60(Task* arg0)
{
    GameActor*             actor;
    GameActor*             target;
    CompanionWork*         companion;
    const AnimationRecord* rec;
    GfxCoord*              coord;
    s16                    sel;

    actor     = arg0->work;
    companion = actor->companionWork;
    rec       = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1);
    coord     = actor->equipmentTasks[1]->extra.tmd->coords;
    sel       = D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant];

    switch (sel) {
        case 3:
            if (rec != NULL) {
                if (rec != actor->lastCueRecord) {
                    actor->lastCueRecord = rec;
                    if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                        if (actor->statePhase == 0) {
                            actor->statePhase = 1;
                        }
                    }
                }
            }
            break;
        case 12:
            if (actor->statePhase == 0) {
                actor->statePhase = 1;
                effectSpawn(EFFECT_RELOAD_EMITTER, coord, 0xC, NULL);
            }
            if (rec != NULL) {
                if (rec != actor->lastCueRecord) {
                    actor->lastCueRecord = rec;
                }
            }
            break;
        default:
            if (actor->statePhase == 0) {
                actor->statePhase = 1;
            } else {
                if (rec != NULL) {
                    if (rec != actor->lastCueRecord) {
                        actor->lastCueRecord = rec;
                    }
                }
            }
            break;
    }

    companion->activity.combat.attacksRemaining = D_actor_800100_80167230[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant];
    if (rec != NULL && playerActorIsSlotAdvancingLinearly(arg0, 1, 0, 0) == 0) {
        target                 = arg0->work;
        target->mode           = GAME_ACTOR_MODE_NORMAL;
        target->state          = 4;
        target->animationState = 0;
        target->statePhase     = 0;
        target->movementSign   = 0;
        target->turnSign       = 0;
        playerActorPlayChildSlotsWithBlend(arg0, 9, 0, 6);
    }
}

static void func_actor_800100_80165010(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    GfxCoord*      target;
    s32            dist;
    u32            rng;
    s32            arg;
    s32            slot;
    s32            diff;
    s32            flag;
    s32            turn;
    s32            state;
    s32            heading;
    u16            angle;

    coord     = arg0->extra.tmd->coords;
    target    = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actor     = arg0->work;
    companion = actor->companionWork;
    dist      = _actor800100GetContactDistance(coord, companion->probe.contacts, NULL);
    state     = actor->statePhase;
    flag      = 1;

    switch (state) {
        case 0:
            if (companion->scanAngle < ACTOR_TRANSFORM_ANGLE_TURN) {
                // Prefer the farthest contact, with a clear direction ending the search.
                if (companion->scanClearance != COMPANION_SCAN_CLEAR && (companion->scanClearance < dist || dist == COMPANION_SCAN_CLEAR)) {
                    companion->scanClearance = dist;
                    companion->targetHeading = companion->scanAngle;
                }
                companion->scanAngle += COMPANION_SCAN_ANGLE_STEP;
            } else {
                actor->statePhase        = flag;
                angle                    = (companion->targetHeading + actor->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
                companion->targetHeading = angle;
                turn                     = playerActorShortestTurn(actor->rotation.vy, angle);
                arg                      = 5;
                if (turn > 0) {
                    arg                = 6;
                    companion->turnDir = flag;
                } else {
                    companion->turnDir = -1;
                }
                playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 3);
            }
            break;
        case 1:
            actor->turnSign = (u8)companion->turnDir;
            do {
                heading = actor->rotation.vy;
                state   = companion->targetHeading;
                diff    = heading - state;
                if (diff < 0) {
                    diff = -diff;
                }
            } while (0);
            if (diff < 0x40) {
                actor->statePhase++;
                actor->rotation.vy = companion->targetHeading;
                actor->turnSign    = 0;
                if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                    slot                = 4;
                    actor->movementMode = 3;
                    rng                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = rng;
                    actor->stateTimer   = ((rng >> 16) & 0x3F) + 0x14;
                    playerActorPlayChildSlotsWithBlend(arg0, slot, 0, 3);
                } else {
                    slot                = 2;
                    actor->movementMode = 1;
                    rng                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState     = rng;
                    actor->stateTimer   = ((rng >> 16) & 0x7F) + 0x28;
                    playerActorPlayChildSlotsWithBlend(arg0, slot, 0, 3);
                }
            }
            break;
        case 2:
            if ((dist < 0x301 && dist != 0) || --actor->stateTimer <= 0) {
                companionEnterIdle(arg0, 0);
            } else {
                actor->movementSign = 1;
            }
            break;
    }
    playerActorTurnAimTowardPoint(arg0, MATRIX_TRANS(&target->coord));
}

static void func_actor_800100_801652B0(Task* arg0)
{
    GameActor*     actor;
    CompanionWork* companion;
    GameActor*     target;
    CompanionWork* targetCompanion;
    s32            flag;
    s32            arg;
    s32            val;
    s32            dist;
    s32            targetDist;
    s32            turn;
    s32            state;
    s16            ang;
    s16            anim;
    u16            old;

    actor      = arg0->work;
    companion  = actor->companionWork;
    targetDist = _actor800100GetContactDistance(arg0->extra.tmd->coords, companion->probe.contacts, NULL);
    state      = actor->statePhase;
    flag       = 1;
    switch (state) {
        case 0:
            actor->statePhase        = flag;
            ang                      = (actor->rotation.vy + (rand() & ACTOR_TRANSFORM_ANGLE_MASK)) & ACTOR_TRANSFORM_ANGLE_MASK;
            companion->targetHeading = ang;
            turn                     = playerActorShortestTurn(actor->rotation.vy, ang);
            arg                      = 5;
            if (turn << 16 > 0) {
                arg                = 6;
                companion->turnDir = flag;
            } else {
                companion->turnDir = -1;
            }
            playerActorPlayChildSlotsWithBlend(arg0, arg, 0, 3);
        case 1:
            val             = (u8)companion->turnDir;
            actor->turnSign = val;
            val             = actor->rotation.vy;
            state           = companion->targetHeading;
            dist            = val - state;
            if (dist < 0) {
                dist = -dist;
            }
            if (dist < 0x40) {
                actor->statePhase  += 1;
                actor->rotation.vy  = (u16)companion->targetHeading;
                actor->turnSign     = 0;
                actor->movementMode = 1;
                val                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = val;
                val                 = (((u32)val >> 16) & 0x7F) + 0x1E;
                actor->stateTimer   = val;
                playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 3);
                break;
            }
            break;
        case 2:
            val               = actor->stateTimer - 1;
            actor->stateTimer = val;
            if (val != 0) {
                break;
            }
            if (targetDist < 0x401 && targetDist != 0) {
                anim                           = 1;
                target                         = arg0->work;
                old                            = target->state;
                target->state                  = 0xA;
                target->aimTrackingState       = anim;
                targetCompanion                = target->companionWork;
                target->mode                   = GAME_ACTOR_MODE_NORMAL;
                target->turnRateIndex          = flag;
                target->animationState         = 0;
                target->statePhase             = 0;
                target->stateAux               = old;
                targetCompanion->scanClearance = COMPANION_SCAN_UNTESTED;
                targetCompanion->scanAngle     = 0;
                playerActorPlayChildSlotsWithBlend(arg0, anim, 0, 6);
                break;
            }
            actor->statePhase += 1;
            actor->stateTimer  = (rand() & 0x3F) + 0x3C;
            playerActorPlayChildSlotsWithBlend(arg0, 2, 0, 3);
            break;
        case 3:
            if (targetDist < 0x301 && targetDist != 0) {
                companionEnterIdle(arg0, 0);
                break;
            }
            val               = actor->stateTimer - 1;
            actor->stateTimer = val;
            if (val <= 0) {
                companionEnterIdle(arg0, 0);
                break;
            }
            actor->movementSign = flag;
            break;
    }
}

static void func_actor_800100_80165528(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp    = D_actor_800100_80161E4C;
    actor = arg0->work;
    if (actor->attackControl.cooldownTicks > 0) {
        actor->attackControl.cooldownTicks--;
    }
    if ((s8)actor->recoveryTicks > 0) {
        actor->recoveryTicks--;
    }
    actor->movementSign = 0;
    actor->turnSign     = 0;
    sp.funcs[actor->mode](arg0);
    actor->usesPushbackDirection = 0;
}

static void func_actor_800100_801655C0(Task* arg0)
{
    GameActor* actor;

    actor                                                  = arg0->work;
    actor->state                                           = 3;
    actor->mode                                            = GAME_ACTOR_MODE_NORMAL;
    actor->animationState                                  = 0;
    actor->statePhase                                      = 0;
    actor->companionWork->activity.combat.repeatsRemaining = 0;
    actor->aimTrackingState                                = GAME_ACTOR_AIM_TRACKING_TARGET;
    actor->targetNode                                      = worldTargetFindLockNode(arg0);
    playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 6);
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

static void func_actor_800100_80165748(Task* arg0)
{
    GameActor* actor;

    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        actor                                                  = arg0->work;
        actor->state                                           = 3;
        actor->mode                                            = GAME_ACTOR_MODE_NORMAL;
        actor->animationState                                  = 0;
        actor->statePhase                                      = 0;
        actor->companionWork->activity.combat.repeatsRemaining = 0;
        actor->aimTrackingState                                = GAME_ACTOR_AIM_TRACKING_TARGET;
        actor->targetNode                                      = worldTargetFindLockNode(arg0);
        playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 6);
        return;
    }
    _actor800100DecideIdleBehavior(arg0);
}

static void func_actor_800100_801657D8(Task* arg0)
{
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
        _actor800100EnterCombatExit(arg0);
        return;
    }
    _actor800100DecideCombatBehavior(arg0);
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

/// Gameplay's player-mode handlers `func_actor_800100_80165930` runs,
/// indexed by `state`.
static const TaskFuncTable7 D_actor_800100_80161E98 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    playerActorMode2State2,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State4,
    Gp_PlayerMode2State1,
    playerActorMode2State6,
} };

static void func_actor_800100_80165930(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable7 sp;

    sp    = D_actor_800100_80161E98;
    actor = arg0->work;
    sp.funcs[(u16)actor->state](arg0);
    playerActorUpdateFacing(arg0);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
        companionInitNativeAnimation(arg0);
        playerActorEnterStoppedPose(arg0, 0);
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
                actor->attackControl.cooldownTicks = 0x28;
                effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH, coord, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] | 0x10000, NULL);
                func_80104490(arg0, 1, 2, 0x110C0A);
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

/// Handlers `func_actor_800100_80166EE8` runs, indexed by `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant`.
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
                    actor->weaponEffectTask->spawnArg1.value = 2;
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
                    actor->weaponEffectTask->spawnArg1.value = 3;
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

static void func_actor_800100_80166514(Task* arg0)
{
    void**                      scratch;
    GameActor*                  actor;
    GfxCoord                    sp10;
    GfxCoord*                   src;
    WorldCollisionBody*         obj;
    _Actor800100AimBeamScratch* blk;
    s16                         distance;

    actor       = arg0->work;
    src         = actor->equipmentTasks[1]->extra.tmd->coords;
    obj         = &actor->collisionBodies[GAME_ACTOR_BODY_AIM];
    sp10        = *src;
    obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    scratch = SCRATCH_HEAD_ADDR;
    blk     = SCRATCH_PUSH_AT(scratch, _Actor800100AimBeamScratch);

    worldCollisionFindContactIndex(obj->context.capsule->contacts, WORLD_COLLISION_FIND_ANY_KEY);
    gfxRotMatrixX(&sp10.workm, 0x400, GRAPHICS_ROTATION_COMPOSE);
    blk->offset.vx = 0;
    blk->offset.vy = 0x120;
    blk->offset.vz = 0x20;
    actorRenderPlaceCoordOffset(&sp10, &blk->coord, &blk->offset);
    distance             = _actor800100GetContactDistance(&blk->coord, actor->aimContacts, NULL);
    blk->contactDistance = distance;
    func_actor_800100_8016666C(&blk->coord, distance);
    blk->offset.vx = 0;
    blk->offset.vz = 0;
    blk->offset.vy = blk->contactDistance + 0x38;
    actorRenderPlaceCoordOffset(&blk->coord, &blk->coord, &blk->offset);
    func_actor_800100_801668C0(&blk->coord);
    worldCollisionClearContacts(actor->aimContacts);
    SCRATCH_POP_AT(scratch, _Actor800100AimBeamScratch);
}

/* The block is carved off `head` into `newhead` and stored there, but the GTE
   calls address it through `blk`: the ROM keeps that pointer as a copy of
   `newhead` in `$a3`, and one variable for both drops it. The post-`rcos`
   reads go through `newhead` for the same reason - naming `blk` there would
   keep the copy live across the call - and the two `originScreen` reads are
   spelled off `head`, whose folded address is the one the ROM uses. */
static void func_actor_800100_8016666C(GfxCoord* arg0, s16 arg1)
{
    void**                          scratch;
    _Actor800100AimBeamLineScratch* head;
    _Actor800100AimBeamLineScratch* newhead;
    _Actor800100AimBeamLineScratch* blk;
    LINE_G2*                        prim;
    s16                             angle;
    s32                             sy0;
    s32                             sy1;

    scratch                                                  = SCRATCH_HEAD_ADDR;
    head                                                     = SCRATCH_HEAD_AT(scratch, _Actor800100AimBeamLineScratch);
    newhead                                                  = head - 1;
    blk                                                      = newhead;
    SCRATCH_HEAD_AT(scratch, _Actor800100AimBeamLineScratch) = newhead;

    angle = arg1;
    if (arg1 == 0) {
        angle = D_80112F60[gPlayerStatus.weapon];
    }
    blk->tip.vy    = angle;
    blk->origin.vx = 0;
    blk->origin.vy = 0;
    blk->origin.vz = 0;
    blk->tip.vx    = 0;
    blk->tip.vz    = 0;

    gte_SetTransMatrix(&arg0->workm);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(&blk->origin);
    gte_rtps();
    gte_stsxy(&blk->originScreen);
    gte_ldv0(&blk->tip);
    gte_rtps();
    gte_stsxy(&blk->tipScreen);
    gte_stszotz(&blk->otz);

    if (newhead->otz >= 0x20) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineG2(prim);
        prim->x0 = (head - 1)->originScreen.vx;
        /* Both `vy` loads sign-extend, which needs the `s32` locals: a direct
           16-bit field copy assembles to `lhu` for either of them. */
        sy0      = (head - 1)->originScreen.vy;
        prim->y0 = sy0;
        prim->x1 = newhead->tipScreen.vx;
        sy1      = newhead->tipScreen.vy;
        prim->y1 = sy1;
        /* Both ends pulse with the frame counter, the far one 0x50 darker. */
        prim->r0 = (rcos(gDisplayState.gameTick) & 0x1F) - 0x80;
        prim->g0 = 0x20;
        prim->b0 = 0x20;
        prim->r1 = prim->r0 - 0x50;
        prim->g1 = 0;
        prim->b1 = 0;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)newhead->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, newhead->otz);
    }
    SCRATCH_POP_AT(scratch, _Actor800100AimBeamLineScratch);
}

/// The four (y, z) corners of the quad `func_actor_800100_801668C0` draws,
/// offset off the placed coordinate's world translation. The table sits in the
/// unit's .rodata right after the jump tables, so it is written here rather
/// than left to the split: nothing else refers to it.
static const _Actor800100AimBeamQuadCorner D_actor_800100_80161F10[4] = {
    { -62, 0 },
    { -62, 124 },
    { 62, 0 },
    { 62, 124 },
};

/* The beam quad `func_actor_800100_80166514` places: the four
   `D_actor_800100_80161F10` (y, z) offsets, raised to the coordinate's world
   translation, projected through `GsWSMATRIX` and textured with one 0x20
   square of the atlas.
   The block is reserved and its pointer taken in one assignment: the ROM
   keeps the allocated pointer as a copy of the store's temporary in `$t1`,
   and splitting the two into separate statements drops that copy. */
static void func_actor_800100_801668C0(GfxCoord* arg0)
{
    void**                          scratch;
    _Actor800100AimBeamQuadScratch* blk;
    POLY_FT4*                       prim;
    s32                             i;
    s32                             ay;
    s32                             az;
    s32                             sy;

    scratch = SCRATCH_HEAD_ADDR;
    blk     = SCRATCH_PUSH_AT(scratch, _Actor800100AimBeamQuadScratch);

    for (i = 0; i < 4; i++) {
        ay                      = D_actor_800100_80161F10[i].vy;
        az                      = D_actor_800100_80161F10[i].vz;
        blk->cornerOffset.vx    = 0;
        blk->cornerOffset.vy    = ay;
        blk->cornerOffset.vz    = az;
        blk->worldCorners[i].vx = (u16)blk->cornerOffset.vx + (u16)arg0->workm.t[0];
        blk->worldCorners[i].vy = (u16)blk->cornerOffset.vy + (u16)arg0->workm.t[1];
        blk->worldCorners[i].vz = (u16)blk->cornerOffset.vz + (u16)arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);

    gte_ldv0(&blk->worldCorners[0]);
    gte_rtps();

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);

    gte_stsxy2(&blk->screenCorners[0]);

    gte_ldv3(&blk->worldCorners[1], &blk->worldCorners[2], &blk->worldCorners[3]);
    gte_rtpt();
    prim->tpage = 0x27;
    prim->clut  = 0x3CCE;
    setUV4(prim, 0x20, 0x80, 0x3F, 0x80, 0x20, 0x9F, 0x3F, 0x9F);
    prim->code |= 3;

    gte_stsxy3(&blk->screenCorners[1], &blk->screenCorners[2], &blk->screenCorners[3]);
    gte_stszotz(&blk->otz);

    /* Both `vy` loads sign-extend, which the `s32` locals keep: a direct
       16-bit field copy assembles to `lhu` for either of them. */
    prim->x0 = blk->screenCorners[0].vx;
    sy       = blk->screenCorners[0].vy;
    prim->y0 = sy;
    prim->x1 = blk->screenCorners[1].vx;
    sy       = blk->screenCorners[1].vy;
    prim->y1 = sy;
    prim->x2 = blk->screenCorners[2].vx;
    sy       = blk->screenCorners[2].vy;
    prim->y2 = sy;
    prim->x3 = blk->screenCorners[3].vx;
    sy       = blk->screenCorners[3].vy;
    prim->y3 = sy;

    addPrim(&gGpuCurrentOt[blk->otz >> 4], prim);
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

static void func_actor_800100_80166EE8(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_800100_80161EC8;
    sp.funcs[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant](arg0);
}

static void func_actor_800100_80166F50(Task* arg0)
{
    GameActor*             actor;
    WorldCollisionBody*    obj;
    WorldCollisionCapsule* rec;
    GfxCoord*              src;
    Task*                  task;

    actor = arg0->work;
    task  = actor->equipmentTasks[1];
    if (task != NULL) {
        obj                         = &actor->collisionBodies[GAME_ACTOR_BODY_AIM];
        rec                         = &actor->aimShape;
        src                         = task->extra.tmd->coords;
        actor->weaponCollisionCoord = *src;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, 0x400, GRAPHICS_ROTATION_COMPOSE);
        obj->coord           = &actor->weaponCollisionCoord;
        obj->context.capsule = &actor->aimShape;
        obj->key             = 0x60000;
        obj->flags           = WORLD_COLLISION_BODY_CAPSULE;
        obj->pos.vx          = 0;
        obj->pos.vy          = 0;
        obj->pos.vz          = 0;
        rec->ends[1].vx      = 0;
        rec->ends[1].vy      = -0x10;
        rec->ends[0].vx      = rec->ends[1].vx;
        rec->ends[1].vz      = 0x20;
        rec->ends[0].vy      = rec->ends[1].vy;
        rec->ends[0].vz      = rec->ends[1].vz + D_80112F60[gPlayerStatus.weapon];
        rec->end1Radius      = 1;
        rec->end0Radius      = 1;
        rec->contacts        = actor->aimContacts;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, obj);
        worldCollisionInitContacts(rec->contacts, 1, 0);
        obj->flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
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
