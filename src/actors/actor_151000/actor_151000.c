#include "actors/actor_151000.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/walk/footstep_walk.h"

static void _footstepWalkUpdate(Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkPlayStepSound(Task* task);
static void _footstepWalkTickAnim(void);
static void _footstepWalkResetAnim(void);
static void _footstepWalkBlendAnim(void);
static s32  _footstepWalkPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32  _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

/// Script-controlled opaque-black cover switch (zero off, any nonzero word on).
///
/// The setter stores the full signed word; the drawing task only tests for zero.
static s32 _gActor151000BlackoutEnabled;

static FootstepWalkWork* _gFootstepWalkWork;

/// Borrowed task pointer of this carrier's singleton sound walker.
///
/// Spawn publishes it for synchronous animation and model-draw messages.
/// The pointer confers no ownership; teardown leaves it unchanged, so those
/// messages require the original task and model to remain live.
static Task* _gFootstepWalkTask;

static s16 _gFootstepWalkMode;

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);

static TmdSource _gActor151000AyaBreaBody;
static void      _actor151000WalkerTask(Task* task);

static s32 _actor151000SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument);
static s32 _actor151000ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);

static void _actor151000SetBlackout(s32 enabled);

static void _actor151000BlackoutTask(Task* task);

/// Selectors and flags used by the garden arrival data.
enum {
    ACTOR_151000_GARDEN_TASK_PRIORITY      = 192,
    ACTOR_151000_GARDEN_CAP_ARRIVAL        = 2,
    ACTOR_151000_GARDEN_RETURN_VIEW        = 3,
    ACTOR_151000_GARDEN_ENABLE_FOOTSTEPS   = 1,
    ACTOR_151000_GARDEN_WALKER_DRAW_HIDE   = 0,
    ACTOR_151000_GARDEN_WALKER_DRAW_SHOW   = 1,
    ACTOR_151000_GARDEN_WEAPONS_PLAYER     = 0,
    ACTOR_151000_GARDEN_BLACKOUT_OFF       = 0,
    ACTOR_151000_GARDEN_BLACKOUT_ON        = 1,
    ACTOR_151000_GARDEN_PLAYER_FIRST_CLIP  = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_151000_GARDEN_PLAYER_SECOND_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 1,
    ACTOR_151000_GARDEN_PLAYER_THIRD_CLIP  = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_151000_GARDEN_PLAYER_RETURN_CLIP = 1,
    ACTOR_151000_GARDEN_WALKER_BASE_CLIP   = 16,
    ACTOR_151000_GARDEN_WALKER_CUE_CLIP    = 17,
    ACTOR_151000_GARDEN_WALKER_BLEND_CLIP  = 19,
};

/// Garden-bank sound cues played together during the arrival scene.
///
/// Their audible roles are unproven.
enum {
    ACTOR_151000_GARDEN_SOUND_CUE_7 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN, 7),
    ACTOR_151000_GARDEN_SOUND_CUE_8 = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN, 8),
};

static AnimationPackedPose _gActor151000Animation00ED8Bank1[2] = {
#include "assets/actor_151000_animation_00ED8_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation00ED8Bank4[25] = {
#include "assets/actor_151000_animation_00ED8_bank4.inc"
};

static AnimationRecord _gActor151000Animation00ED8Records[96] = {
#include "assets/actor_151000_animation_00ED8_records.inc"
};

static u16 _gActor151000Animation00ED8Indices[20] = {
#include "assets/actor_151000_animation_00ED8_indices.inc"
};

static AnimationSet _gActor151000Animation00ED8 = {
    _gActor151000Animation00ED8Records,
    _gActor151000Animation00ED8Indices,
    { NULL, _gActor151000Animation00ED8Bank1, NULL, NULL, _gActor151000Animation00ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation012A0Bank1[6] = {
#include "assets/actor_151000_animation_012A0_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation012A0Bank4[76] = {
#include "assets/actor_151000_animation_012A0_bank4.inc"
};

static AnimationRecord _gActor151000Animation012A0Records[128] = {
#include "assets/actor_151000_animation_012A0_records.inc"
};

static u16 _gActor151000Animation012A0Indices[20] = {
#include "assets/actor_151000_animation_012A0_indices.inc"
};

static AnimationSet _gActor151000Animation012A0 = {
    _gActor151000Animation012A0Records,
    _gActor151000Animation012A0Indices,
    { NULL, _gActor151000Animation012A0Bank1, NULL, NULL, _gActor151000Animation012A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation01518Bank1[3] = {
#include "assets/actor_151000_animation_01518_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation01518Bank4[45] = {
#include "assets/actor_151000_animation_01518_bank4.inc"
};

static AnimationRecord _gActor151000Animation01518Records[84] = {
#include "assets/actor_151000_animation_01518_records.inc"
};

static u16 _gActor151000Animation01518Indices[20] = {
#include "assets/actor_151000_animation_01518_indices.inc"
};

static AnimationSet _gActor151000Animation01518 = {
    _gActor151000Animation01518Records,
    _gActor151000Animation01518Indices,
    { NULL, _gActor151000Animation01518Bank1, NULL, NULL, _gActor151000Animation01518Bank4, NULL, NULL, NULL },
};

/// Bodyless, priority-192 task that covers the scene while the blackout switch is nonzero.
///
/// Spawn arguments and descriptor metadata are unused. The actor package's
/// callback must remain loaded until the switch is cleared and the task exits.
static TaskDesc _gActor151000BlackoutTaskDesc = { { { TASK_BODY_NONE, ACTOR_151000_GARDEN_TASK_PRIORITY } }, _actor151000BlackoutTask, { .value = 0 } };

/// Three player clips installed at equipped-bank indices 47..49 for the garden scene.
///
/// The copy request transfers 32 words, reading 29 words beyond this array:
/// all five following play requests, then the first four words of
/// `_gActor151000WalkerAnimation16`. Preserve that original overread and the
/// adjacency of these objects. Only the first three copied words are clips;
/// the remaining extension words must never be selected for playback.
/// The player borrows the clips while actor_151000 remains loaded.
static AnimationSet* _gActor151000PlayerAnimationBankExtension[3] = { &_gActor151000Animation00ED8, &_gActor151000Animation012A0, &_gActor151000Animation01518 };

/// Unaddressed bank-1 clip-47 reset request included in the raw extension copy.
///
/// Its intended playback receiver and visual role are unproven.
static AnimationPlayRequest _gActor151000UnusedBank1Animation47 = { { .index = 1 }, ACTOR_151000_GARDEN_PLAYER_FIRST_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// First player scene clip, reset without world collision after the opening CAP cue.
///
/// The event interpreter replaces the stored bank selector with the equipped bank.
static AnimationPlayRequest _gActor151000PlayerAnimation47 = { { .index = 1 }, ACTOR_151000_GARDEN_PLAYER_FIRST_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Second player scene clip, reset without world collision at a later CAP cue.
///
/// The event interpreter replaces the stored bank selector with the equipped bank.
static AnimationPlayRequest _gActor151000PlayerAnimation48 = { { .index = 1 }, ACTOR_151000_GARDEN_PLAYER_SECOND_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Third player scene clip, reset without world collision at the return placement.
///
/// The event interpreter replaces the stored bank selector with the equipped bank.
static AnimationPlayRequest _gActor151000PlayerAnimation49 = { { .index = 1 }, ACTOR_151000_GARDEN_PLAYER_THIRD_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Unaddressed bank-1 clip-0 reset request included in the raw extension copy.
///
/// Its intended playback receiver and visual role are unproven.
static AnimationPlayRequest _gActor151000UnusedBank1Animation0 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets walker clip 16 at scene opening, completion and skip cleanup.
///
/// The walker ignores the bank selector and world-collision option.
static AnimationPlayRequest _gActor151000WalkerAnimation16 = { { .index = 1 }, ACTOR_151000_GARDEN_WALKER_BASE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets walker clip 17 at its later CAP cue; bank and collision words are ignored.
static AnimationPlayRequest _gActor151000WalkerAnimation17 = { { .index = 1 }, ACTOR_151000_GARDEN_WALKER_CUE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the walker to clip 19 over eight frames after the blackout clears.
///
/// The walker ignores the bank selector and world-collision option.
static AnimationPlayRequest _gActor151000WalkerAnimation19 = { { .index = 1 }, ACTOR_151000_GARDEN_WALKER_BLEND_CLIP, ANIMATION_BLEND_INTERPOLATE, FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Unaddressed bank-0 reset requests for clips 13 and 14.
///
/// Their intended receiver and scene role are unproven.
static AnimationPlayRequest _gActor151000UnusedBank0Animations13And14[2] = {
    { { .index = 0 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Resets the walker's forward-travel clip before its departure-target message.
///
/// The walker ignores the bank selector and world-collision option.
static AnimationPlayRequest _gActor151000WalkerAnimation15 = { { .index = 0 }, FOOTSTEP_WALK_ANIM_WALK_15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player clip 1 without collision during normal and skipped scene cleanup.
///
/// The event interpreter replaces the stored bank selector with the equipped bank.
static AnimationPlayRequest _gActor151000PlayerAnimation1 = { { .index = 1 }, ACTOR_151000_GARDEN_PLAYER_RETURN_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Enables walker footstep sounds for the rest of its task lifetime.
///
/// The garden stage/area tags are stored but ignored by this walker.
static ActorCommand _gActor151000EnableWalkerFootstepsCommand = { { .loc = { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_GARDEN } }, ACTOR_151000_GARDEN_ENABLE_FOOTSTEPS };

/// Player's opening garden placement in world units, with zero Euler angles.
static ActorTransform _gActor151000PlayerArrivalPlacement = { { -7700, 0, -18100, 0 }, { 0, 0, 0, 0 } };

/// Player's return placement for normal and skip paths; yaw is 227/4096 turns.
static ActorTransform _gActor151000PlayerReturnPlacement = { { -6300, 0, -17200, 0 }, { 0, 227, 0, 0 } };

/// Walker's opening garden placement in world units, with zero yaw.
static ActorTransform _gActor151000WalkerArrivalPlacement = { { -7480, 0, -17590, 0 }, { 0, 0, 0, 0 } };

/// Walker's placement before its forward departure, in world units with zero yaw.
static ActorTransform _gActor151000WalkerDeparturePlacement = { { -4900, 0, -15550, 0 }, { 0, 0, 0, 0 } };

/// Walker's departure destination in world units.
///
/// Only the position's X/Z components are consumed; the orientation is unused.
static ActorTransform _gActor151000WalkerDepartureTarget = { { -3600, 0, -12500, 0 }, { 0, 0, 0, 0 } };

/// Installs three player clips and 29 following raw words in the equipped-bank extension.
///
/// The fixed 32-word transfer intentionally includes adjacent requests, as
/// documented on `_gActor151000PlayerAnimationBankExtension`. This request is
/// borrowed through dispatch; the copied clip pointers remain borrowed afterwards.
static AnimationBankCopyRequest _gActor151000PlayerAnimationBankCopy = { { .sets = _gActor151000PlayerAnimationBankExtension }, ANIMATION_BANK_EXTENSION_CAPACITY };

EvsCommand gActor151000GardenArrivalScript[47] = {
    // Set up the player bank and both actors while the scene is covered.
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_151000_GARDEN_CAP_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor151000PlayerAnimationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor151000PlayerArrivalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor151000WalkerArrivalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gActor151000EnableWalkerFootstepsCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor151000SetBlackout }, { .value = ACTOR_151000_GARDEN_BLACKOUT_ON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP cues coordinate the clips, sound pair and two fade ramps.
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_151000_GARDEN_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_151000_GARDEN_WALKER_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor151000PlayerAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor151000WalkerAnimation16 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_151000_GARDEN_SOUND_CUE_7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_151000_GARDEN_SOUND_CUE_8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_ADD }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor151000SetBlackout }, { .value = ACTOR_151000_GARDEN_BLACKOUT_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor151000WalkerAnimation19 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor151000WalkerAnimation17 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor151000PlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Reposition both actors and start the walker's forward departure.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor151000PlayerReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor151000PlayerAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor151000WalkerDeparturePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor151000WalkerAnimation15 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .pointer = &_gActor151000WalkerDepartureTarget.pos } }, { .value = FOOTSTEP_WALK_MODE_FORWARD } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore the room view and actor presentation before releasing control.
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_151000_GARDEN_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor151000WalkerAnimation16 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_151000_GARDEN_WALKER_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor151000PlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_151000_GARDEN_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor151000GardenArrivalSkipScript[15] = {
    // Fade over scene cleanup and restore the same player/view endpoint.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_151000_GARDEN_RETURN_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor151000SetBlackout }, { .value = ACTOR_151000_GARDEN_BLACKOUT_OFF }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor151000WalkerAnimation16 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_151000_GARDEN_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor151000PlayerAnimation1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_151000_GARDEN_WALKER_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor151000PlayerReturnPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor151000AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor151000AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor151000AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor151000AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor151000AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor151000AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor151000AyaBreaBodyPartVerts,
    _gActor151000AyaBreaBodyVerts,
    _gActor151000AyaBreaBodyNormals,
    _gActor151000AyaBreaBodySkeleton,
    _gActor151000AyaBreaBodyStream,
};

static AnimationPackedPose _gActor151000Animation07660Bank1[2] = {
#include "assets/actor_151000_animation_07660_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation07660Bank4[23] = {
#include "assets/actor_151000_animation_07660_bank4.inc"
};

static AnimationRecord _gActor151000Animation07660Records[84] = {
#include "assets/actor_151000_animation_07660_records.inc"
};

static u16 _gActor151000Animation07660Indices[20] = {
#include "assets/actor_151000_animation_07660_indices.inc"
};

static AnimationSet _gActor151000Animation07660 = {
    _gActor151000Animation07660Records,
    _gActor151000Animation07660Indices,
    { NULL, _gActor151000Animation07660Bank1, NULL, NULL, _gActor151000Animation07660Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation07A58Bank1[7] = {
#include "assets/actor_151000_animation_07A58_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation07A58Bank4[75] = {
#include "assets/actor_151000_animation_07A58_bank4.inc"
};

static AnimationRecord _gActor151000Animation07A58Records[138] = {
#include "assets/actor_151000_animation_07A58_records.inc"
};

static u16 _gActor151000Animation07A58Indices[20] = {
#include "assets/actor_151000_animation_07A58_indices.inc"
};

static AnimationSet _gActor151000Animation07A58 = {
    _gActor151000Animation07A58Records,
    _gActor151000Animation07A58Indices,
    { NULL, _gActor151000Animation07A58Bank1, NULL, NULL, _gActor151000Animation07A58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0863CBank1[22] = {
#include "assets/actor_151000_animation_0863C_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0863CBank4[298] = {
#include "assets/actor_151000_animation_0863C_bank4.inc"
};

static AnimationRecord _gActor151000Animation0863CRecords[377] = {
#include "assets/actor_151000_animation_0863C_records.inc"
};

static u16 _gActor151000Animation0863CIndices[20] = {
#include "assets/actor_151000_animation_0863C_indices.inc"
};

static AnimationSet _gActor151000Animation0863C = {
    _gActor151000Animation0863CRecords,
    _gActor151000Animation0863CIndices,
    { NULL, _gActor151000Animation0863CBank1, NULL, NULL, _gActor151000Animation0863CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation088A4Bank1[4] = {
#include "assets/actor_151000_animation_088A4_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation088A4Bank4[48] = {
#include "assets/actor_151000_animation_088A4_bank4.inc"
};

static AnimationRecord _gActor151000Animation088A4Records[74] = {
#include "assets/actor_151000_animation_088A4_records.inc"
};

static u16 _gActor151000Animation088A4Indices[20] = {
#include "assets/actor_151000_animation_088A4_indices.inc"
};

static AnimationSet _gActor151000Animation088A4 = {
    _gActor151000Animation088A4Records,
    _gActor151000Animation088A4Indices,
    { NULL, _gActor151000Animation088A4Bank1, NULL, NULL, _gActor151000Animation088A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation08CACBank1[5] = {
#include "assets/actor_151000_animation_08CAC_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation08CACBank4[76] = {
#include "assets/actor_151000_animation_08CAC_bank4.inc"
};

static AnimationRecord _gActor151000Animation08CACRecords[147] = {
#include "assets/actor_151000_animation_08CAC_records.inc"
};

static u16 _gActor151000Animation08CACIndices[20] = {
#include "assets/actor_151000_animation_08CAC_indices.inc"
};

static AnimationSet _gActor151000Animation08CAC = {
    _gActor151000Animation08CACRecords,
    _gActor151000Animation08CACIndices,
    { NULL, _gActor151000Animation08CACBank1, NULL, NULL, _gActor151000Animation08CACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation090B8Bank1[5] = {
#include "assets/actor_151000_animation_090B8_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation090B8Bank4[89] = {
#include "assets/actor_151000_animation_090B8_bank4.inc"
};

static AnimationRecord _gActor151000Animation090B8Records[135] = {
#include "assets/actor_151000_animation_090B8_records.inc"
};

static u16 _gActor151000Animation090B8Indices[20] = {
#include "assets/actor_151000_animation_090B8_indices.inc"
};

static AnimationSet _gActor151000Animation090B8 = {
    _gActor151000Animation090B8Records,
    _gActor151000Animation090B8Indices,
    { NULL, _gActor151000Animation090B8Bank1, NULL, NULL, _gActor151000Animation090B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation093C8Bank1[4] = {
#include "assets/actor_151000_animation_093C8_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation093C8Bank4[59] = {
#include "assets/actor_151000_animation_093C8_bank4.inc"
};

static AnimationRecord _gActor151000Animation093C8Records[105] = {
#include "assets/actor_151000_animation_093C8_records.inc"
};

static u16 _gActor151000Animation093C8Indices[20] = {
#include "assets/actor_151000_animation_093C8_indices.inc"
};

static AnimationSet _gActor151000Animation093C8 = {
    _gActor151000Animation093C8Records,
    _gActor151000Animation093C8Indices,
    { NULL, _gActor151000Animation093C8Bank1, NULL, NULL, _gActor151000Animation093C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation09620Bank1[2] = {
#include "assets/actor_151000_animation_09620_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation09620Bank4[30] = {
#include "assets/actor_151000_animation_09620_bank4.inc"
};

static AnimationRecord _gActor151000Animation09620Records[94] = {
#include "assets/actor_151000_animation_09620_records.inc"
};

static u16 _gActor151000Animation09620Indices[20] = {
#include "assets/actor_151000_animation_09620_indices.inc"
};

static AnimationSet _gActor151000Animation09620 = {
    _gActor151000Animation09620Records,
    _gActor151000Animation09620Indices,
    { NULL, _gActor151000Animation09620Bank1, NULL, NULL, _gActor151000Animation09620Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation098B0Bank1[2] = {
#include "assets/actor_151000_animation_098B0_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation098B0Bank4[48] = {
#include "assets/actor_151000_animation_098B0_bank4.inc"
};

static AnimationRecord _gActor151000Animation098B0Records[90] = {
#include "assets/actor_151000_animation_098B0_records.inc"
};

static u16 _gActor151000Animation098B0Indices[20] = {
#include "assets/actor_151000_animation_098B0_indices.inc"
};

static AnimationSet _gActor151000Animation098B0 = {
    _gActor151000Animation098B0Records,
    _gActor151000Animation098B0Indices,
    { NULL, _gActor151000Animation098B0Bank1, NULL, NULL, _gActor151000Animation098B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation09C98Bank1[5] = {
#include "assets/actor_151000_animation_09C98_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation09C98Bank4[59] = {
#include "assets/actor_151000_animation_09C98_bank4.inc"
};

static AnimationRecord _gActor151000Animation09C98Records[156] = {
#include "assets/actor_151000_animation_09C98_records.inc"
};

static u16 _gActor151000Animation09C98Indices[20] = {
#include "assets/actor_151000_animation_09C98_indices.inc"
};

static AnimationSet _gActor151000Animation09C98 = {
    _gActor151000Animation09C98Records,
    _gActor151000Animation09C98Indices,
    { NULL, _gActor151000Animation09C98Bank1, NULL, NULL, _gActor151000Animation09C98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation09F48Bank1[3] = {
#include "assets/actor_151000_animation_09F48_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation09F48Bank4[29] = {
#include "assets/actor_151000_animation_09F48_bank4.inc"
};

static AnimationRecord _gActor151000Animation09F48Records[114] = {
#include "assets/actor_151000_animation_09F48_records.inc"
};

static u16 _gActor151000Animation09F48Indices[20] = {
#include "assets/actor_151000_animation_09F48_indices.inc"
};

static AnimationSet _gActor151000Animation09F48 = {
    _gActor151000Animation09F48Records,
    _gActor151000Animation09F48Indices,
    { NULL, _gActor151000Animation09F48Bank1, NULL, NULL, _gActor151000Animation09F48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0A104Bank1[2] = {
#include "assets/actor_151000_animation_0A104_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0A104Bank4[20] = {
#include "assets/actor_151000_animation_0A104_bank4.inc"
};

static AnimationRecord _gActor151000Animation0A104Records[65] = {
#include "assets/actor_151000_animation_0A104_records.inc"
};

static u16 _gActor151000Animation0A104Indices[20] = {
#include "assets/actor_151000_animation_0A104_indices.inc"
};

static AnimationSet _gActor151000Animation0A104 = {
    _gActor151000Animation0A104Records,
    _gActor151000Animation0A104Indices,
    { NULL, _gActor151000Animation0A104Bank1, NULL, NULL, _gActor151000Animation0A104Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0A4ECBank1[3] = {
#include "assets/actor_151000_animation_0A4EC_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0A4ECBank4[65] = {
#include "assets/actor_151000_animation_0A4EC_bank4.inc"
};

static AnimationRecord _gActor151000Animation0A4ECRecords[156] = {
#include "assets/actor_151000_animation_0A4EC_records.inc"
};

static u16 _gActor151000Animation0A4ECIndices[20] = {
#include "assets/actor_151000_animation_0A4EC_indices.inc"
};

static AnimationSet _gActor151000Animation0A4EC = {
    _gActor151000Animation0A4ECRecords,
    _gActor151000Animation0A4ECIndices,
    { NULL, _gActor151000Animation0A4ECBank1, NULL, NULL, _gActor151000Animation0A4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0AE9CBank1[18] = {
#include "assets/actor_151000_animation_0AE9C_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0AE9CBank4[232] = {
#include "assets/actor_151000_animation_0AE9C_bank4.inc"
};

static AnimationRecord _gActor151000Animation0AE9CRecords[314] = {
#include "assets/actor_151000_animation_0AE9C_records.inc"
};

static u16 _gActor151000Animation0AE9CIndices[20] = {
#include "assets/actor_151000_animation_0AE9C_indices.inc"
};

static AnimationSet _gActor151000Animation0AE9C = {
    _gActor151000Animation0AE9CRecords,
    _gActor151000Animation0AE9CIndices,
    { NULL, _gActor151000Animation0AE9CBank1, NULL, NULL, _gActor151000Animation0AE9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0B218Bank1[4] = {
#include "assets/actor_151000_animation_0B218_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0B218Bank4[68] = {
#include "assets/actor_151000_animation_0B218_bank4.inc"
};

static AnimationRecord _gActor151000Animation0B218Records[123] = {
#include "assets/actor_151000_animation_0B218_records.inc"
};

static u16 _gActor151000Animation0B218Indices[20] = {
#include "assets/actor_151000_animation_0B218_indices.inc"
};

static AnimationSet _gActor151000Animation0B218 = {
    _gActor151000Animation0B218Records,
    _gActor151000Animation0B218Indices,
    { NULL, _gActor151000Animation0B218Bank1, NULL, NULL, _gActor151000Animation0B218Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor151000Animation0B464Bank1[2] = {
#include "assets/actor_151000_animation_0B464_bank1.inc"
};

static AnimationPackedRotation _gActor151000Animation0B464Bank4[27] = {
#include "assets/actor_151000_animation_0B464_bank4.inc"
};

static AnimationRecord _gActor151000Animation0B464Records[94] = {
#include "assets/actor_151000_animation_0B464_records.inc"
};

static u16 _gActor151000Animation0B464Indices[20] = {
#include "assets/actor_151000_animation_0B464_indices.inc"
};

static AnimationSet _gActor151000Animation0B464 = {
    _gActor151000Animation0B464Records,
    _gActor151000Animation0B464Indices,
    { NULL, _gActor151000Animation0B464Bank1, NULL, NULL, _gActor151000Animation0B464Bank4, NULL, NULL, NULL },
};

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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor151000SetWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor151000ApplyWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor151000WalkerTaskDesc = { { { TASK_BODY_TMD, ACTOR_151000_GARDEN_TASK_PRIORITY } }, _actor151000WalkerTask, { .model = &_gActor151000AyaBreaBody } };

/// Native clip-index table borrowed by the sound walker's animation context.
///
/// Loaded keys are 1..8, 10 and 13..19; every other entry is NULL. The
/// play handler's exclusive bound of 35 does not validate loaded entries.
/// Each selected set must provide tracks 1..18, and both this table and its
/// clip data must remain loaded throughout playback.
static AnimationSet* _gFootstepWalkAnimationSets[35] = {
    NULL,
    &_gActor151000Animation088A4,
    &_gActor151000Animation08CAC,
    &_gActor151000Animation090B8,
    &_gActor151000Animation093C8,
    &_gActor151000Animation09620,
    &_gActor151000Animation098B0,
    &_gActor151000Animation09C98,
    &_gActor151000Animation09F48,
    NULL,
    &_gActor151000Animation0A104,
    NULL,
    NULL,
    &_gActor151000Animation07660,
    &_gActor151000Animation07A58,
    &_gActor151000Animation0863C,
    &_gActor151000Animation0A4EC,
    &_gActor151000Animation0AE9C,
    &_gActor151000Animation0B464,
    &_gActor151000Animation0B218,
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
};

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

/// Queues a centered opaque black rectangle over the scene.
///
/// Covers 320 by 256 pixels starting at (-160, -128) relative to the draw origin.
/// Reserves `sizeof(TILE)` bytes without checking capacity; the current packet
/// arena must be word-aligned and have that space free. Ordering-table entry 10
/// must be writable. The frame owns the linked packet until GPU drawing finishes.
static __inline__ void _actor151000DrawBlackoutTile(void)
{
    enum {
        ACTOR_151000_BLACKOUT_WIDTH_PIXELS  = 320,
        ACTOR_151000_BLACKOUT_HEIGHT_PIXELS = 256,
        ACTOR_151000_BLACKOUT_OT_TAG        = 10
    };

    TILE* tile;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    tile->x0 = -ACTOR_151000_BLACKOUT_WIDTH_PIXELS / 2;
    tile->y0 = -ACTOR_151000_BLACKOUT_HEIGHT_PIXELS / 2;
    tile->w  = ACTOR_151000_BLACKOUT_WIDTH_PIXELS;
    tile->h  = ACTOR_151000_BLACKOUT_HEIGHT_PIXELS;
    addPrim(gGpuCurrentOt + ACTOR_151000_BLACKOUT_OT_TAG, tile);
}

/// Covers the scene in opaque black until its script clears the blackout switch.
///
/// Each enabled update queues one 320-by-256-pixel tile; there is no colour ramp
/// or countdown. The first disabled update kills this live task without drawing.
static void _actor151000BlackoutTask(Task* task)
{
    if (_gActor151000BlackoutEnabled != 0) {
        _actor151000DrawBlackoutTile();
    } else {
        taskKill(task);
    }
}

/// Switches the scene's opaque black cover on or off from an event script.
///
/// Zero disables drawing and lets existing cover tasks exit on their next update.
/// Every nonzero call stores the complete argument word and spawns another cover
/// task; callers normally enable once, then clear it on completion or skip.
/// The package and task descriptor must remain loaded while cover tasks are live.
static void _actor151000SetBlackout(s32 enabled)
{
    _gActor151000BlackoutEnabled = enabled;
    if (enabled != 0) {
        taskSpawnFromTable(&_gActor151000BlackoutTaskDesc, 0, 0, 0);
    }
}

#include "../../shared/walk/footstep_walk_spawn.inc.c"

/// Initializes or advances this package's sound-enabled scripted walker.
///
/// `task` must own a nineteen-part TMD model and a live `Enemy` in
/// `spawnArg2.pointer`. Task state 0 allocates and binds its work and enters
/// state 1; state 1 refreshes lighting, updates motion/animation and draws
/// the ground shadow. No other state is valid for this unchecked dispatch.
/// Publishes `task->work` for the singleton animation and message handlers;
/// state 0 replaces the initially NULL pointer before using those handlers.
/// The package, model, clip data and required scratch/GTE state must be live.
/// Initialization failure can release the enemy and begin task teardown, so
/// the task, enemy and published work must not be used after that failure.
static void _actor151000WalkerTask(Task* task)
{
    enum {
        ACTOR_151000_WALKER_STATE_SPAWN = 0,
        ACTOR_151000_WALKER_STATE_FRAME = 1
    };

    EnemyTaskFunc stateHandlers[] = {
        [ACTOR_151000_WALKER_STATE_SPAWN] = _footstepWalkSpawn,
        [ACTOR_151000_WALKER_STATE_FRAME] = _actorRenderWalkerFrame,
    };

    // Singleton helpers must see this task's work before its state runs.
    _gFootstepWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
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
#define ACTOR_RENDER_UPDATE_WALKER _footstepWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
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

/// Replaces the published walker's model flags for `ACTOR_MESSAGE_SET_MODEL_DRAW`.
///
/// Requires a live TMD model in `_gFootstepWalkTask`. Bit 0 permits active drawing;
/// without it the model is excluded. Bit 1 suppresses automatic buffer allocation.
/// All other model flags are cleared and other request bits are ignored. No buffer
/// is allocated or released. The receiver, message ID and second payload are
/// ignored. Returns 0.
static s32 _actor151000SetWalkerModelDraw(Task* unusedTask, s32 messageId, s32 flags, s32 unusedArgument)
{
    enum {
        ACTOR_151000_WALKER_DRAW_SHOW             = 1 << 0,
        ACTOR_151000_WALKER_DRAW_SKIP_AUTO_BUFFER = 1 << 1
    };

    TmdObject* model;

    model = _gFootstepWalkTask->extra.tmd;
    if (flags & ACTOR_151000_WALKER_DRAW_SHOW) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_151000_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/footstep_walk_place.inc.c"

/// Applies turn or footstep commands to the published walker.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` with live `_gFootstepWalkWork` and a
/// command borrowed only for this call. Command 0 schedules twenty turning
/// updates while the turn clip plays; it does not select that clip. Command 1
/// enables footstep sounds until work teardown. Other commands do nothing.
/// Context tags, receiver, message ID and second payload are ignored. Returns 0.
static s32 _actor151000ApplyWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum {
        ACTOR_151000_WALKER_COMMAND_TURN             = 0,
        ACTOR_151000_WALKER_COMMAND_ENABLE_FOOTSTEPS = 1,
        ACTOR_151000_WALKER_TURN_UPDATES             = 20
    };

    s32 commandId;

    commandId = command->command;
    switch (commandId) {
        case ACTOR_151000_WALKER_COMMAND_TURN:
            _gFootstepWalkWork->turnFrames = ACTOR_151000_WALKER_TURN_UPDATES;
            break;
        case ACTOR_151000_WALKER_COMMAND_ENABLE_FOOTSTEPS:
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

/// Selects this sound walker's private ground shadow definition with room shading.
///
/// Bind to one function identifier declared `static void (Task*)` in the prologue.
/// Use the same identifier for `ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW` when
/// including `walker_frame.inc.c` for this walker. This object-like alias takes
/// no arguments, captures no locals and uses no stringification or token pasting.
/// Define it only around `actor_render_walker_shadow.inc.c`, then undefine it;
/// further shadow instances need distinct function identifiers and prototypes.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW
