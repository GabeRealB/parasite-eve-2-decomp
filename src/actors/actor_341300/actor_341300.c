#include "actors/actor_341300.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/tmd_types.h"

/// Advances the shared random sequence once and returns its high 16 bits for a shard draw.
///
/// Returns a u32 in 0..65535. The multiplication and addition wrap unsigned
/// at 32 bits, and the updated `gRandomLcgState` is visible to other consumers.
/// Each invocation consumes a draw, including separate sign and magnitude draws;
/// evaluating this expression again advances the sequence again.
#define ACTOR_341300_DRAW_RANDOM_U16() ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)

/// Work block of a debris shard task: one flat triangle that tumbles and falls
/// under gravity until its origin drops below y 0.
///
/// The task allocates the block zeroed and fills it once, on its first frame,
/// from its own random draws; afterwards only `rot` and `vel.vy` change. The
/// shard's position is not kept here but in the task's coordinate, which is
/// parented to the view.
typedef struct {
    SVECTOR rot;      // Tumble angles (4096 = one turn), rebuilt into the coordinate's rotation each frame as Y, then X, then Z; `pad` is never set
    SVECTOR spin;     // Per-frame step of `rot`, 100 to 227 either way per axis; `pad` is never set
    SVECTOR vel;      // Per-frame translation of the coordinate, up to 31 either way per axis at spawn, with 8 added to `vy` each frame; `pad` is never set
    SVECTOR verts[3]; // The triangle's corners in the shard's own XY plane: equilateral about the origin at radius 20, each nonzero coordinate randomly pushed 2 further out
} _Actor341300ShardWork;
STATIC_ASSERT_SIZEOF(_Actor341300ShardWork, 0x30);

/// Scene-event value written at the end of either event-script path.
enum { ACTOR_341300_SCENE_EVENT_AFTER_SCRIPT = 12 };

/// Display-task states and update counts, preserving the script's halfword timer.
enum {
    ACTOR_341300_QUADS_EXIT             = -1,
    ACTOR_341300_QUADS_ACTIVE           = 0,
    ACTOR_341300_QUADS_BLINK            = 1,
    ACTOR_341300_QUADS_SKIPPED_FRAME    = 61,
    ACTOR_341300_QUADS_BLINK_FRAMES     = 30,
    ACTOR_341300_QUADS_BLINK_FIRST_GAP  = 10,
    ACTOR_341300_QUADS_BLINK_SECOND_GAP = 20
};

/// Shard states, world-unit motion/shape bounds and angle-unit spin bounds.
/// Gravity is world units per update squared; spin magnitude is 100..227.
enum {
    ACTOR_341300_SHARD_INIT              = 0,
    ACTOR_341300_SHARD_FALL              = 1,
    ACTOR_341300_SHARD_GRAVITY           = 8,
    ACTOR_341300_SHARD_VELOCITY_MASK     = 31,
    ACTOR_341300_SHARD_SPIN_MASK         = 127,
    ACTOR_341300_SHARD_MIN_SPIN          = 100,
    ACTOR_341300_SHARD_RADIUS            = 20,
    ACTOR_341300_SHARD_JITTER            = 2,
    ACTOR_341300_SHARD_DARK_RGB          = 16,
    ACTOR_341300_SHARD_MIDDLE_RGB        = 64,
    ACTOR_341300_SHARD_LIGHT_RGB         = 128,
    ACTOR_341300_SHARD_DEPTH_SHIFT       = 4,
    ACTOR_341300_EMITTER_2_SHARD_OT_SLOT = 1039
};

static void _actor341300StartDebrisEmitter(s16 emitterIndex);
static void _actor341300StopDebrisEmitter(s16 emitterIndex);

/// Descriptor indices, batch sizes and update delays of the debris emitters.
enum {
    ACTOR_341300_EMITTER_0_SHARD_TASK_INDEX = 1,
    ACTOR_341300_EMITTER_2_SHARD_TASK_INDEX = 3,
    ACTOR_341300_EMITTER_SHARD_BATCH_SIZE   = 10,
    ACTOR_341300_EMITTER_FIRST_DELAY        = 31,
    ACTOR_341300_EMITTER_LATER_DELAY        = 16
};

static void _actor341300Emitter0Task(Task* task);
static void _actor341300Emitter2Task(Task* task);
static void _actor341300PairedDebrisEmitterTask(Task* task);

static void _actor341300StageSceneAudioStart(void);
static void _actor341300EnqueueScenePlayback(void);
static void _actor341300FinishSceneStream(void);
static void _actor341300CancelScene(void);
static void _actor341300StartTexturedQuads(void);
static void _actor341300RequestTexturedQuadsExit(void);
static void _actor341300StartPlayerFacingTask(void);
static void _actor341300StartDebrisEmitterCallback(s16 emitterIndex);
static void _actor341300StopDebrisEmitterCallback(s16 emitterIndex);
static void _actor341300SpawnPlayerHitPuffs(void);
static void _actor341300SetSceneEvent(s8 sceneEvent);

static void _actor341300TurnPlayerTowardScenePlacementTask(Task* task);
static void _actor341300TexturedQuadsTask(Task* task);

#include "assets/rooms/shelter/b2_north_maintenance_walkway/stinger_scene_clips.inc"

/// Priority of the scene's temporary tasks and placed-resource exit recipe.
enum { ACTOR_341300_TASK_PRIORITY = 192 };

/// Scene-local script choices; music durations count audio updates.
enum {
    ACTOR_341300_SCENE_VIEW               = 4,
    ACTOR_341300_SCENE_CAP_COMMAND        = 2,
    ACTOR_341300_MUSIC_FADE_TICKS         = 60,
    ACTOR_341300_SCENE_ACTOR_DRAW_HIDE    = 0,
    ACTOR_341300_SCENE_ACTOR_DRAW_SHOW    = 1,
    ACTOR_341300_SCENE_ACTOR_EXIT_COMMAND = 1
};

TaskDesc gActor341300ScriptTaskDescriptors[3] = {
    { { { TASK_BODY_COORD, ACTOR_341300_TASK_PRIORITY } }, _actor341300TexturedQuadsTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_341300_TASK_PRIORITY } }, _actor341300TurnPlayerTowardScenePlacementTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_341300_TASK_PRIORITY } }, enemyTaskExit, { .value = 0 } },
};

/// Player weapon-bank extension: NULL at index 47, then scene sets at 48..52.
///
/// The six pointers are copied synchronously; their animation data remains
/// borrowed by playback, so this actor package must remain loaded while used.
#include "assets/rooms/shelter/b2_north_maintenance_walkway/player_stinger_scene_bank.inc"

/// Six-word copy request installing the scene sets in the equipped player bank.
///
/// Both script paths borrow this request through dispatch; copied set pointers
/// require the actor package to stay loaded through playback.
static AnimationBankCopyRequest _gActor341300PlayerAnimationExtensionCopy = {
    _gActor341300PlayerAnimationExtensionSets, ARRAY_SIZE(_gActor341300PlayerAnimationExtensionSets)
};

/// Unreferenced bank-1, animation-1 reset request with world collision enabled.
///
/// No dispatch path establishes its intended receiver.
static AnimationPlayRequest _gActor341300UnusedBank1Animation1Request = {
    .source = { .index = 1 }, .animationId = 1, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// First appended player set, played before resuming control on either scene path.
///
/// Weapon-animation dispatch replaces source.index with the equipped bank;
/// clip data is borrowed from this actor package through playback.
static AnimationPlayRequest _gActor341300PlayerSceneAnimation1Request = {
    .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 1, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// Second appended player set, reset before the scene's player-hit phase.
///
/// Weapon-animation dispatch replaces source.index with the equipped bank;
/// clip data is borrowed from this actor package through playback.
static AnimationPlayRequest _gActor341300PlayerSceneAnimation2Request = {
    .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 2, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// Third appended player set, reset after debris emitter 0 is stopped.
///
/// Weapon-animation dispatch replaces source.index with the equipped bank;
/// clip data is borrowed from this actor package through playback.
static AnimationPlayRequest _gActor341300PlayerSceneAnimation3Request = {
    .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 3, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// Fourth appended player set, reset while the scene's textured quads run.
///
/// Weapon-animation dispatch replaces source.index with the equipped bank;
/// clip data is borrowed from this actor package through playback.
static AnimationPlayRequest _gActor341300PlayerSceneAnimation4Request = {
    .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 4, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// Fifth appended player set, blended over ten frames during the opening turn.
///
/// Weapon-animation dispatch replaces source.index with the equipped bank;
/// clip data is borrowed from this actor package through playback.
static AnimationPlayRequest _gActor341300PlayerSceneAnimation5Request = {
    .source = { .index = 1 }, .animationId = ANIMATION_BANK_BASE_SET_COUNT + 5, .blend = ANIMATION_BLEND_INTERPOLATE, .blendFrames = 10, .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE
};

/// Player placement before the hit phase, in world units and 4096-unit-turn angles.
static ActorTransform _gActor341300PlayerScenePlacement = {
    .pos = { 2000, 0, 2250, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1, 0, 0 }
};

/// Common end placement for normal and skipped paths, in world/4096-turn units.
static ActorTransform _gActor341300PlayerEndPlacement = {
    .pos = { 2000, 0, 4000, 0 }, .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1, 0, 0 }
};

/// Scene Brain Stinger's opening event animation (event 0, package clip 19).
///
/// The receiver ignores bank and collision fields; the reset request is read
/// synchronously and does not retain this record.
static AnimationPlayRequest _gActor341300SceneActorAnimation0Request = {
    .source = { .index = 0 }, .animationId = 0, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Scene Brain Stinger's player-hit event animation (event 1, package clip 20).
///
/// The receiver ignores bank and collision fields; the reset request is read
/// synchronously and does not retain this record.
static AnimationPlayRequest _gActor341300SceneActorAnimation1Request = {
    .source = { .index = 0 }, .animationId = 1, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Scene Brain Stinger's debris-phase event animation (event 2, package clip 21).
///
/// The receiver ignores bank and collision fields; the reset request is read
/// synchronously and does not retain this record.
static AnimationPlayRequest _gActor341300SceneActorAnimation2Request = {
    .source = { .index = 0 }, .animationId = 2, .blend = ANIMATION_BLEND_RESET, .blendFrames = 0, .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE
};

/// Fixed scene-actor placement, also supplying the player's facing target X/Z.
///
/// Position is world units; rotation uses 4096 units per turn. The placement
/// message reads the complete record synchronously; the facing task borrows pos.
static ActorTransform _gActor341300SceneActorPlacement = {
    .pos = { 2090, -3480, 1440, 0 }, .rot = { 0, 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1, 0 }
};

/// Removes the scene Brain Stinger on either completion path.
///
/// The receiver removes itself for every nonzero command and ignores context;
/// this record supplies the walkway's stage/area namespace and is read at dispatch.
static ActorCommand _gActor341300SceneActorExitCommand = {
    .context = { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY } },
    .command = ACTOR_341300_SCENE_ACTOR_EXIT_COMMAND
};

/// Scene/audio key for group 4, stream 13 and qualifier 11 (second qualifier zero).
///
/// Event playback retains the key until replacement or script end.
static EvsSceneKey _gActor341300SceneStreamKey = { .group = 4, .streamId = 13, .subId = 11 };

EvsCommand gActor341300WalkwaySceneScript[] = {
    // Install player scene sets and stage the actor before CAP-controlled phases.
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = ACTOR_341300_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor341300PlayerAnimationExtensionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_341300_SCENE_ACTOR_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_341300_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor341300SceneActorPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor341300SceneActorAnimation0Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor341300PlayerSceneAnimation5Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300StartPlayerFacingTask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StartDebrisEmitterCallback }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, 6) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_341300_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Select the stream after the opening CAP cues, then stage the hit phase.
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor341300SceneStreamKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor341300PlayerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor341300PlayerSceneAnimation2Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor341300SceneActorAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300SpawnPlayerHitPuffs }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor341300SceneActorAnimation2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StartDebrisEmitterCallback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor341300PlayerSceneAnimation3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_341300_SCENE_ACTOR_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StartDebrisEmitterCallback }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300StartTexturedQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor341300PlayerSceneAnimation4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300RequestTexturedQuadsExit }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Fade out the staged scene before restoring player placement and control.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor341300PlayerSceneAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor341300PlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor341300SceneActorExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300FinishSceneStream }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACTOR_341300_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor341300SetSceneEvent }, { .value = ACTOR_341300_SCENE_EVENT_AFTER_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor341300WalkwaySceneSkipScript[] = {
    // Hide cleanup under a fade and restore the common player end state.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor341300PlayerEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor341300PlayerAnimationExtensionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor341300PlayerSceneAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor341300SceneActorExitCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor341300StopDebrisEmitterCallback }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300RequestTexturedQuadsExit }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Cancel streaming, apply the end view/event and release the event presentation.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor341300CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = ACTOR_341300_SCENE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACTOR_341300_MUSIC_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor341300SetSceneEvent }, { .value = ACTOR_341300_SCENE_EVENT_AFTER_SCRIPT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Saved debris-emitter handle storage; only entry 0 has an established role.
///
/// Start overwrites entry 0 without stopping an earlier emitter. Stop requires
/// NULL or a live task, tears down its child shards, and clears the entry.
/// The other two words are never accessed; their roles and index mapping are unproven.
static Task* _gActor341300DebrisEmitterHandles[3] = { NULL };

/// World-space shard origins selected by emitter 0's unchecked spawnArg1 index.
///
/// Valid indices are 0..3. The package emitters use 0, 1 and 3; initialized
/// index 2 has no spawn use. XYZ become a view-parented coordinate's translation.
static SVECTOR _gActor341300Emitter0ShardSpawnPositions[4] = {
    { 2090, -2500, 1440, 0 },
    { 2390, -2500, 1140, 0 },
    { 1900, -2500, 1100, 0 },
    { 1900, -2500, 1000, 0 },
};

/// World-space shard origins for the unstarted descriptor-slot-2 emitter.
///
/// The shard's unchecked spawnArg1 index must be 0 or 1; it also selects the
/// initial X-velocity sign. The package has no start path for this emitter.
static SVECTOR _gActor341300Emitter2ShardSpawnPositions[2] = {
    { 1800, -2300, 900, 0 },
    { 2450, -2300, 900, 0 },
};

static void _actor341300Emitter0ShardTask(Task* task);
static void _actor341300Emitter2ShardTask(Task* task);

/// Debris task recipes: emitter/shard pairs at indices 0/1 and 2/3.
///
/// Only emitter 0 is started by the package. Shard index 1 takes a position
/// index 0..3; shard index 3 takes 0..1. Both borrow their emitter in spawnArg2
/// until their first update joins its teardown tree. Emitters have no body;
/// shards own coordinate bodies. All callbacks require this package loaded.
static TaskDesc _gActor341300DebrisTaskDescriptors[4] = {
    { { { TASK_BODY_NONE, ACTOR_341300_TASK_PRIORITY } }, _actor341300Emitter0Task, { .value = 0 } },
    { { { TASK_BODY_COORD, ACTOR_341300_TASK_PRIORITY } }, _actor341300Emitter0ShardTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_341300_TASK_PRIORITY } }, _actor341300Emitter2Task, { .value = 0 } },
    { { { TASK_BODY_COORD, ACTOR_341300_TASK_PRIORITY } }, _actor341300Emitter2ShardTask, { .value = 0 } },
};

/// Unspawned bodyless emitter recipe for two delayed ten-pair shard bursts.
///
/// Its callback uses shard descriptor 1 at positions 0/1. No package spawn
/// path reaches this recipe; ignored script indices do not establish a mapping.
static TaskDesc _gActor341300PairedDebrisEmitterTaskDescriptor = { { { TASK_BODY_NONE, ACTOR_341300_TASK_PRIORITY } }, _actor341300PairedDebrisEmitterTask, { .value = 0 } };

/// Borrowed task handle shared by the player-facing and textured-quad script phases.
///
/// Spawning replaces it without stopping the previous task. Reset only forgets
/// it; facing-task teardown does not clear it. Exit requests require NULL or a
/// still-live task, then clear the slot. This handle does not extend task lifetime.
static Task* _gActor341300ScriptTaskHandle = NULL;

/// Initializes a shard's POLY_G3 packet with projected corners and a grey ramp.
///
/// triangle points to a writable packet, and screenX/screenY each provide three
/// s16 pixel coordinates. Arguments are evaluated repeatedly and must have no
/// side effects. Sets the packet kind and all three vertex colours and positions;
/// the caller allocates it and links it into the ordering table.
/// Statement-only GNU expression; its value is not part of the interface.
#define ACTOR_341300_FILL_SHARD_TRIANGLE(triangle, screenX, screenY)                      \
    ({                                                                                    \
        setPolyG3(triangle);                                                              \
        (triangle)->r0 = (triangle)->g0 = (triangle)->b0 = ACTOR_341300_SHARD_DARK_RGB;   \
        (triangle)->r1 = (triangle)->g1 = (triangle)->b1 = ACTOR_341300_SHARD_MIDDLE_RGB; \
        (triangle)->r2 = (triangle)->g2 = (triangle)->b2 = ACTOR_341300_SHARD_LIGHT_RGB;  \
        (triangle)->x0                                   = (screenX)[0];                  \
        (triangle)->y0                                   = (screenY)[0];                  \
        (triangle)->x1                                   = (screenX)[1];                  \
        (triangle)->y1                                   = (screenY)[1];                  \
        (triangle)->x2                                   = (screenX)[2];                  \
        (triangle)->y2                                   = (screenY)[2];                  \
    })

/// Advances a shard's halfword tumble angles and rebuilds its Y-X-Z rotation.
///
/// shardCoord and work are stable pointers to the live GfxCoord and shard work,
/// evaluated repeatedly. Spin counts angle units per update, with 4096 per turn;
/// each stored angle narrows to s16. Replaces the basis while preserving translation
/// and invalidates composition. The callers apply this after drawing the current pose.
/// Statement-only GNU expression; its value is not part of the interface.
#define ACTOR_341300_ADVANCE_SHARD_TUMBLE(shardCoord, work)                             \
    ({                                                                                  \
        (work)->rot.vx += (work)->spin.vx;                                              \
        (work)->rot.vy += (work)->spin.vy;                                              \
        (work)->rot.vz += (work)->spin.vz;                                              \
        gfxRotMatrixY(&(shardCoord)->coord, (work)->rot.vy, GRAPHICS_ROTATION_REPLACE); \
        gfxRotMatrixX(&(shardCoord)->coord, (work)->rot.vx, GRAPHICS_ROTATION_COMPOSE); \
        gfxRotMatrixZ(&(shardCoord)->coord, (work)->rot.vz, GRAPHICS_ROTATION_COMPOSE); \
        (shardCoord)->composeStamp = GRAPHICS_COORD_DIRTY;                              \
    })

/// Draws two fixed world-space quads with an unmodulated 8-bit texture.
///
/// Requires a composed view coordinate, space for two POLY_FT4 packets, and
/// the texture at VRAM (448, 256) with its CLUT at (0, 248). Quads with a
/// negative projection flag are skipped; accepted depths use the display's
/// ordering-table scaling. Screen XY stays packed in SDK `long` words.
static void _actor341300DrawTexturedQuads(void)
{
    enum {
        ACTOR_341300_QUAD_VERTEX_COUNT = 4,
        ACTOR_341300_QUAD_U_MIN        = 35,
        ACTOR_341300_QUAD_U_MAX        = 47,
        ACTOR_341300_QUAD_V_MIN        = 209,
        ACTOR_341300_QUAD_V_MAX        = 221,
        ACTOR_341300_QUAD_NEUTRAL_RGB  = 128
    };

    s16     screenX[8];
    s16     screenY[8];
    long    packedScreenXY[8];
    s32     quadDepth[2];
    SVECTOR corners[8] = {
        { 0x80C, -0xBC6, 0x150 },
        { 0x83C, -0xBC6, 0x150 },
        { 0x80C, -0xBC6, 0x180 },
        { 0x83C, -0xBC6, 0x180 },
        { 0x747, -0xBC6, 0x150 },
        { 0x777, -0xBC6, 0x150 },
        { 0x747, -0xBC6, 0x180 },
        { 0x777, -0xBC6, 0x180 },
    };
    long      perspective;
    long      projectionFlags;
    s32       quadIndex;
    s32       cornerIndex;
    long      firstCorner;
    POLY_FT4* quad;

    // Project both world-space quads using the current view.
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    for (quadIndex = 0; quadIndex < (s32)ARRAY_SIZE(quadDepth); quadIndex++) {
        firstCorner = quadIndex * ACTOR_341300_QUAD_VERTEX_COUNT;
        RotTransPers(&corners[firstCorner + 3], &packedScreenXY[firstCorner + 3], &perspective, &projectionFlags);
        quadDepth[quadIndex] = RotTransPers3(&corners[firstCorner], &corners[firstCorner + 1], &corners[firstCorner + 2], &packedScreenXY[firstCorner], &packedScreenXY[firstCorner + 1], &packedScreenXY[firstCorner + 2], &perspective, &projectionFlags);
        if (projectionFlags >= 0) {
            for (cornerIndex = firstCorner; cornerIndex < firstCorner + ACTOR_341300_QUAD_VERTEX_COUNT; cornerIndex++) {
                screenX[cornerIndex] = packedScreenXY[cornerIndex];
                screenY[cornerIndex] = packedScreenXY[cornerIndex] >> 16;
            }
            // Raw texture mode bypasses RGB modulation; the packet still stores neutral RGB.
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            setShadeTex(quad, 1);
            quad->x0 = screenX[firstCorner];
            quad->y0 = screenY[firstCorner];
            quad->x1 = screenX[firstCorner + 1];
            quad->y1 = screenY[firstCorner + 1];
            quad->x2 = screenX[firstCorner + 2];
            quad->y2 = screenY[firstCorner + 2];
            quad->x3 = screenX[firstCorner + 3];
            quad->y3 = screenY[firstCorner + 3];
            setUV4(quad, ACTOR_341300_QUAD_U_MIN, ACTOR_341300_QUAD_V_MIN, ACTOR_341300_QUAD_U_MAX, ACTOR_341300_QUAD_V_MIN,
                   ACTOR_341300_QUAD_U_MIN, ACTOR_341300_QUAD_V_MAX, ACTOR_341300_QUAD_U_MAX, ACTOR_341300_QUAD_V_MAX);
            setRGB0(quad, ACTOR_341300_QUAD_NEUTRAL_RGB, ACTOR_341300_QUAD_NEUTRAL_RGB, ACTOR_341300_QUAD_NEUTRAL_RGB);
            quad->clut  = getClut(0, 248);
            quad->tpage = getTPage(1, 0, 448, 256);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(quadDepth[quadIndex] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
        }
    }
}

/// Turns the player toward the scene's fixed placement while an event is active.
///
/// Steps yaw by 128 angle units per update (4096 per turn), taking the shorter
/// turn and snapping when at most one step remains. Ends on arrival, an idle
/// event, or a missing area-placement actor; that actor only gates the turn.
/// Requires a live player with `GameActor` work and a model root coordinate.
/// Uses the placement's x/z and the player's stored root translation.
static void _actor341300TurnPlayerTowardScenePlacementTask(Task* task)
{
    enum { ACTOR_341300_PLAYER_YAW_STEP = 128 };

    Task*      playerTask;
    GameActor* playerActor;
    Enemy*     areaActor;
    GfxCoord*  playerCoord;
    VECTOR*    targetPosition;
    s32        targetYaw;
    s32        yawDelta;
    s32        absoluteYawDelta;
    s32        yawStep;
    s32        wrappedYawDelta;

    /// Wraps a signed yaw difference by at most one turn, retaining half-turn ties.
    ///
    /// Arguments must be distinct writable s32 locals without side effects,
    /// evaluated repeatedly. magnitude receives ABS of the original delta;
    /// wrapped is written only when that magnitude exceeds 2048. The corrected
    /// delta subtracts or adds one 4096-unit turn; this is not general modulo reduction.
#define ACTOR_341300_UNWRAP_YAW_DELTA(delta, magnitude, wrapped)  \
    do {                                                          \
        (magnitude) = ABS(delta);                                 \
        if ((magnitude) >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) { \
            (wrapped) = (delta) - ACTOR_TRANSFORM_ANGLE_TURN;     \
            if ((delta) < 0) {                                    \
                (wrapped) = (delta) + ACTOR_TRANSFORM_ANGLE_TURN; \
            }                                                     \
            (delta) = (wrapped);                                  \
        }                                                         \
    } while (0)

    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerActor = playerTask->work;
    areaActor   = sceneFindEnemyByPlaceKey(gGameSession->location.area | (gGameSession->location.stage << 8));
    if ((areaActor != NULL) && (gGameSession->eventState != 0)) {
        playerCoord    = playerTask->extra.tmd->coords;
        targetPosition = &_gActor341300SceneActorPlacement.pos;
        targetYaw      = ratan2(targetPosition->vx - playerCoord->coord.t[0], targetPosition->vz - playerCoord->coord.t[2]);
        yawDelta       = targetYaw - playerActor->rotation.vy;
        ACTOR_341300_UNWRAP_YAW_DELTA(yawDelta, absoluteYawDelta, wrappedYawDelta);
#undef ACTOR_341300_UNWRAP_YAW_DELTA
        absoluteYawDelta = ABS(yawDelta);
        if (absoluteYawDelta >= ACTOR_341300_PLAYER_YAW_STEP + 1) {
            yawStep = ACTOR_341300_PLAYER_YAW_STEP;
            if (yawDelta < 0) {
                yawStep = -ACTOR_341300_PLAYER_YAW_STEP;
            }
            // Preserve the stored yaw's 16-bit wrap.
            playerActor->rotation.vy = (s16)((u16)playerActor->rotation.vy + yawStep);
            return;
        }
        playerActor->rotation.vy = targetYaw;
    }
    taskKill(task);
}

/// Event callback that stages an audio-start request for the selected scene.
///
/// Requires a selected scene descriptor. An audio-free scene queues no request.
static void _actor341300StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Event callback that queues selected-scene audio playback or marks an audio-free scene playing.
static void _actor341300EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Event callback that ends the selected scene stream and restores its saved RNG state.
///
/// Requires a prior scene selection; stream buffers and tasks remain owned by
/// their existing callers.
static void _actor341300FinishSceneStream(void)
{
    streamFinishScene();
}

/// Cancels and finishes the selected scene when the event script is skipped.
///
/// Discards its deferred CD request, requests queue cancellation and finishes
/// the scene stream, restoring the RNG state saved at scene selection.
static void _actor341300CancelScene(void)
{
    cdCmdCancelScene();
}

/// Starts the textured-quad task and stores its handle in the script's shared task slot.
///
/// Replaces the saved handle without killing a previously stored task.
static void _actor341300StartTexturedQuads(void)
{
    enum { ACTOR_341300_QUADS_TASK_INDEX = 0 };

    _gActor341300ScriptTaskHandle = taskSpawnFromTable(gActor341300ScriptTaskDescriptors, ACTOR_341300_QUADS_TASK_INDEX, 0, NULL);
}

/// Requests immediate exit of the saved quad task and forgets its handle.
///
/// Sets state -1 and clears its counter; teardown occurs on its next update.
/// The shared slot may be NULL and must otherwise hold a live task.
static void _actor341300RequestTexturedQuadsExit(void)
{
    if (_gActor341300ScriptTaskHandle != NULL) {
        _gActor341300ScriptTaskHandle->state         = ACTOR_341300_QUADS_EXIT;
        _gActor341300ScriptTaskHandle->killCountdown = 0;
        _gActor341300ScriptTaskHandle                = NULL;
    }
}

/// Draws the fixed quads in the task's active or blinking phase.
///
/// State 0 draws except at counter 61; it does not transition or saturate the
/// signed-halfword counter. State 1 draws for counter values below 30 except
/// 10 and 20, then releases the task. Other states, including -1, exit at once.
/// This package starts state 0 and requests -1; it never selects state 1.
static void _actor341300TexturedQuadsTask(Task* task)
{
    s16 nextFrame;
    s16 frame;

    switch (task->state) {
        case ACTOR_341300_QUADS_ACTIVE:
            // Increment with halfword wrap; only count 61 suppresses this frame.
            nextFrame           = (u16)task->killCountdown + 1;
            task->killCountdown = nextFrame;
            if (nextFrame != ACTOR_341300_QUADS_SKIPPED_FRAME) {
                _actor341300DrawTexturedQuads();
                return;
            }
            return;
        case ACTOR_341300_QUADS_BLINK:
            frame = task->killCountdown;
            if (frame < ACTOR_341300_QUADS_BLINK_FRAMES) {
                if ((frame != ACTOR_341300_QUADS_BLINK_FIRST_GAP) && (frame != ACTOR_341300_QUADS_BLINK_SECOND_GAP)) {
                    _actor341300DrawTexturedQuads();
                }
                task->killCountdown = (u16)task->killCountdown + 1;
                return;
            }
        default:
            taskKill(task);
            break;
    }
}

/// Starts the player-facing task and stores its handle in the script's shared task slot.
///
/// Replaces the saved handle without killing a previously stored task.
static void _actor341300StartPlayerFacingTask(void)
{
    enum { ACTOR_341300_PLAYER_FACING_TASK_INDEX = 1 };

    _gActor341300ScriptTaskHandle = taskSpawnFromTable(gActor341300ScriptTaskDescriptors, ACTOR_341300_PLAYER_FACING_TASK_INDEX, 0, NULL);
}

/// Event callback that starts debris emitter 0; other signed-halfword indices are ignored.
static void _actor341300StartDebrisEmitterCallback(s16 emitterIndex)
{
    _actor341300StartDebrisEmitter(emitterIndex);
}

/// Event callback that releases debris emitter 0 and its child shards; other indices are ignored.
static void _actor341300StopDebrisEmitterCallback(s16 emitterIndex)
{
    _actor341300StopDebrisEmitter(emitterIndex);
}

/// Part-local offset (100, -200, -100) sampled for the scene's four player hit puffs.
///
/// Relative to player model part 2; copied to stack storage used only during spawn.
static const SVECTOR _gActor341300PlayerHitPuffOffset = { 100, -200, -100, 0 };

/// Emits the scene's four hit puffs from the player's model part 2.
///
/// Requires a live player TMD task and part 2's initialized coordinate chain.
/// All four sample offset (100, -200, -100) in part-local units during spawn.
/// The first uses size 768 and a three-tick texture period; the other three
/// use size 640, a two-tick period and random initial velocity. All select the
/// alternate palette and additive blend. The puff callback does not follow the
/// retained stack offset; allocation failures are ignored and no handle is saved.
static void _actor341300SpawnPlayerHitPuffs(void)
{
    enum { ACTOR_341300_HIT_PART            = 2,
           ACTOR_341300_HIT_PALETTE         = 0x10000000,
           ACTOR_341300_HIT_RANDOM_VELOCITY = 0x100000,
           ACTOR_341300_HIT_PERIOD_SHIFT    = 12,
           ACTOR_341300_HIT_BLEND_SHIFT     = 16 };
    enum {
        ACTOR_341300_MAIN_HIT_PUFF_ARG  = (ACTOR_341300_HIT_PALETTE | (GPU_BLEND_ADD << ACTOR_341300_HIT_BLEND_SHIFT) | (3 << ACTOR_341300_HIT_PERIOD_SHIFT) | 768),
        ACTOR_341300_SMALL_HIT_PUFF_ARG = (ACTOR_341300_HIT_PALETTE | ACTOR_341300_HIT_RANDOM_VELOCITY | (GPU_BLEND_ADD << ACTOR_341300_HIT_BLEND_SHIFT) | (2 << ACTOR_341300_HIT_PERIOD_SHIFT) | 640)
    };
    SVECTOR   hitOffset       = _gActor341300PlayerHitPuffOffset;
    GfxCoord* playerPartCoord = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[ACTOR_341300_HIT_PART];

    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_MAIN_HIT_PUFF_ARG, &hitOffset);
    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_SMALL_HIT_PUFF_ARG, &hitOffset);
    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_SMALL_HIT_PUFF_ARG, &hitOffset);
    effectSpawn(EFFECT_HIT_PUFF, playerPartCoord, ACTOR_341300_SMALL_HIT_PUFF_ARG, &hitOffset);
}

/// Stores the event script's signed-byte scene event in the live save's music-selection state.
static void _actor341300SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

void actor341300ResetScriptTaskHandle(void)
{
    _gActor341300ScriptTaskHandle = NULL;
}

/// Attempts ten falling debris shards at one of emitter 0's four positions.
///
/// `spawnIndex` must be 0..3 and is forwarded unchanged in spawnArg1. Requires
/// the loaded shard descriptor and position table. Each successful child borrows
/// `emitter` through its first update, then joins its teardown tree; the emitter
/// must stay live until then. Failed spawns still count toward the ten attempts.
static inline void _actor341300SpawnShardBatch(Task* emitter, s32 spawnIndex)
{
    s16 shardIndex;

    for (shardIndex = 0; shardIndex < ACTOR_341300_EMITTER_SHARD_BATCH_SIZE; shardIndex++) {
        taskSpawnFromTable(_gActor341300DebrisTaskDescriptors, ACTOR_341300_EMITTER_0_SHARD_TASK_INDEX, spawnIndex, emitter);
    }
}

/// Spawns ten interleaved pairs of slot-1 shards, preserving position order.
///
/// Both position indices must fit the four-position table. The emitter must
/// outlive each child's first update, which establishes its teardown parent.
/// Sends each signed index unchanged in spawnArg1 and the borrowed emitter in
/// spawnArg2. The s16 loop attempts ten first/second pairs, including failed
/// spawns; allocation failures are ignored independently for each shard.
static inline void _actor341300SpawnShardPairs(Task* emitter, s32 firstSpawnIndex, s32 secondSpawnIndex)
{
    s16 pairIndex;

    for (pairIndex = 0; pairIndex < ACTOR_341300_EMITTER_SHARD_BATCH_SIZE; pairIndex++) {
        taskSpawnFromTable(_gActor341300DebrisTaskDescriptors, ACTOR_341300_EMITTER_0_SHARD_TASK_INDEX, firstSpawnIndex, emitter);
        taskSpawnFromTable(_gActor341300DebrisTaskDescriptors, ACTOR_341300_EMITTER_0_SHARD_TASK_INDEX, secondSpawnIndex, emitter);
    }
}

/// Emits the scripted debris sequence from descriptor slot 0, then stays idle.
///
/// Requires a live bodyless task, the loaded descriptor/position tables and an
/// initial state of 0. Emits ten shards immediately at position 0, ten there
/// after 31 updates, ten at position 1 after 16 more, then two twenty-shard
/// bursts at positions 3/1 spaced 16 updates apart. State 5 waits for the script
/// to tear down the emitter and its children. Failed spawns are ignored.
static void _actor341300Emitter0Task(Task* task)
{
    enum {
        ACTOR_341300_EMITTER_0_START        = 0,
        ACTOR_341300_EMITTER_0_REPEAT_FIRST = 1,
        ACTOR_341300_EMITTER_0_SECOND       = 2,
        ACTOR_341300_EMITTER_0_FIRST_PAIR   = 3,
        ACTOR_341300_EMITTER_0_SECOND_PAIR  = 4,
        ACTOR_341300_EMITTER_0_DONE         = 5
    };

    switch (task->state) {
        case ACTOR_341300_EMITTER_0_START:
            _actor341300SpawnShardBatch(task, 0);
            task->killCountdown = 0;
            task->state         = task->state + 1;
            break;
        case ACTOR_341300_EMITTER_0_REPEAT_FIRST:
            task->killCountdown++;
            if (task->killCountdown >= ACTOR_341300_EMITTER_FIRST_DELAY) {
                _actor341300SpawnShardBatch(task, 0);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;
        case ACTOR_341300_EMITTER_0_SECOND:
            task->killCountdown++;
            if (task->killCountdown >= ACTOR_341300_EMITTER_LATER_DELAY) {
                _actor341300SpawnShardBatch(task, 1);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;
        case ACTOR_341300_EMITTER_0_FIRST_PAIR:
        case ACTOR_341300_EMITTER_0_SECOND_PAIR:
            task->killCountdown++;
            if (task->killCountdown >= ACTOR_341300_EMITTER_LATER_DELAY) {
                _actor341300SpawnShardPairs(task, 3, 1);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;
        case ACTOR_341300_EMITTER_0_DONE:
            break;
    }
}

/// Updates one falling, tumbling debris triangle from emitter 0.
///
/// Requires a coordinate-body task, a live emitter in `spawnArg2.pointer`, and
/// a spawn-position index in `spawnArg1.value` (the emitter uses 0, 1 and 3;
/// table index 2 is also valid). Owns its allocated work until task teardown.
/// Uses world units for translation and 4096 angle units per turn. Positive
/// Y is downward; releases the shard on an update starting below world Y zero.
/// Needs one POLY_G3 packet per drawn update and the normal frame ordering
/// table. The third vertex's unsigned SZ3 depth divided by 64 selects tag 0..1023.
static void _actor341300Emitter0ShardTask(Task* task)
{
    _Actor341300ShardWork* work;
    GfxCoord*              coord;
    POLY_G3*               triangle;
    s16                    screenX[3];
    s16                    screenY[3];
    s32                    packedScreenXY;
    s32                    quarterDepth;
    s16                    vertexIndex;
    s32                    rightCornerX;
    s32                    rightCornerY;
    s32                    leftCornerX;
    s32                    leftCornerY;

    work  = task->work;
    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACTOR_341300_SHARD_INIT:
            // Own the work block and attach this shard to its emitter's teardown tree.
            task->work = memCalloc(sizeof(*work), false);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            work          = task->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, sizeof(*work));
            taskReparent(task->spawnArg2.pointer, task);
            coord->coord.t[0] = _gActor341300Emitter0ShardSpawnPositions[task->spawnArg1.value].vx;
            coord->coord.t[1] = _gActor341300Emitter0ShardSpawnPositions[task->spawnArg1.value].vy;
            coord->coord.t[2] = _gActor341300Emitter0ShardSpawnPositions[task->spawnArg1.value].vz;
            work->vel.vx      = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->vel.vy      = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->vel.vz      = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->spin.vx     = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vy     = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vz     = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK);
            if (work->spin.vx > 0) {
                work->spin.vx += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vx -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vy > 0) {
                work->spin.vy += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vy -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vz > 0) {
                work->spin.vz += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vz -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            // Form a radius-20 triangle, jittering each nonzero coordinate outward.
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? ACTOR_341300_SHARD_RADIUS + ACTOR_341300_SHARD_JITTER : ACTOR_341300_SHARD_RADIUS;
            work->verts[0].vz = 0;
            rightCornerX      = rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE;
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                rightCornerX += ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vx = rightCornerX;
            rightCornerY      = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                rightCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vy = rightCornerY;
            work->verts[1].vz = 0;
            leftCornerX       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                leftCornerX -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vx = leftCornerX;
            leftCornerY       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                leftCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vy = leftCornerY;
            work->verts[2].vz = 0;
            task->state++;
            break;
        case ACTOR_341300_SHARD_FALL:
            if (coord->coord.t[1] > 0) {
                taskKill(task);
                break;
            }
            // Advance in world space, then project through the view parent.
            work->vel.vy      += ACTOR_341300_SHARD_GRAVITY;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            actorRenderComposeCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (vertexIndex = 0; vertexIndex < (s32)ARRAY_SIZE(work->verts); vertexIndex++) {
                gte_ldv0(&work->verts[vertexIndex]);
                gte_rtps();
                gte_stsxy(&packedScreenXY);
                gte_stszotz(&quarterDepth);
                screenX[vertexIndex] = packedScreenXY;
                screenY[vertexIndex] = packedScreenXY >> 16;
            }
            triangle       = gGpuPrimCursor;
            gGpuPrimCursor = triangle + 1;
            ACTOR_341300_FILL_SHARD_TRIANGLE(triangle, screenX, screenY);
            addPrim(&gGpuCurrentOt[quarterDepth >> ACTOR_341300_SHARD_DEPTH_SHIFT], triangle);
            // Build the next frame's Y-X-Z tumble after drawing the current rotation.
            ACTOR_341300_ADVANCE_SHARD_TUMBLE(coord, work);
            break;
    }
}

/// Attempts three emitter-2 debris shards in position order 0, middle, 1.
///
/// `middleSpawnIndex` must be 0 or 1, selecting which of the two loaded positions
/// repeats. Requires the slot-3 shard descriptor and its two-position table.
/// Successful children borrow `emitter` until their first update attaches them
/// to its teardown tree; it must stay live until then. Failures are ignored
/// independently, preserving all three spawn attempts and their order.
static inline void _actor341300SpawnEmitter2Burst(Task* emitter, s32 middleSpawnIndex)
{
    taskSpawnFromTable(_gActor341300DebrisTaskDescriptors, ACTOR_341300_EMITTER_2_SHARD_TASK_INDEX, 0, emitter);
    taskSpawnFromTable(_gActor341300DebrisTaskDescriptors, ACTOR_341300_EMITTER_2_SHARD_TASK_INDEX, middleSpawnIndex, emitter);
    taskSpawnFromTable(_gActor341300DebrisTaskDescriptors, ACTOR_341300_EMITTER_2_SHARD_TASK_INDEX, 1, emitter);
}

/// Emits alternating three-shard bursts from descriptor slot 2, then stays idle.
///
/// No package spawn path starts this task. Starting in state 0 clears the
/// signed-halfword counter; updates 3, 6, ... 30 emit at positions 0/1/1 then
/// 0/0/1 in alternation. Update 31 selects terminal state 3 without releasing
/// the task. Counts wrap to 16 bits before signed modulo/comparison. Requires
/// loaded slot-3 shard resources and a live task until its children attach.
static void _actor341300Emitter2Task(Task* task)
{
    enum {
        ACTOR_341300_EMITTER_2_START         = 0,
        ACTOR_341300_EMITTER_2_REPEAT_SECOND = 1,
        ACTOR_341300_EMITTER_2_REPEAT_FIRST  = 2,
        ACTOR_341300_EMITTER_2_DONE          = 3,
        ACTOR_341300_EMITTER_2_BURST_PERIOD  = 3,
        ACTOR_341300_EMITTER_2_STOP_FRAME    = 31
    };
    u16 elapsedFrames;

    switch (task->state) {
        case ACTOR_341300_EMITTER_2_START:
            task->killCountdown = 0;
            task->state         = task->state + 1;
            break;
        case ACTOR_341300_EMITTER_2_REPEAT_SECOND:
            elapsedFrames       = (u16)task->killCountdown + 1;
            task->killCountdown = elapsedFrames;
            if ((s16)elapsedFrames % ACTOR_341300_EMITTER_2_BURST_PERIOD == 0) {
                _actor341300SpawnEmitter2Burst(task, 1);
                task->state = task->state + 1;
            }
            break;
        case ACTOR_341300_EMITTER_2_REPEAT_FIRST:
            elapsedFrames       = (u16)task->killCountdown + 1;
            task->killCountdown = elapsedFrames;
            if ((s16)elapsedFrames % ACTOR_341300_EMITTER_2_BURST_PERIOD == 0) {
                _actor341300SpawnEmitter2Burst(task, 0);
                task->state = task->state - 1;
            }
            break;
    }
    // Keep the terminal test after every state, including initialization and idle.
    if (task->killCountdown >= ACTOR_341300_EMITTER_2_STOP_FRAME) {
        task->state = ACTOR_341300_EMITTER_2_DONE;
    }
}

/// Updates one falling debris triangle for the unstarted emitter in table slot 2.
///
/// Requires a coordinate-body task, a live emitter in `spawnArg2.pointer`, and
/// `spawnArg1.value` 0 or 1. X velocity is positive at spawn 0 and negative at
/// spawn 1. Owns its work until teardown; translation uses world units and
/// spin uses 4096 angle units per turn. Releases after falling below Y zero.
/// Needs one POLY_G3 packet per drawn update and the normal frame ordering
/// table, including its reserved background entry 1039.
/// The package has a spawning callback for these shards but never starts it.
static void _actor341300Emitter2ShardTask(Task* task)
{
    _Actor341300ShardWork* work;
    GfxCoord*              coord;
    POLY_G3*               triangle;
    s16                    screenX[3];
    s16                    screenY[3];
    s32                    packedScreenXY;
    s32                    quarterDepth;
    s16                    vertexIndex;
    s32                    rightCornerX;
    s32                    rightCornerY;
    s32                    leftCornerX;
    s32                    leftCornerY;

    work  = task->work;
    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACTOR_341300_SHARD_INIT:
            // Own the work block and attach this shard to its emitter's teardown tree.
            task->work = memCalloc(sizeof(*work), false);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            work          = task->work;
            coord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, sizeof(*work));
            taskReparent(task->spawnArg2.pointer, task);
            coord->coord.t[0] = _gActor341300Emitter2ShardSpawnPositions[task->spawnArg1.value].vx;
            coord->coord.t[1] = _gActor341300Emitter2ShardSpawnPositions[task->spawnArg1.value].vy;
            coord->coord.t[2] = _gActor341300Emitter2ShardSpawnPositions[task->spawnArg1.value].vz;
            if (task->spawnArg1.value == 0) {
                work->vel.vx = ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK;
            } else {
                work->vel.vx = -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK);
            }
            work->vel.vy  = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->vel.vz  = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_VELOCITY_MASK);
            work->spin.vx = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vy = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK);
            work->spin.vz = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? (ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK) : -(ACTOR_341300_DRAW_RANDOM_U16() & ACTOR_341300_SHARD_SPIN_MASK);
            if (work->spin.vx > 0) {
                work->spin.vx += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vx -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vy > 0) {
                work->spin.vy += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vy -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            if (work->spin.vz > 0) {
                work->spin.vz += ACTOR_341300_SHARD_MIN_SPIN;
            } else {
                work->spin.vz -= ACTOR_341300_SHARD_MIN_SPIN;
            }
            // Form a radius-20 triangle, jittering each nonzero coordinate outward.
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_DRAW_RANDOM_U16() & 1) ? ACTOR_341300_SHARD_RADIUS + ACTOR_341300_SHARD_JITTER : ACTOR_341300_SHARD_RADIUS;
            work->verts[0].vz = 0;
            rightCornerX      = rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE;
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                rightCornerX += ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vx = rightCornerX;
            rightCornerY      = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                rightCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[1].vy = rightCornerY;
            work->verts[1].vz = 0;
            leftCornerX       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 6) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                leftCornerX -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vx = leftCornerX;
            leftCornerY       = -(rsin(ACTOR_TRANSFORM_ANGLE_TURN / 12) * ACTOR_341300_SHARD_RADIUS / ONE);
            if (ACTOR_341300_DRAW_RANDOM_U16() & 1) {
                leftCornerY -= ACTOR_341300_SHARD_JITTER;
            }
            work->verts[2].vy = leftCornerY;
            work->verts[2].vz = 0;
            task->state++;
            break;
        case ACTOR_341300_SHARD_FALL:
            if (coord->coord.t[1] > 0) {
                taskKill(task);
                break;
            }
            // Advance in world space, then project through the view parent.
            work->vel.vy      += ACTOR_341300_SHARD_GRAVITY;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            actorRenderComposeCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (vertexIndex = 0; vertexIndex < (s32)ARRAY_SIZE(work->verts); vertexIndex++) {
                gte_ldv0(&work->verts[vertexIndex]);
                gte_rtps();
                gte_stsxy(&packedScreenXY);
                gte_stszotz(&quarterDepth);
                screenX[vertexIndex] = packedScreenXY;
                screenY[vertexIndex] = packedScreenXY >> 16;
            }
            triangle       = gGpuPrimCursor;
            gGpuPrimCursor = triangle + 1;
            ACTOR_341300_FILL_SHARD_TRIANGLE(triangle, screenX, screenY);
            addPrim(&gGpuCurrentOt[ACTOR_341300_EMITTER_2_SHARD_OT_SLOT], triangle);
            // Build the next frame's Y-X-Z tumble after drawing the current rotation.
            ACTOR_341300_ADVANCE_SHARD_TUMBLE(coord, work);
            break;
    }
}

#undef ACTOR_341300_ADVANCE_SHARD_TUMBLE
#undef ACTOR_341300_FILL_SHARD_TRIANGLE

/// Starts debris emitter 0; other signed-halfword indices do nothing.
///
/// Replaces its saved handle without releasing an earlier emitter. Other
/// indices do nothing; allocation failure stores NULL.
static void _actor341300StartDebrisEmitter(s16 emitterIndex)
{
    if (emitterIndex == 0) {
        _gActor341300DebrisEmitterHandles[0] = taskSpawnFromTable(_gActor341300DebrisTaskDescriptors, 0, 0, NULL);
    }
}

/// Releases debris emitter 0 and its child shards; other signed-halfword indices do nothing.
///
/// A NULL saved handle is accepted; a non-NULL handle must still be live.
/// Clears the handle after teardown. Other indices do nothing.
static void _actor341300StopDebrisEmitter(s16 emitterIndex)
{
    if ((emitterIndex == 0) && (_gActor341300DebrisEmitterHandles[0] != NULL)) {
        taskKill(_gActor341300DebrisEmitterHandles[0]);
        _gActor341300DebrisEmitterHandles[0] = NULL;
    }
}

/// Emits two interleaved ten-pair bursts of slot-1 debris, then stays idle.
///
/// Its standalone descriptor has no package spawn path. Starting in state 0,
/// clears the signed-halfword timer and emits at positions 0/1 after each of
/// two 16-update waits. State 3 and negative states do nothing. Failed spawns
/// are ignored; the emitter must stay live until successful children attach.
static void _actor341300PairedDebrisEmitterTask(Task* task)
{
    enum { ACTOR_341300_PAIRED_EMITTER_START = 0,
           ACTOR_341300_PAIRED_EMITTER_DONE  = 3 };

    if (task->state < ACTOR_341300_PAIRED_EMITTER_DONE) {
        if (task->state <= ACTOR_341300_PAIRED_EMITTER_START) {
            if (task->state == ACTOR_341300_PAIRED_EMITTER_START) {
                task->killCountdown = 0;
                task->state++;
            }
        } else if (++task->killCountdown >= ACTOR_341300_EMITTER_LATER_DELAY) {
            _actor341300SpawnShardPairs(task, 0, 1);
            task->killCountdown = 0;
            task->state++;
        }
    }
}
