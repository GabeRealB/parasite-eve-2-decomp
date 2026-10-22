#include "actors/actor_461800.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/companion_load.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
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

#include "rooms/neo_ark_r31.h"
#include "../../shared/walk/footstep_walk.h"
#include "../../shared/walk/scripted_walk.h"

static void _footstepWalkUpdate(Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkPlayStepSound(Task* task);
static void _footstepWalkTickAnim(void);
static void _footstepWalkResetAnim(void);
static void _footstepWalkBlendAnim(void);
static s32  _footstepWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

static s16 _gScriptedWalkMode;

static ScriptedWalkAttachmentsWork* _gScriptedWalkWork;

static FootstepWalkWork* _gFootstepWalkWork;

/// Borrowed task pointer of this carrier's singleton sound walker.
///
/// Spawn publishes it for synchronous animation and model-draw messages.
/// The pointer confers no ownership; teardown leaves it unchanged, so those
/// messages require the original task and model to remain live.
static Task* _gFootstepWalkTask;

static s16 _gFootstepWalkMode;

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor461800ExitScriptedWalker(Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static s32  _actor461800PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _actor461800SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor461800ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static s32  _actor461800SetFootstepWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor461800ApplyFootstepWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static void _actor461800ScriptedWalkerTask(Task* task);
static void _actor461800ScriptedWalkerAttachmentTask(Task* task);
static void _actor461800FootstepWalkerTask(Task* task);

static void _actor461800SceneDistortionTask(Task* task);
static void _actor461800SceneFadeTask(Task* task);
static void _actor461800SetSceneFadeState(s32 state);
static void _actor461800SetSceneDistortionState(s32 state);
static void _actor461800FinishScene(void);

/// Scene-effect descriptor indices and the negative callback argument that starts them.
enum {
    ACTOR_461800_SCENE_EFFECT_START      = -1,
    ACTOR_461800_SCENE_EFFECT_FADE       = 0,
    ACTOR_461800_SCENE_EFFECT_DISTORTION = 1,
};

/// Fade states selected by the scene callbacks; ramp changes take one unit per update.
enum {
    ACTOR_461800_SCENE_FADE_OPAQUE = 0,
    ACTOR_461800_SCENE_FADE_CLEAR  = 1,
    ACTOR_461800_SCENE_FADE_IN     = 2,
    ACTOR_461800_SCENE_FADE_OUT    = 3,
};

/// Updates needed for the scene fade to span its full intensity range.
enum { ACTOR_461800_SCENE_FADE_RAMP_UPDATES = 90 };

/// Distortion states; settling reaches both ramp maxima before starting the slow decay.
enum {
    ACTOR_461800_SCENE_DISTORTION_INITIALIZE = 0,
    ACTOR_461800_SCENE_DISTORTION_PULSE      = 1,
    ACTOR_461800_SCENE_DISTORTION_SETTLE     = 2,
    ACTOR_461800_SCENE_DISTORTION_DECAY      = 3,
    ACTOR_461800_SCENE_DISTORTION_STOP       = 4,
};

/// Commands shared by the walkers and their duration in turning updates.
enum {
    ACTOR_461800_WALKER_COMMAND_TURN             = 0,
    ACTOR_461800_WALKER_COMMAND_ENABLE_FOOTSTEPS = 1,
    ACTOR_461800_WALKER_TURN_UPDATES             = 20,
};

/// Model-draw request bits shared by the two scene walkers.
enum {
    ACTOR_461800_WALKER_DRAW_SHOW             = 1 << 0,
    ACTOR_461800_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1,
};

#include "assets/rooms/neo_ark/r31/kyle_and_eve_clips_1.inc"

/// Borrowed singleton task controlling the scene's subtractive fade.
///
/// NULL until a successful start request. Later state requests require the
/// same live task; teardown does not clear this handle.
static Task* _gActor461800SceneFadeTask = NULL;

/// Borrowed singleton task controlling the room's horizontal framebuffer shift.
///
/// NULL until a successful start request. Later state requests require the
/// same live task; stopping the room effect leaves this task and handle alive.
static Task* _gActor461800SceneDistortionTask = NULL;

/// Bodyless scene-effect recipes: fade at index 0, distortion at index 1.
///
/// Both run at priority 32 with unused spawn arguments; there is no terminator.
static TaskDesc _gActor461800SceneEffectTasks[2] = {
    { { { TASK_BODY_NONE, 32 } }, _actor461800SceneFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _actor461800SceneDistortionTask, { .value = 0 } },
};

/// Scene-child selectors and native clip IDs used by this package's requests.
enum {
    ACTOR_461800_SCENE_KYLE           = 0,
    ACTOR_461800_SCENE_EVE            = 1,
    ACTOR_461800_SCENE_KYLE_CLIP_1    = 1,
    ACTOR_461800_SCENE_KYLE_WALK_CLIP = 2,
    ACTOR_461800_SCENE_KYLE_TURN_CLIP = 3,
    ACTOR_461800_SCENE_EVE_CLIP_18    = 18,
};

/// Player's opening room-space placement; angles use 4096 units per turn.
static ActorTransform _gActor461800ScenePlayerPlacement = {
    .pos = { 7710, 980, 6290, 0 },
    .rot = { 0, -1479, 0, 0 },
};

/// Eve scene walker's room-space placement; angles use 4096 units per turn.
static ActorTransform _gActor461800SceneEvePlacement = {
    .pos = { 7030, 980, 7530, 0 },
    .rot = { 0, -1820, 0, 0 },
};

/// Restarts the player's first extension clip with world collision disabled.
///
/// The script replaces source.index with the equipped weapon bank in a copy;
/// the copied set and its clip data stay borrowed throughout playback.
static AnimationPlayRequest _gActor461800ScenePlayerAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Eve scene walker's clip 18 after revealing that model.
///
/// This receiver ignores source.index, blendFrames on reset and collision mode.
static AnimationPlayRequest _gActor461800SceneEveAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_461800_SCENE_EVE_CLIP_18,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced clip-zero reset request; its intended receiver is unproven.
static AnimationPlayRequest D_actor_461800_80133F2C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Restarts Kyle scene walker's clip 1 between the script's timed clips.
///
/// The walker reads animationId and blend; its bank is the loaded local table.
static AnimationPlayRequest _gActor461800SceneKyleAnimation1Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_461800_SCENE_KYLE_CLIP_1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle scene walker's walk clip for each 41-update scripted interval.
///
/// The request does not schedule translation; travel is a separate work count.
static AnimationPlayRequest _gActor461800SceneKyleWalkRequest = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_461800_SCENE_KYLE_WALK_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts Kyle scene walker's turn clip for the 55-update scripted interval.
///
/// The request does not schedule yaw steps; turning is a separate work count.
static AnimationPlayRequest _gActor461800SceneKyleTurnRequest = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_461800_SCENE_KYLE_TURN_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// One borrowed clip copied into the player's equipped-bank extension.
#include "assets/rooms/neo_ark/r31/scene_player_bank.inc"

/// Copies one set-pointer word into the player's bank at the first extension index.
static AnimationBankCopyRequest _gActor461800ScenePlayerAnimationCopyRequest = {
    .sets  = _gActor461800ScenePlayerAnimationSets,
    .count = ARRAY_SIZE(_gActor461800ScenePlayerAnimationSets),
};

/// Neo Ark room-31 scene/audio key, borrowed until the event script finishes.
static EvsSceneKey _gActor461800SceneKey = { .group = 6, .streamId = 18, .subId = 11 };

/// Long interpreter wait allowing the requested ending or room load to take over.
enum { ACTOR_461800_SCENE_TRANSITION_HOLD_UPDATES = 1000000 };

EvsCommand gActor461800NeoArkSceneScript[52] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor461800SceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor461800ScenePlayerAnimationCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor461800ScenePlayerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_EFFECT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneDistortionState }, { .value = ACTOR_461800_SCENE_EFFECT_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_461800_WALKER_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_EVE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_EVE }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor461800SceneEvePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneKyleAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor461800ScenePlayerAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneDistortionState }, { .value = ACTOR_461800_SCENE_DISTORTION_SETTLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneKyleWalkRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneKyleAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneKyleTurnRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 55 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneKyleAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneKyleWalkRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 41 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneKyleAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneDistortionState }, { .value = ACTOR_461800_SCENE_DISTORTION_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_KYLE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_EVE }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_461800_WALKER_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_461800_SCENE_EVE }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor461800SceneEveAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_IN }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor461800SetSceneFadeState }, { .value = ACTOR_461800_SCENE_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor461800FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_461800_SCENE_TRANSITION_HOLD_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor461800NeoArkSceneSkipScript[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor461800FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_461800_SCENE_TRANSITION_HOLD_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/kyle/body.inc"

#include "assets/characters/kyle/hand_right_2.inc"

#include "assets/characters/kyle/hand_left_3.inc"

#include "assets/rooms/neo_ark/r31/kyle_and_eve_clips_2.inc"

/// Latched duration of the first walker's next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword; walk completion replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`.
/// Plain resets leave it intact. Zero requests no transition time; 0..2047
/// keeps the playback timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

/// Singleton Kyle walker messages, followed by the required lookup terminator.
///
/// Five callbacks consume borrowed animation, placement, command or target
/// payloads synchronously; drawing takes integer flags. The published walker
/// and work must remain live through dispatch. The walk target is a VECTOR;
/// placement is an ActorTransform. No callback retains its request pointer.
static TaskMessageEntry _gActor461800ScriptedWalkerMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor461800PlayScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor461800SetScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor461800ApplyScriptedWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _scriptedWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor461800ScriptedWalkerTasks[3] = {
    { { { TASK_BODY_TMD, 192 } }, _actor461800ScriptedWalkerTask, { .model = &gKyleBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor461800ScriptedWalkerAttachmentTask, { .model = &_gKyleHandLeft3 } },
    { { { TASK_BODY_TMD, 192 } }, _actor461800ScriptedWalkerAttachmentTask, { .model = &_gKyleHandRight2 } },
};

/// Native clip-index table borrowed by Kyle walker's twenty-part animation rig.
///
/// Only indices 1..3 contain clips; 0, 4 and 5 are NULL. The play handler checks
/// only the exclusive upper bound of six, not loaded entries or negative IDs.
/// Table and clips must remain loaded while the rig uses them. Travel completion
/// selects absent clip 13, so this table does not support completing walk-to travel.
#include "assets/rooms/neo_ark/r31/kyle_walker_bank.inc"

#include "assets/characters/eve/body.inc"

#include "assets/rooms/neo_ark/r31/kyle_and_eve_clips_3.inc"

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

/// Synchronous messages of this carrier's sound-enabled scene walker.
///
/// Playback and drawing use the published singleton; placement and approach
/// use the receiver. The command handler enables steps or schedules turning.
/// The final sentinel makes unknown IDs return zero. The task borrows this
/// table and its callbacks for its entire lifetime.
static TaskMessageEntry _gFootstepWalkMessageTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor461800SetFootstepWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor461800ApplyFootstepWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor461800FootstepWalkerTaskDesc = { { { TASK_BODY_TMD, 192 } }, _actor461800FootstepWalkerTask, { .model = &_gEveBody } };

/// Native clip-index table borrowed by the sound walker's animation context.
///
/// Loaded keys are 1..8, 10 and 13..19; every other entry is NULL. The
/// play handler's exclusive bound of 35 does not validate loaded entries.
/// Each selected set must provide tracks 1..18, and both this table and its
/// clip data must remain loaded throughout playback.
#include "assets/rooms/neo_ark/r31/eve_walker_bank.inc"

/// Horizontal-shift ramp in 0..16; the room gets 3 + ramp / 3 left-shift pixels.
static s32 _gActor461800SceneShiftRamp = 0;

/// Pulse direction of the shift ramp (0 decreasing, 1 increasing).
static s32 _gActor461800SceneShiftIncreasing = 0;

/// Pulse direction of the second ramp (0 decreasing, 1 increasing).
static s32 _gActor461800SceneSecondRampIncreasing = 0;

/// Second ramp in 0..19, whose maximum gates entry to the slow-decay state.
///
/// It pulses and decays alongside the shift ramp but supplies no rendered
/// value. Its intended visual purpose is unproven.
static s32 _gActor461800SceneSecondRamp = 0;

/// Borrowed work block of the scripted walker and its two model attachments.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// Animation and message handlers require it to remain live; task teardown
/// releases it without clearing this pointer.
static ScriptedWalkAttachmentsWork* _gScriptedWalkWork = NULL;

/// Borrowed singleton Kyle walker task, also owning the published work block.
///
/// Set after successful work allocation. Attachment and model-draw callbacks
/// borrow its model, and animation requests update it immediately. Teardown
/// destroys the enemy and attachments without clearing this pointer; all such
/// callbacks require the original task, work and model to remain live.
static Task* _gActor461800ScriptedWalkerTask = NULL;

/// Signed approach mode of the scripted walker's last walk-to message (`SCRIPTED_WALK_MODE_*`).
static s16 _gScriptedWalkMode = 0;

/// Unreferenced initialized halfword; its role and signed interpretation are unproven.
static u16 D_actor_461800_8014389E = 0xC9A8;

/// Borrowed pointer to the sound walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkWork* _gFootstepWalkWork;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

static void _actor461800InitScriptedWalker(Enemy* enemy, Task* task);

/// Advances the scene ramps and publishes the room framebuffer's horizontal shift.
///
/// Requires the live Neo Ark room-31 effect and a bodyless task. State 0 sets the
/// shift ramp to 16; 1 pulses both ramps; 2 raises them to 16/19 and enters 3;
/// 3 decays them every four calls. The room receives 3..8 pixels of left shift.
/// State 4 clears vertical shake and tells the room effect to exit, leaving this
/// task alive. Other states still advance the signed-halfword tick counter and
/// publish the shift. The second ramp gates settling but supplies no rendered
/// value; its intended visual purpose is unproven.
static void _actor461800SceneDistortionTask(Task* task)
{
    enum {
        ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX    = 16,
        ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX   = 19,
        ACTOR_461800_SCENE_DISTORTION_DECAY_PERIOD      = 4,
        ACTOR_461800_SCENE_DISTORTION_RAMP_PER_PIXEL    = 3,
        ACTOR_461800_SCENE_DISTORTION_BASE_SHIFT_PIXELS = 3,
        ACTOR_461800_SCENE_DISTORTION_DISABLED          = -1,
    };

    /// Steps one triangle-wave ramp and holds each endpoint for one update.
    ///
    /// ramp and increasing are distinct writable `s32` lvalues without side
    /// effects; both are evaluated repeatedly. maximum is positive and has no
    /// side effects, with ramp initially in 0..maximum. The direction flips
    /// only on an update already at an endpoint; a full cycle takes
    /// 2*(maximum+1) calls. Direction uses zero for falling, nonzero for rising.
    /// No caller locals are captured. The two scene ramps step independently;
    /// this macro publishes no framebuffer shift itself.
#define ACTOR_461800_STEP_SCENE_RAMP(ramp, increasing, maximum) \
    {                                                           \
        if ((increasing) != 0) {                                \
            if ((ramp) < (maximum)) {                           \
                (ramp)++;                                       \
            } else {                                            \
                (increasing) = 0;                               \
            }                                                   \
        } else if ((ramp) > 0) {                                \
            (ramp)--;                                           \
        } else {                                                \
            (increasing) = 1;                                   \
        }                                                       \
    }

    switch (task->state) {
        case ACTOR_461800_SCENE_DISTORTION_INITIALIZE:
            _gActor461800SceneShiftRamp = ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX;
            break;
        case ACTOR_461800_SCENE_DISTORTION_PULSE:
            ACTOR_461800_STEP_SCENE_RAMP(_gActor461800SceneShiftRamp, _gActor461800SceneShiftIncreasing,
                                         ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX);
            ACTOR_461800_STEP_SCENE_RAMP(_gActor461800SceneSecondRamp, _gActor461800SceneSecondRampIncreasing,
                                         ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX);
            break;
        case ACTOR_461800_SCENE_DISTORTION_SETTLE:
            if (_gActor461800SceneShiftRamp < ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX) {
                _gActor461800SceneShiftRamp++;
            }
            if (_gActor461800SceneSecondRamp < ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX) {
                _gActor461800SceneSecondRamp++;
            }
            if (_gActor461800SceneShiftRamp == ACTOR_461800_SCENE_DISTORTION_SHIFT_RAMP_MAX && _gActor461800SceneSecondRamp == ACTOR_461800_SCENE_DISTORTION_SECOND_RAMP_MAX) {
                task->state = ACTOR_461800_SCENE_DISTORTION_DECAY;
            }
            break;
        case ACTOR_461800_SCENE_DISTORTION_DECAY:
            if (!(task->killCountdown & (ACTOR_461800_SCENE_DISTORTION_DECAY_PERIOD - 1))) {
                if (_gActor461800SceneShiftRamp > 0) {
                    _gActor461800SceneShiftRamp--;
                }
                if (_gActor461800SceneSecondRamp > 0) {
                    _gActor461800SceneSecondRamp--;
                }
            }
            break;
        case ACTOR_461800_SCENE_DISTORTION_STOP:
            displaySetShakeY(0);
            gNeoArkR31FramebufferShiftPixels = ACTOR_461800_SCENE_DISTORTION_DISABLED;
            return;
    }
#undef ACTOR_461800_STEP_SCENE_RAMP
    task->killCountdown++;
    gNeoArkR31FramebufferShiftPixels = _gActor461800SceneShiftRamp / ACTOR_461800_SCENE_DISTORTION_RAMP_PER_PIXEL + ACTOR_461800_SCENE_DISTORTION_BASE_SHIFT_PIXELS;
}

/// Queues the scene's centered 320x256 subtractive fade at its current intensity.
///
/// `fadeTask->killCountdown` must be in 0..90 ramp updates; scaling to 0..255
/// truncates to a colour byte without clamping. The task is borrowed and unchanged.
/// Requires tag 5 in the current OT and word-aligned frame-arena space for
/// `sizeof(TILE) + sizeof(DR_TPAGE)`, reserved without a capacity check.
/// Packets must remain live until GPU completion. The command precedes the tile
/// and leaves subtractive blending and dithering enabled, with displayed-area
/// drawing disabled. Its texture page at VRAM (0, 0) is unused by the tile.
static inline void _actor461800QueueSceneFade(const Task* fadeTask)
{
    enum {
        ACTOR_461800_SCENE_FADE_MAX_INTENSITY      = 255,
        ACTOR_461800_SCENE_FADE_WIDTH_PIXELS       = 320,
        ACTOR_461800_SCENE_FADE_HEIGHT_PIXELS      = 256,
        ACTOR_461800_SCENE_FADE_OT_TAG             = 5,
        ACTOR_461800_SCENE_FADE_TEXTURE_DEPTH_4BIT = 0,
    };
    TILE*     tile;
    DR_TPAGE* blendCommand;
    u8        intensity;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    intensity      = (fadeTask->killCountdown * ACTOR_461800_SCENE_FADE_MAX_INTENSITY) / ACTOR_461800_SCENE_FADE_RAMP_UPDATES;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->x0 = -ACTOR_461800_SCENE_FADE_WIDTH_PIXELS / 2;
    tile->y0 = -ACTOR_461800_SCENE_FADE_HEIGHT_PIXELS / 2;
    tile->w  = ACTOR_461800_SCENE_FADE_WIDTH_PIXELS;
    tile->h  = ACTOR_461800_SCENE_FADE_HEIGHT_PIXELS;
    tile->r0 = intensity;
    tile->g0 = intensity;
    tile->b0 = intensity;
    addPrim(gGpuCurrentOt + ACTOR_461800_SCENE_FADE_OT_TAG, tile);

    // OT insertion prepends the draw mode so it executes before the tile.
    blendCommand   = gGpuPrimCursor;
    gGpuPrimCursor = blendCommand + 1;
    setDrawTPage(blendCommand, false, true, getTPage(ACTOR_461800_SCENE_FADE_TEXTURE_DEPTH_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt + ACTOR_461800_SCENE_FADE_OT_TAG, blendCommand);
}

/// Queues the scene's 320x256 subtractive fade tile and advances its intensity.
///
/// State 0 holds black, 1 holds clear, 2 reveals the scene and 3 darkens it.
/// The signed-halfword ramp in `killCountdown` runs from 0 to 90 in one-unit
/// updates and is scaled to an unsigned colour byte; other states hold it.
/// Requires a live bodyless task, OT tag 5 and a word-aligned primitive arena
/// with `sizeof(TILE) + sizeof(DR_TPAGE)` free bytes. Packets stay live until GPU
/// completion. Subtractive blending and dithering remain active after drawing;
/// displayed-area drawing is disabled. Neither ramp endpoint ends the task.
static void _actor461800SceneFadeTask(Task* task)
{
    switch (task->state) {
        case ACTOR_461800_SCENE_FADE_CLEAR:
            task->killCountdown = 0;
            break;
        case ACTOR_461800_SCENE_FADE_OUT:
            if (task->killCountdown < ACTOR_461800_SCENE_FADE_RAMP_UPDATES) {
                task->killCountdown++;
            }
            break;
        case ACTOR_461800_SCENE_FADE_IN:
            if (task->killCountdown > 0) {
                task->killCountdown--;
            }
            break;
        case ACTOR_461800_SCENE_FADE_OPAQUE:
            task->killCountdown = ACTOR_461800_SCENE_FADE_RAMP_UPDATES;
            break;
    }
    _actor461800QueueSceneFade(task);
}

/// Starts or selects a state of the scene's fade task.
///
/// Any negative state spawns the singleton if it is absent; scripts use
/// `ACTOR_461800_SCENE_EFFECT_START`. A nonnegative state requires that spawn to
/// have succeeded and the published task to remain live. The state is copied
/// unchanged; this callback neither waits for the ramp nor releases the task.
static void _actor461800SetSceneFadeState(s32 state)
{
    if (state < 0) {
        if (_gActor461800SceneFadeTask == NULL) {
            _gActor461800SceneFadeTask = taskSpawnFromTable(_gActor461800SceneEffectTasks, ACTOR_461800_SCENE_EFFECT_FADE, 0, 0);
        }
    } else {
        _gActor461800SceneFadeTask->state = state;
    }
}

/// Starts or selects a state of the scene's distortion task.
///
/// Any negative state spawns the singleton if it is absent; scripts use
/// `ACTOR_461800_SCENE_EFFECT_START`. A nonnegative state requires that spawn to
/// have succeeded and the published task to remain live. The state is copied
/// unchanged; this callback neither waits for the ramp nor releases the task.
static void _actor461800SetSceneDistortionState(s32 state)
{
    if (state < 0) {
        if (_gActor461800SceneDistortionTask == NULL) {
            _gActor461800SceneDistortionTask = taskSpawnFromTable(_gActor461800SceneEffectTasks, ACTOR_461800_SCENE_EFFECT_DISTORTION, 0, 0);
        }
    } else {
        _gActor461800SceneDistortionTask->state = state;
    }
}

/// Awards rescue bonuses and chooses the scene's ending or Shelter transition.
///
/// Does nothing during attract demo 9. The companion sterilization event and
/// meeting progress award their collected bits independently. With neither the
/// item follow-up nor meeting progress, requests the ending with a 15-frame
/// fade. Otherwise selects Shelter R36, room/warp 1, in the live save destination,
/// starts the session transition and normal load caption, then finishes scene
/// streaming and restores its saved RNG state. Called by normal and skip scripts.
static void _actor461800FinishScene(void)
{
    enum {
        ACTOR_461800_SCENE_DEMO                   = 9,
        ACTOR_461800_STERILIZATION_WITH_COMPANION = 2,
        ACTOR_461800_ENDING_FADE_FRAMES           = 15,
        ACTOR_461800_TRANSITION_TASK_BANK         = GAME_FLOW_RELOAD_TASK_BANK,
        ACTOR_461800_TRANSITION_TASK_INDEX        = GAME_FLOW_RELOAD_TASK_SLOT,
    };
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_461800_SCENE_DEMO) {
        // Award the rescue bonuses before selecting the ending path.
        if (gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == ACTOR_461800_STERILIZATION_WITH_COMPANION) {
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_SOLDIER_RESCUE_BONUS);
        }
        if (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) != 0) {
            inventorySetCollectedBit(INVENTORY_COLLECTION_ID_PIERCE_RESCUE_BONUS);
        }
        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) == 0 && gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == 0) {
            gGameSession->restartMode     = GAME_SESSION_RESTART_ENDING;
            gGameSession->deathFadeFrames = ACTOR_461800_ENDING_FADE_FRAMES;
            return;
        }
        // Hand the live save destination to the session transition and finish streaming.
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_MINE_SHELTER;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_R36;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = 1;
        gDisplayState.spriteVariant                                 = 1;
        taskSpawn(ACTOR_461800_TRANSITION_TASK_BANK, ACTOR_461800_TRANSITION_TASK_INDEX, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
        streamFinishScene();
    }
}

/// Initializes the scripted scene walker and its two hand-model attachment tasks.
///
/// Owns zeroed primary-heap work and publishes it for singleton walk/message
/// handlers. Allocation failure destroys the enemy. The view-parented body
/// borrows work-owned lighting matrices and samples its existing composed
/// translation with Y reduced by 800; this state does not refresh that cache.
/// Starts attachment entries 1/2 on parts 8/12, applies the walker's placement
/// textures to each successful spawn, then updates motion and advances state.
/// Attachment allocation failures retain NULL in the corresponding work slot.
static void _actor461800InitScriptedWalker(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_461800_WALKER_SORT_OFFSET            = 1,
        ACTOR_461800_WALKER_LIGHT_SAMPLE_Y_OFFSET  = -800,
        ACTOR_461800_WALKER_FIRST_ANIMATION        = 1,
        ACTOR_461800_WALKER_ATTACHMENT1_DESCRIPTOR = 1,
        ACTOR_461800_WALKER_ATTACHMENT2_DESCRIPTOR = 2,
        ACTOR_461800_WALKER_ATTACHMENT1_PART       = 8,
        ACTOR_461800_WALKER_ATTACHMENT2_PART       = 12,
        ACTOR_461800_WALKER_LIGHT_COUNT            = 3,
    };
    VECTOR     lightingPosition;
    GfxCoord*  rootCoord;
    TmdObject* model;
    Task*      attachment1Task;
    Task*      attachment2Task;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    task->work = (_gScriptedWalkWork = memCalloc(sizeof(ScriptedWalkAttachmentsWork), false));
    if (_gScriptedWalkWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor461800ExitScriptedWalker;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = ACTOR_461800_WALKER_SORT_OFFSET;
    model->flags                     = 0;
    model->lightMtx                  = &_gScriptedWalkWork->light;
    model->colorMtx                  = &_gScriptedWalkWork->color;
    lightingPosition.vx              = rootCoord->workm.t[0];
    lightingPosition.vy              = rootCoord->workm.t[1] + ACTOR_461800_WALKER_LIGHT_SAMPLE_Y_OFFSET;
    _gActor461800ScriptedWalkerTask  = task;
    lightingPosition.vz              = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingPosition, 0, ACTOR_461800_WALKER_LIGHT_COUNT);
    animationInitContext(&_gScriptedWalkWork->rig.anim, _gActor461800ScriptedWalkerAnimationSets, model,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = ACTOR_461800_WALKER_FIRST_ANIMATION;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;

    // Each attachment borrows a hand part and the walker lighting matrices.
    attachment1Task = taskSpawnFromTable(gActor461800ScriptedWalkerTasks, ACTOR_461800_WALKER_ATTACHMENT1_DESCRIPTOR, ACTOR_461800_WALKER_ATTACHMENT1_PART, 0);
    if (attachment1Task != NULL) {
        _gScriptedWalkWork->attachment1 = attachment1Task;
        _actorRenderApplyPlacementTextureOffsets(attachment1Task->extra.tmd, task->spawnArg2.pointer);
    }

    attachment2Task = taskSpawnFromTable(gActor461800ScriptedWalkerTasks, ACTOR_461800_WALKER_ATTACHMENT2_DESCRIPTOR, ACTOR_461800_WALKER_ATTACHMENT2_PART, 0);
    if (attachment2Task != NULL) {
        _gScriptedWalkWork->attachment2 = attachment2Task;
        _actorRenderApplyPlacementTextureOffsets(attachment2Task->extra.tmd, task->spawnArg2.pointer);
    }

    _gScriptedWalkWork->st.travel  = 0;
    _gScriptedWalkWork->turnFrames = 0;
    task->msgTable                 = _gActor461800ScriptedWalkerMessages;
    _scriptedWalkUpdate(task);
    task->state++;
}

#include "../../shared/walk/scripted_walk_update.inc.c"

/// Publishes and dispatches the scripted Kyle walker with two held models.
///
/// Task state must be 0 (initialize) or 1 (frame update), with the live enemy in
/// spawnArg2.pointer and the twenty-part model. Publication precedes dispatch
/// so singleton animation/message handlers use this task's work. Initialization
/// replaces the initially NULL publication after allocating owned work; held
/// models and their borrowed lighting remain live until teardown.
static void _actor461800ScriptedWalkerTask(Task* task)
{
    EnemyTaskFunc handlers[2] = {
        _actor461800InitScriptedWalker,
        _actorRenderWalkerFrame,
    };

    _gScriptedWalkWork = task->work;
    handlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _scriptedWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Begins teardown of the scripted walker and its two attachment tasks.
///
/// Requires a live task with its owned `Enemy` in `spawnArg2.pointer` and a
/// `ScriptedWalkAttachmentsWork` block with two live, non-NULL attachments.
/// The published work and task pointers are left stale. Work is released by
/// `enemyDestroy` before its attachment pointers are read; this retained order
/// depends on those freed bytes remaining readable until consumed.
static void _actor461800ExitScriptedWalker(Task* task)
{
    ScriptedWalkAttachmentsWork* work = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    // Preserve the post-release attachment reads and teardown order.
    taskKill(work->attachment1);
    taskKill(work->attachment2);
}

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Attaches a model root to a borrowed walker part and clears all model flags.
///
/// `attachmentRoot` is the preloaded `attachmentModel->coords` of a live model.
/// `walkerPart` must be live and its parent chain must not reach that root.
/// The root's local transform is retained in the walker's part space; its cached
/// composition is marked stale. Clearing every flag permits active drawing and
/// automatic buffer allocation, retaining any existing primitive buffer.
/// No lighting or teardown ownership is transferred. The borrowed parent must
/// remain live whenever the attachment root is composed.
static inline void _actor461800AttachWalkerModel(TmdObject* attachmentModel, GfxCoord* attachmentRoot, GfxCoord* walkerPart)
{
    attachmentRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    attachmentModel->flags       = 0;
    attachmentRoot->parent       = walkerPart;
}

/// Parents a hand model to the scripted walker and updates its room lighting.
///
/// Requires live TMD models on this task and the published walker task.
/// `spawnArg1.value` is a coordinate index in that walker's twenty-part model;
/// the two hand spawns use 8 and 12. State 0 attaches the root and clears model
/// flags, then enters state 1. State 1 samples all three lights at the walker
/// root's composed world translation, 800 world units above it in negative Y.
/// The attachment borrows the parent coordinate until teardown; transforms must
/// be composed and the lighting helper's scratch/GTE state available.
static void _actor461800ScriptedWalkerAttachmentTask(Task* task)
{
    enum {
        ACTOR_461800_ATTACHMENT_INITIALIZE   = 0,
        ACTOR_461800_ATTACHMENT_LIGHT        = 1,
        ACTOR_461800_ATTACHMENT_LIGHT_HEIGHT = 800,
        ACTOR_461800_ATTACHMENT_LIGHT_COUNT  = 3,
    };
    TmdObject* attachmentModel = task->extra.tmd;
    GfxCoord*  attachmentRoot  = attachmentModel->coords;
    GfxCoord*  walkerCoords    = _gActor461800ScriptedWalkerTask->extra.tmd->coords;
    GfxCoord*  parentPart      = walkerCoords + task->spawnArg1.value;
    VECTOR     lightSample;

    switch (task->state) {
        case ACTOR_461800_ATTACHMENT_INITIALIZE:
            _actor461800AttachWalkerModel(attachmentModel, attachmentRoot, parentPart);
            task->state++;
            break;
        case ACTOR_461800_ATTACHMENT_LIGHT:
            lightSample.vx = walkerCoords->workm.t[0];
            lightSample.vy = walkerCoords->workm.t[1] - ACTOR_461800_ATTACHMENT_LIGHT_HEIGHT;
            lightSample.vz = walkerCoords->workm.t[2];
            worldCoordSetModelLighting(attachmentModel, &lightSample, 0, ACTOR_461800_ATTACHMENT_LIGHT_COUNT);
            break;
    }
}

#include "../../shared/walk/scripted_walk_tick_anim.inc.c"

#include "../../shared/walk/scripted_walk_reset_anim.inc.c"

#include "../../shared/walk/scripted_walk_blend_anim.inc.c"

/// Reseeds the published scripted walker from a borrowed animation request.
///
/// Requires a live walker and initialized rig. Loaded clip keys are 1..3. The
/// signed check rejects IDs >= 6 with -1 but also accepts negative IDs and NULL
/// entries 0/4/5; those are unsafe unless narrowing selects a loaded clip. An
/// accepted ID is narrowed to `s16`. Nonzero blend latches the low signed halfword
/// of `blendFrames` (whole normal-rate frames; 0..2047 keeps playback nonnegative);
/// zero blend restarts and leaves that latch intact. Reseeding runs immediately.
/// Receiver, message ID, second payload and the other request words are ignored.
/// Returns 0 after acceptance and retains no request pointer.
static s32 _actor461800PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    if (request->animationId < (s32)ARRAY_SIZE(_gActor461800ScriptedWalkerAnimationSets)) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        _scriptedWalkUpdate(_gActor461800ScriptedWalkerTask);
        return 0;
    }
    return -1;
}

/// Replaces the scripted walker and both hand models' draw flags.
///
/// Requires a live published walker, work block and both attachment models.
/// Bit 0 permits active drawing; bit 1 suppresses automatic primitive-buffer
/// allocation. All other model flags are cleared and other request bits ignored.
/// No buffers are allocated or released. Receiver, message ID and second payload
/// are ignored. Returns 0.
static s32 _actor461800SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* walkerModel      = _gActor461800ScriptedWalkerTask->extra.tmd;
    TmdObject* attachment1Model = _gScriptedWalkWork->attachment1->extra.tmd;
    TmdObject* attachment2Model = _gScriptedWalkWork->attachment2->extra.tmd;

    if (drawFlags & ACTOR_461800_WALKER_DRAW_SHOW) {
        walkerModel->flags      = 0;
        attachment1Model->flags = 0;
        attachment2Model->flags = 0;
    } else {
        walkerModel->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment1Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment2Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_461800_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags      |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment1Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment2Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/scripted_walk_place.inc.c"

/// Schedules twenty turning updates for the published scripted walker.
///
/// Requires live published work and a command borrowed through dispatch.
/// Command 0 arms the countdown consumed while turn clip 3 plays; it does not
/// select the clip. Other commands do nothing. Context tags, receiver, message
/// ID and second payload are ignored. Returns 0.
static s32 _actor461800ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    if (request->command == ACTOR_461800_WALKER_COMMAND_TURN) {
        _gScriptedWalkWork->turnFrames = ACTOR_461800_WALKER_TURN_UPDATES;
    }
    return 0;
}

#include "../../shared/walk/scripted_walk_to.inc.c"

#include "../../shared/walk/footstep_walk_spawn.inc.c"

/// Dispatches the scene's footstep walker through initialization or its frame update.
///
/// `task` must own a live nineteen-part TMD model and an `Enemy` in
/// `spawnArg2.pointer`. State 0 requires no existing work allocation, allocates
/// and publishes `FootstepWalkWork`, binds playback and messages, and enters
/// state 1; allocation failure tears it down.
/// State 1 requires that initialized work and refreshes lighting, movement,
/// animation and the ground shadow. Other state indices are out of bounds.
/// Work is published before dispatch for singleton animation and message handlers;
/// it remains borrowed from the task and must not be used after teardown.
/// Required model, clip, room-light, scratch and GTE state must remain available
/// during the selected handler. Initialization may invalidate the task or enemy;
/// neither is accessed after dispatch.
static void _actor461800FootstepWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _footstepWalkSpawn,
        _actorRenderWalkerFrameSecond,
    };

    _gFootstepWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrameSecond
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER             _footstepWalkUpdate
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy and begins teardown of its task and model.
///
/// `task` must be live with its owned `Enemy` in `spawnArg2.pointer`.
/// Enemy storage is invalid on return; task work and the model follow
/// `taskKill`'s immediate/deferred release rules. Do not use the task afterwards.
static void _footstepWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/walk/footstep_walk_play_steps.inc.c"

#include "../../shared/walk/footstep_walk_tick_anim.inc.c"

#include "../../shared/walk/footstep_walk.inc.c"

/// Replaces the published footstep walker's model draw flags.
///
/// Requires a live TMD model in `_gFootstepWalkTask`.
/// Bit 0 permits active drawing; bit 1 suppresses automatic primitive-buffer
/// allocation. All other model flags are cleared and other request bits ignored.
/// No buffers are allocated or released. Receiver, message ID and second payload
/// are ignored. Returns 0.
static s32 _actor461800SetFootstepWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* model;

    model = _gFootstepWalkTask->extra.tmd;
    if (drawFlags & ACTOR_461800_WALKER_DRAW_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_461800_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/footstep_walk_place.inc.c"

/// Applies scheduled turning or footstep enablement to the published sound walker.
///
/// Requires live `_gFootstepWalkWork` and a command borrowed through dispatch.
/// Command 0 schedules twenty updates while turn clip 3 plays, without selecting
/// that clip. Command 1 enables footstep sounds until work teardown; other
/// commands do nothing. Context tags, receiver, message ID and second payload
/// are ignored. Returns 0.
static s32 _actor461800ApplyFootstepWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    s32 commandId;

    commandId = request->command;
    switch (commandId) {
        case ACTOR_461800_WALKER_COMMAND_TURN:
            _gFootstepWalkWork->turnFrames = ACTOR_461800_WALKER_TURN_UPDATES;
            break;
        case ACTOR_461800_WALKER_COMMAND_ENABLE_FOOTSTEPS:
            _gFootstepWalkWork->playFootsteps = commandId;
            break;
    }
    return 0;
}

/// Selects this sound walker's allocated work type for the walk-target handler.
///
/// The receiver owns a live `FootstepWalkWork`; the fragment writes its
/// signed-halfword `st.yaw` (4096 units per turn) and `st.travel` (whole moving
/// updates). This type-name alias evaluates no expressions and lasts one inclusion.
#define FOOTSTEP_WALK_WORK_T FootstepWalkWork
#include "../../shared/walk/footstep_walk_to.inc.c"
#undef FOOTSTEP_WALK_WORK_T

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW
