#include "rooms/dryfield_junk_yard.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor_00100.h"
#include "actors/actor_02500.h"

#include "gameplay/actor_render.h"
#include "gameplay/actor_render_shadow_types.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/game_debug.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

static s32 _junkYardCommandMessage(Task* task, s32 messageId, s32 command, s32 unusedSecondArg);

static void _dryfieldJunkYardDrawModelGroundShadow(Task* task);
static void _dryfieldJunkYardInitRoomTask(Task* task);
static void _dryfieldJunkYardIdleRoomTask(Task* unusedTask);

/// Initialization, idle and teardown handlers indexed by room-task state 0..2.
static const TaskFuncTable3 _gDryfieldJunkYardRoomTaskStates = {
    { _dryfieldJunkYardInitRoomTask, _dryfieldJunkYardIdleRoomTask, taskKill },
};

/// Companion label borrowed by the room's debug hook while this overlay is loaded.
///
/// The hook receives the literal "DOG"; its display or other use is unproven.
static const char _gDryfieldJunkYardCompanionDebugLabel[] = "DOG";

static void _dryfieldJunkYardCompanionSequenceTask(Task* task);
static s32  _dryfieldJunkYardRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _dryfieldJunkYardResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _dryfieldJunkYardHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArgument);

enum {
    DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT = 5,
};

/// Native companion bank and clip selections used by this room's scenes.
enum {
    DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK                 = 6,
    DRYFIELD_JUNK_YARD_COMPANION_IDLE_ANIMATION                 = 1,
    DRYFIELD_JUNK_YARD_COMPANION_REST_ANIMATION                 = 7,
    DRYFIELD_JUNK_YARD_COMPANION_REST_EXIT_ANIMATION            = 8,
    DRYFIELD_JUNK_YARD_COMPANION_REST_HOLD_ANIMATION            = 9,
    DRYFIELD_JUNK_YARD_COMPANION_ACTION_ANIMATION               = 10,
    DRYFIELD_JUNK_YARD_COMPANION_OPENING_ANIMATION              = 34,
    DRYFIELD_JUNK_YARD_COMPANION_OPENING_CONTINUATION_ANIMATION = 35,
};

/// CAP command slots and sound requests synchronized by the companion scripts.
enum {
    DRYFIELD_JUNK_YARD_CAP_COMPANION_FINISH      = 1,
    DRYFIELD_JUNK_YARD_CAP_COMPANION_RETURN      = 2,
    DRYFIELD_JUNK_YARD_CAP_COMPANION_OPENING     = 5,
    DRYFIELD_JUNK_YARD_CAP_COMPANION_DEPARTURE   = 7,
    DRYFIELD_JUNK_YARD_COMPANION_ACTION_SOUND    = 0x40720009,
    DRYFIELD_JUNK_YARD_COMPANION_DEPARTURE_SOUND = 0x521A0008,
};

/// Interpreter-update delays shared by the scene and skip streams.
enum {
    DRYFIELD_JUNK_YARD_SKIP_FADE_WAIT_FRAMES        = 8,
    DRYFIELD_JUNK_YARD_COMPANION_ACTION_HOLD_FRAMES = 38,
};

/// Door sound requests and map markers stored in the area's warp endpoints.
enum {
    DRYFIELD_JUNK_YARD_GARAGE_ARRIVAL_SOUND    = 0x521A0004,
    DRYFIELD_JUNK_YARD_GARAGE_DEPARTURE_SOUND  = 0x521A0003,
    DRYFIELD_JUNK_YARD_TRAILER_ARRIVAL_SOUND   = 0x521A0002,
    DRYFIELD_JUNK_YARD_TRAILER_DEPARTURE_SOUND = 0x521A0001,
    DRYFIELD_JUNK_YARD_WARP3_ARRIVAL_SOUND     = 0x521A0006,
    DRYFIELD_JUNK_YARD_WARP3_DEPARTURE_SOUND   = 0x521A0005,
    DRYFIELD_JUNK_YARD_GARAGE_MAP_MARKER       = 472,
    DRYFIELD_JUNK_YARD_TRAILER_MAP_MARKER      = 471,
};

/// Action-trigger payloads: room action IDs and packed departure/arrival warp slots.
enum {
    DRYFIELD_JUNK_YARD_TRIGGER_BEGIN_SEQUENCE  = 1,
    DRYFIELD_JUNK_YARD_TRIGGER_FINISH_SEQUENCE = 2,
    DRYFIELD_JUNK_YARD_GARAGE_WARP_SLOTS       = (1 << 4) | 2,
    DRYFIELD_JUNK_YARD_TRAILER_WARP_SLOTS      = (2 << 4) | 1,
};

static WorldCollisionGrid         _gDryfieldJunkYardCollisionGrid;
static WorldCollisionOccluder     _gDryfieldJunkYardSightOccluders[1];
static WorldCollisionTrigger      _gDryfieldJunkYardViewBoundaryTriggers[10];
static WorldCollisionTrigger      _gDryfieldJunkYardActionTriggers[19];
static WorldCoordRoomAmbientEntry _gDryfieldJunkYardAmbientByView[8];
static WorldCoordRoomLights       _gDryfieldJunkYardRoomLights;
static void                       _dryfieldJunkYardSetSceneEvent(s8 sceneEvent);

/// Room receiver's transition, key-item, dialogue and trigger-action messages.
///
/// The loaded overlay owns the table and callbacks. The final end-ID record
/// makes unsupported messages return zero; payload types follow each handler.
static TaskMessageEntry _gDryfieldJunkYardRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldJunkYardResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldJunkYardRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _junkYardCommandMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldJunkYardHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless companion-sequence controller at descriptor index 0, then an end marker.
///
/// Priority 32 orders it on the execution list. The task starts the opening
/// scene at state 0 with both spawn arguments zero. Keep the overlay loaded
/// until the task ends.
static TaskDesc _gDryfieldJunkYardCompanionSequenceTasks[] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _dryfieldJunkYardCompanionSequenceTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

enum {
    DRYFIELD_JUNK_YARD_PLAYER_IDLE_ANIMATION = 1,
    DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR  = 128,
    DRYFIELD_JUNK_YARD_WALK_FOOTSTEP_CUE_2   = 0x10000049,
    DRYFIELD_JUNK_YARD_RUN_FOOTSTEP_CUE_2    = 0x1000004B,
};

/// Holds the player in equipped-weapon idle while retaining world collision.
///
/// Scene scripts copy this overlay-owned request and replace its bank index
/// before synchronous playback. Clip 1 blends over eight normal-rate frames;
/// the stored bank index is not used by those scripts.
static AnimationPlayRequest _gDryfieldJunkYardPlayerIdleRequest = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_JUNK_YARD_PLAYER_IDLE_ANIMATION,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Restarts native companion idle after the finish scene, with world collision off.
///
/// Bank 6 and clip 1 are consumed synchronously; the loaded animation bank must
/// remain live through playback. Reset playback ignores the zero blend duration.
static AnimationPlayRequest _gDryfieldJunkYardCompanionIdleRequest = {
    .source               = { .index = DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK },
    .animationId          = DRYFIELD_JUNK_YARD_COMPANION_IDLE_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Selects opening clip 34 at room entry and at the opening scene's first cue.
///
/// Room initialization overwrites the bank index with the current companion's
/// selection before dispatch. The script reuses that stored index directly.
/// Playback resets without blending and disables world collision; the request
/// is consumed synchronously and the selected bank stays borrowed for playback.
static AnimationPlayRequest _gDryfieldJunkYardCompanionOpeningRequest = {
    .source               = { .index = DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK },
    .animationId          = DRYFIELD_JUNK_YARD_COMPANION_OPENING_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays opening clip 35 after a CAP cue, before the script waits for completion.
///
/// The interpreter copies the request and substitutes the current companion
/// bank. Playback resets without blending and disables world collision; keep
/// the overlay live until dispatch and the selected animation bank through playback.
static AnimationPlayRequest _gDryfieldJunkYardCompanionOpeningContinuationRequest = {
    .source               = { .index = DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK },
    .animationId          = DRYFIELD_JUNK_YARD_COMPANION_OPENING_CONTINUATION_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's rest-exit clip before its scene action.
///
/// The opening script substitutes the current companion bank in a copy; the
/// return scene uses bank 6 directly. Clip 8 resets without blending and world
/// collision stays off. The request is consumed synchronously; retain the bank for playback.
static AnimationPlayRequest _gDryfieldJunkYardCompanionRestExitRequest = {
    .source               = { .index = DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK },
    .animationId          = DRYFIELD_JUNK_YARD_COMPANION_REST_EXIT_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts the companion's player-directed action clip 10 alongside its sound cue.
///
/// The opening script substitutes the current companion bank in a copy; finish
/// and return scenes use bank 6 directly. Reset playback disables world collision.
/// Dispatch consumes the request synchronously and playback borrows the bank.
static AnimationPlayRequest _gDryfieldJunkYardCompanionAction10Request = {
    .source               = { .index = DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK },
    .animationId          = DRYFIELD_JUNK_YARD_COMPANION_ACTION_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Starts native companion rest after the controller's scripted yaw turn.
///
/// Clip 7 in bank 6 resets without blending and disables world collision. The
/// controller waits for playback before selecting rest hold. Dispatch consumes
/// the request synchronously; the bank remains borrowed through playback.
static AnimationPlayRequest _gDryfieldJunkYardCompanionRestRequest = {
    .source               = { .index = DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK },
    .animationId          = DRYFIELD_JUNK_YARD_COMPANION_REST_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Holds native companion rest after the turn sequence or skipped opening scene.
///
/// Also sets the return scene's initial rest pose. Clip 9 in bank 6 resets without
/// blending and disables world collision. Dispatch consumes this overlay-owned
/// request synchronously; the selected animation bank must outlive playback.
static AnimationPlayRequest _gDryfieldJunkYardCompanionRestHoldRequest = {
    .source               = { .index = DRYFIELD_JUNK_YARD_COMPANION_ANIMATION_BANK },
    .animationId          = DRYFIELD_JUNK_YARD_COMPANION_REST_HOLD_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Companion placement for initial room entry and the normal or skipped opening scene.
///
/// Position uses whole room-coordinate units; yaw uses 4096 units per turn.
/// Placement consumes both vectors synchronously; the overlay owns this record.
static ActorTransform _gDryfieldJunkYardCompanionOpeningPlacement = {
    .pos = { 0x288B, 0, 3893, 0 },
    .rot = { 0, 2700, 0, 0 },
};

/// Target yaw before the companion's rest animation, in 4096 units per turn.
///
/// The turn message consumes only rot.vy, synchronously. The retained position
/// and other rotation components have no consumer in this room.
static ActorTransform _gDryfieldJunkYardCompanionRestFacing = {
    .pos = { 0x530A, 0, 3407, 0 },
    .rot = { 0, 3074, 0, 0 },
};

/// Companion destination beside the trailer coach for finish, return and arrival 2.
///
/// Position uses whole room-coordinate units; yaw is a half-turn (2048).
/// Placement messages consume both vectors; the finish scene's turn message
/// reads only yaw. The overlay-owned record is borrowed through dispatch.
static ActorTransform _gDryfieldJunkYardCompanionTrailerPlacement = {
    .pos = { 0x54C4, 0, 3800, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 },
};

/// Runs the opening companion scene for room action 1.
///
/// CAP command 5 cues opening clips 34/35, rest exit and action clip 10, then
/// restores the saved view and releases both actors from scripted control.
/// Zero-frame waits still yield one update before polling animation. Commands
/// and referenced overlay payloads remain borrowed until the final END.
static EvsCommand _gDryfieldJunkYardCompanionOpeningScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_JUNK_YARD_CAP_COMPANION_OPENING }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardPlayerIdleRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldJunkYardCompanionOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionOpeningRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardCompanionOpeningContinuationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardCompanionRestExitRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardCompanionAction10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_JUNK_YARD_COMPANION_ACTION_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Completes a skipped opening scene under the primary fade.
///
/// Places the companion at its opening position in rest hold, restores the
/// saved view, cleans up CAP and releases both actors after fading back.
/// The overlay-owned stream and its payloads must stay live through END.
static EvsCommand _gDryfieldJunkYardCompanionOpeningSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardPlayerIdleRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldJunkYardCompanionOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionRestHoldRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the companion finish scene once room action 2 reaches its X threshold.
///
/// CAP command 1 accompanies a yaw turn toward the trailer placement, action
/// clip 10 and idle. Releases player control after the CAP cue; companion
/// control stays scripted. The overlay and animation bank must remain loaded.
static EvsCommand _gDryfieldJunkYardCompanionFinishScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_JUNK_YARD_CAP_COMPANION_FINISH }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardPlayerIdleRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gDryfieldJunkYardCompanionTrailerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_JUNK_YARD_COMPANION_ACTION_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionAction10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_COMPANION_ACTION_HOLD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionIdleRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Completes either skipped finish scene or skipped trailer-departure scene.
///
/// Under a primary fade, places the companion beside the trailer in idle,
/// selects view 5, cleans up CAP and releases the player. Companion control
/// remains scripted. The borrowed overlay stream ends with END.
static EvsCommand _gDryfieldJunkYardCompanionFinishSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardPlayerIdleRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldJunkYardCompanionTrailerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionIdleRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the final companion scene when departure for the trailer is intercepted.
///
/// CAP command 7 and the room sound play before player control is restored.
/// Uses the finish skip stream; it does not commit a room transition itself.
/// Commands and callback code remain in the loaded overlay through END.
static EvsCommand _gDryfieldJunkYardCompanionDepartureScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_JUNK_YARD_CAP_COMPANION_DEPARTURE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardPlayerIdleRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_JUNK_YARD_COMPANION_DEPARTURE_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs the first companion return scene after trailer-coach progress reaches 2.
///
/// CAP command 2 cues rest hold, rest exit and action beside the trailer.
/// A secondary fade covers entry; both actors resume before scene event 5
/// is stored. Area music is requested before that store on the normal path.
/// Commands, payloads and callback code must stay loaded through END.
static EvsCommand _gDryfieldJunkYardCompanionReturnScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = DRYFIELD_JUNK_YARD_CAP_COMPANION_RETURN }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gDryfieldJunkYardPlayerIdleRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gDryfieldJunkYardCompanionTrailerPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionRestHoldRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionRestExitRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = DRYFIELD_JUNK_YARD_COMPANION_ACTION_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .message = { .animation = &_gDryfieldJunkYardCompanionAction10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_COMPANION_ACTION_HOLD_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _dryfieldJunkYardSetSceneEvent }, { .value = DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Cleans up a skipped return scene and records its scene event under a fade.
///
/// Stores scene event 5 before requesting area music, then fades back and
/// releases both actors. It retains the companion pose reached before skipping.
/// The overlay-owned stream and callback code must remain loaded through END.
static EvsCommand _gDryfieldJunkYardCompanionReturnSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _dryfieldJunkYardSetSceneEvent }, { .value = DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = DRYFIELD_JUNK_YARD_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gDryfieldJunkYardModel01378Skeleton[1] = {
#include "assets/dryfield_junk_yard_model_01378_skeleton.inc"
};

static u32 _gDryfieldJunkYardModel01378PartVerts[1] = {
#include "assets/dryfield_junk_yard_model_01378_partVerts.inc"
};

static SVECTOR _gDryfieldJunkYardModel01378Verts[54] = {
#include "assets/dryfield_junk_yard_model_01378_verts.inc"
};

static u32 _gDryfieldJunkYardModel01378Stream[234] = {
#include "assets/dryfield_junk_yard_model_01378_stream.inc"
};

TmdSource gDryfieldJunkYardModel01378 = {
    0,
    1640,
    0,
    1,
    _gDryfieldJunkYardModel01378PartVerts,
    _gDryfieldJunkYardModel01378Verts,
    &_gDryfieldJunkYardModel01378Verts[54],
    _gDryfieldJunkYardModel01378Skeleton,
    _gDryfieldJunkYardModel01378Stream,
};

WorldCollisionRoomResources gDryfieldJunkYardRoomCollisionResources[1] = {
    {
        .grid                 = &_gDryfieldJunkYardCollisionGrid,
        .viewBoundaryTriggers = _gDryfieldJunkYardViewBoundaryTriggers,
        .actionTriggers       = _gDryfieldJunkYardActionTriggers,
        .occluders            = _gDryfieldJunkYardSightOccluders,
    },
};

WorldCoordRoomLighting gDryfieldJunkYardRoomLighting[1] = {
    { .lights = &_gDryfieldJunkYardRoomLights, .ambientTable = _gDryfieldJunkYardAmbientByView },
};

u8* gDryfieldJunkYardRoomViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gDryfieldJunkYardRoomViewCounts[1] = { ARRAY_SIZE(gDryfieldJunkYardViewCameras) };

DirectionWarpEntry gDryfieldJunkYardWarpEntries[3] = {
    {
        .player         = { { .word = 0 }, 5030, 0, 390 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 4800, 0, 1000 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_JUNK_YARD_GARAGE_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_JUNK_YARD_GARAGE_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_JUNK_YARD_GARAGE_MAP_MARKER,
    },
    {
        .player         = { { .word = 0 }, 0x55A1, 0, 1639 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 0x55A1, 0, 1639 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_JUNK_YARD_TRAILER_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_JUNK_YARD_TRAILER_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_JUNK_YARD_TRAILER_MAP_MARKER,
    },
    {
        .player         = { { .word = 0 }, 1859, 0, 672 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 0 }, 1859, 0, 672 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = DRYFIELD_JUNK_YARD_WARP3_ARRIVAL_SOUND,
        .departureSound = DRYFIELD_JUNK_YARD_WARP3_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 3,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Room-grid plane normals in 4096-per-unit coordinates, indexed by each face.
static SVECTOR _gDryfieldJunkYardCollisionGridNormals[] = {
#include "assets/dryfield_junk_yard_collision_01F08_normals.inc"
};

/// Room-grid vertices in whole signed room-coordinate units.
static SVECTOR _gDryfieldJunkYardCollisionGridVerts[] = {
#include "assets/dryfield_junk_yard_collision_01F08_verts.inc"
};

/// Room-grid quads; face 0 uses footstep surface class 1, all others class 0.
static WorldCollisionGridFace _gDryfieldJunkYardCollisionGridFaces[] = {
#include "assets/dryfield_junk_yard_collision_01F08_faces.inc"
};

/// Cell face-index lists ending at -1, with retained zero words between lists.
static s16 _gDryfieldJunkYardCollisionGridCells[] = {
#include "assets/dryfield_junk_yard_collision_01F08_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gDryfieldJunkYardCollisionGridCells` (166 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define DRYFIELD_JUNK_YARD_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldJunkYardCollisionGridCells[(halfwordOffset)])

/// Fourteen X-major cell lists for the seven-by-two room grid.
static s16* _gDryfieldJunkYardCollisionGridTable[] = {
#include "assets/dryfield_junk_yard_collision_01F08_table.inc"
};
#undef DRYFIELD_JUNK_YARD_CELL_FACE_LIST_AT_OFFSET

/// The junk yard's sole collision mesh and its seven-by-two XZ cell index.
///
/// Cell side length is 4000 room-coordinate units; biases 2400 and 608 put the
/// grid origin at (-2400, -608). Face indices address all 36 quads, with 88
/// vertices and 33 normals. Setup supplies the current view coordinate.
/// The descriptor and mutable pools belong to this loaded overlay and must
/// outlive active-grid queries; no room-specific geometry edits are present.
static WorldCollisionGrid _gDryfieldJunkYardCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldJunkYardCollisionGridNormals,
    .vertices    = _gDryfieldJunkYardCollisionGridVerts,
    .faces       = _gDryfieldJunkYardCollisionGridFaces,
    .cellFaceIds = _gDryfieldJunkYardCollisionGridTable,
    .xBias       = 2400,
    .zBias       = 608,
    .cellCountX  = 7,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldJunkYardCollisionGridFaces),
};

/// Empty resource list for placement variant 1.
///
/// Its sole terminator pairs with the map overlay's empty placement list.
/// Area loading and spawning borrow this list while the room overlay is loaded.
static AreaResource _gDryfieldJunkYardVariant1Resources[] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Actor-102500 resource recipe for the four placements in variant 2.
///
/// Entry 25 loads global-library file 102500 and spawns task descriptor 0.
/// The live recipe precedes an end marker; this overlay and the borrowed actor
/// descriptor table must remain loaded during area loading and spawning.
static AreaResource _gDryfieldJunkYardVariant2Resources[] = {
    { .entryId = 25, .fileNumber = 25, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102500Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Desert Chaser resource recipe for the two placements in variant 10.
///
/// Entry 1 loads global-library file 400100 and uses the borrowed Desert Chaser
/// descriptor. The live recipe precedes an end marker; both overlays must
/// remain loaded during area loading and spawning.
static AreaResource _gDryfieldJunkYardVariant10Resources[] = {
    { .entryId = 1, .fileNumber = 1, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = &gActor00100DesertChaserTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Two initial Desert Chaser poses for placement variant 10, then an end marker.
///
/// Both select resource entry 1, actor variant/mode/row 0 and CLUT-row offset 2.
/// Positions use world units and yaw uses 4096 units per turn. Spawned actors
/// borrow these placement records, which must not outlive the room overlay.
static AreaPlacement _gDryfieldJunkYardVariant10Placements[] = {
    { .entryId = 1, .x = 5685, .z = 2272, .yaw = 4096, .clutRowOffset = 2 },
    { .entryId = 1, .x = 5485, .z = 2072, .yaw = 4352, .clutRowOffset = 2 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gDryfieldJunkYardAreaVariants[13] = {
    [1]  = { .placements = gMapDryfieldJunkYardVariant1Placements, .resources = _gDryfieldJunkYardVariant1Resources },
    [2]  = { .placements = gMapDryfieldJunkYardVariant2Placements, .resources = _gDryfieldJunkYardVariant2Resources },
    [10] = { .placements = _gDryfieldJunkYardVariant10Placements, .resources = _gDryfieldJunkYardVariant10Resources },
};

ViewCamera gDryfieldJunkYardViewCameras[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2DE6, 0x61A8, -5750 } }, 257 },
    { { { { 965, 0, -3980 }, { -535, 4058, -129 }, { 3944, 550, 956 } }, { 1022, 2299, -989 } }, 282 },
    { { { { 783, 0, -4020 }, { -3412, 2165, -665 }, { 2125, 3476, 414 } }, { 1120, 4985, -2136 } }, 239 },
    { { { { 1009, 0, -3969 }, { -1187, 3908, -301 }, { 3788, 1224, 962 } }, { -2958, 2969, -1227 } }, 282 },
    { { { { 66, 0, 4095 }, { 1199, 3916, -19 }, { -3915, 1199, 63 } }, { -0x63AC, 2717, -2429 } }, 289 },
    { { { { 1933, 0, -3610 }, { -957, 3949, -512 }, { 3481, 1085, 1864 } }, { -8412, 1167, -2987 } }, 257 },
    { { { { 1162, 0, 3927 }, { 1136, 3920, -336 }, { -3759, 1185, 1113 } }, { -0x4970, 2301, -1670 } }, 269 },
};

/// Empty sprite list for view 1, preserving decoded background strips.
///
/// The zero-count first record is followed by an end marker. The view retains
/// this overlay-owned list in both its batch pointer and unused source slot.
static SpriteBatch _gDryfieldJunkYardView1SpriteBatches[] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Thirty texture rectangles and sorting depths for mapped view 2.
///
/// Overlay-owned mutable sources: XY is relative to the view centre, dimensions
/// use pixels, UVs use texels, and depths are unscaled OT values. Batches cover every element;
/// cached packet geometry is a snapshot, while linking reads current depths.
static SpriteSource _gDryfieldJunkYardView2SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -40, -104, 3066, { .fields = { 32, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 24, -72, 6500, { .fields = { 104, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, 56, -112, 5000, { .fields = { 16, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, 64, 1304, { .fields = { 88, 56 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -96, -32, 1248, { .fields = { 112, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 1068, { .fields = { 96, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -88, 48, 1249, { .fields = { 96, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 1146, { .fields = { 16, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, -160, 48, 937, { .fields = { 104, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 1038, { .fields = { 64, 248 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, 40, 988, { .fields = { 8, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 40, 1281, { .fields = { 104, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1249, { .fields = { 32, 248 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -112, 56, 1036, { .fields = { 96, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -104, 56, 1074, { .fields = { 96, 144 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 64, 1281, { .fields = { 104, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 1282, { .fields = { 112, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -24, 4346, { .fields = { 72, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -16, 4080, { .fields = { 56, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -8, 3816, { .fields = { 72, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 64, 1172, { .fields = { 16, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 1189, { .fields = { 24, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 120, 72, 1150, { .fields = { 88, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 72, 1160, { .fields = { 120, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -16, 3495, { .fields = { 72, 32 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -40, 3638, { .fields = { 0, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, -72, 3625, { .fields = { 120, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 88, -104, 3625, { .fields = { 56, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, 96, 24, 3500, { .fields = { 64, 144 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 64, 144 } }, 96, -120, 3500, { .fields = { 64, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Five sprite ranges for view 2, after a background-strip selector.
///
/// Ranges partition all 30 sources, then an end marker stops the walk. Hidden
/// and exclusion flags start clear; the retained tail bytes have unproven roles.
/// The mutable list remains borrowed for the loaded overlay's drawing lifetime.
static SpriteBatch _gDryfieldJunkYardView2SpriteBatches[] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 3, .field_6 = { 0, 0 } },
    { .firstSprite = 3, .spriteCount = 14, .field_6 = { 3, 0 } },
    { .firstSprite = 17, .spriteCount = 3, .field_6 = { 2, 0 } },
    { .firstSprite = 20, .spriteCount = 4, .field_6 = { 4, 0 } },
    { .firstSprite = 24, .spriteCount = ARRAY_SIZE(_gDryfieldJunkYardView2SpriteSources) - 24, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Sixty texture rectangles and sorting depths for mapped view 3.
///
/// Overlay-owned mutable sources: XY is relative to the view centre, dimensions
/// use pixels, UVs use texels, and depths are unscaled OT values. Batches cover every element;
/// cached packet geometry is a snapshot, while linking reads current depths.
static SpriteSource _gDryfieldJunkYardView3SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -104, 0, 1538, { .fields = { 16, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, 0, 1522, { .fields = { 48, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -64, -56, 1522, { .fields = { 80, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -104, -56, 1538, { .fields = { 88, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -64, -120, 1372, { .fields = { 40, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -104, -120, 1388, { .fields = { 40, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, -80, 1541, { .fields = { 16, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, -72, 1541, { .fields = { 24, 184 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -104, -80, 1528, { .fields = { 0, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -64, 1558, { .fields = { 64, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -56, 1577, { .fields = { 64, 208 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -48, 1578, { .fields = { 24, 16 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 56, 984, { .fields = { 80, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 64, 1124, { .fields = { 56, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 64, 1038, { .fields = { 72, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 64, 976, { .fields = { 32, 8 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 955, { .fields = { 80, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 72, 951, { .fields = { 48, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 952, { .fields = { 32, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 949, { .fields = { 32, 24 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 951, { .fields = { 32, 32 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 80, 942, { .fields = { 32, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 80, 937, { .fields = { 32, 136 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 934, { .fields = { 32, 144 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 930, { .fields = { 32, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 931, { .fields = { 24, 8 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 88, 929, { .fields = { 24, 24 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 88, 923, { .fields = { 24, 32 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 88, 920, { .fields = { 24, 208 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 88, 915, { .fields = { 24, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 88, 916, { .fields = { 24, 56 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 918, { .fields = { 24, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 96, 916, { .fields = { 24, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 906, { .fields = { 24, 48 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 96, 903, { .fields = { 24, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 900, { .fields = { 24, 136 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 903, { .fields = { 24, 144 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 904, { .fields = { 24, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 918, { .fields = { 24, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 898, { .fields = { 16, 8 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 888, { .fields = { 24, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 884, { .fields = { 16, 24 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 885, { .fields = { 64, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 890, { .fields = { 16, 16 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 963, { .fields = { 16, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 938, { .fields = { 16, 56 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 931, { .fields = { 16, 48 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 887, { .fields = { 16, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 878, { .fields = { 16, 32 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 886, { .fields = { 24, 224 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, 24, 950, { .fields = { 32, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 56, 56, 1134, { .fields = { 80, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -136, 24, 1220, { .fields = { 0, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 32, 1270, { .fields = { 24, 104 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 48, 1196, { .fields = { 64, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, 56, 1174, { .fields = { 80, 176 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 64, 1158, { .fields = { 64, 224 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 72, 1142, { .fields = { 96, 224 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 48, 1184, { .fields = { 48, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 48, 1200, { .fields = { 32, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Three sprite ranges for view 3, after a background-strip selector.
///
/// Ranges partition all 60 sources, then an end marker stops the walk. Hidden
/// and exclusion flags start clear; the retained tail bytes have unproven roles.
/// The mutable list remains borrowed for the loaded overlay's drawing lifetime.
static SpriteBatch _gDryfieldJunkYardView3SpriteBatches[] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 9, .field_6 = { 1, 0 } },
    { .firstSprite = 9, .spriteCount = 3, .field_6 = { 2, 0 } },
    { .firstSprite = 12, .spriteCount = ARRAY_SIZE(_gDryfieldJunkYardView3SpriteSources) - 12, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Twenty-five texture rectangles and sorting depths for mapped view 4.
///
/// Overlay-owned mutable sources: XY is relative to the view centre, dimensions
/// use pixels, UVs use texels, and depths are unscaled OT values. Batches cover every element;
/// cached packet geometry is a snapshot, while linking reads current depths.
static SpriteSource _gDryfieldJunkYardView4SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 2084, { .fields = { 32, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 0, 2125, { .fields = { 48, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 16, 2042, { .fields = { 24, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 2069, { .fields = { 96, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 2101, { .fields = { 112, 112 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -40, 2069, { .fields = { 72, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 2069, { .fields = { 112, 136 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -88, 3836, { .fields = { 104, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -120, 3836, { .fields = { 104, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -80, 6000, { .fields = { 120, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -112, 6000, { .fields = { 112, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, -56, 3550, { .fields = { 16, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 32, -48, 3452, { .fields = { 0, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, -40, 3366, { .fields = { 120, 56 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -32, 2914, { .fields = { 40, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -32, 2838, { .fields = { 104, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -32, 3473, { .fields = { 56, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 2857, { .fields = { 32, 24 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 104, -120, 2500, { .fields = { 120, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -72, 2750, { .fields = { 56, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -32, 2545, { .fields = { 56, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, 40, 2500, { .fields = { 80, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -16, 2500, { .fields = { 56, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -72, 2500, { .fields = { 56, 136 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 2500, { .fields = { 56, 192 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Three sprite ranges for view 4, after a background-strip selector.
///
/// Ranges partition all 25 sources, then an end marker stops the walk. Hidden
/// and exclusion flags start clear; the retained tail bytes have unproven roles.
/// The mutable list remains borrowed for the loaded overlay's drawing lifetime.
static SpriteBatch _gDryfieldJunkYardView4SpriteBatches[] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 11, .field_6 = { 1, 0 } },
    { .firstSprite = 11, .spriteCount = 7, .field_6 = { 2, 0 } },
    { .firstSprite = 18, .spriteCount = ARRAY_SIZE(_gDryfieldJunkYardView4SpriteSources) - 18, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Ninety-seven texture rectangles and sorting depths for mapped view 5.
///
/// Overlay-owned mutable sources: XY is relative to the view centre, dimensions
/// use pixels, UVs use texels, and depths are unscaled OT values. Batches cover every element;
/// cached packet geometry is a snapshot, while linking reads current depths.
static SpriteSource _gDryfieldJunkYardView5SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -112, 8250, { .fields = { 72, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -96, -48, 1825, { .fields = { 80, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -120, 1825, { .fields = { 80, 88 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 4051, { .fields = { 32, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 4051, { .fields = { 120, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 8, -88, 7006, { .fields = { 112, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2314, { .fields = { 16, 96 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2762, { .fields = { 16, 88 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2398, { .fields = { 48, 112 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2533, { .fields = { 64, 248 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2312, { .fields = { 64, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -16, 2122, { .fields = { 40, 112 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1963, { .fields = { 16, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 0, 1825, { .fields = { 32, 56 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 8, 1720, { .fields = { 32, 48 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, 16, 1627, { .fields = { 72, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -40, 2182, { .fields = { 96, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -32, 1941, { .fields = { 96, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -64, -24, 1907, { .fields = { 104, 144 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, -16, 1862, { .fields = { 80, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -72, -8, 1785, { .fields = { 64, 8 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -80, 0, 1689, { .fields = { 56, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 2039, { .fields = { 88, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, -80, 8, 1673, { .fields = { 104, 96 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 1250, { .fields = { 16, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1256, { .fields = { 16, 104 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -48, 1372, { .fields = { 48, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, 0, 1267, { .fields = { 24, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, 16, 1379, { .fields = { 72, 192 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 40, 1314, { .fields = { 64, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 0, 1388, { .fields = { 64, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 1250, { .fields = { 72, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 1250, { .fields = { 56, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 1250, { .fields = { 88, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 1250, { .fields = { 80, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 1250, { .fields = { 96, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 1250, { .fields = { 24, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 1250, { .fields = { 16, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 1250, { .fields = { 8, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 1250, { .fields = { 8, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 1250, { .fields = { 16, 112 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -120, 1290, { .fields = { 104, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -80, 1290, { .fields = { 8, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 1290, { .fields = { 8, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 0, 1290, { .fields = { 8, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 569, { .fields = { 96, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -88, 24, 1275, { .fields = { 40, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, 40, 1250, { .fields = { 48, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 40, 1253, { .fields = { 24, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -112, 72, 1131, { .fields = { 112, 208 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 80, 1079, { .fields = { 104, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 88, 1032, { .fields = { 112, 224 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 96, 610, { .fields = { 112, 16 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1521, { .fields = { 88, 248 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 120, -16, 1266, { .fields = { 104, 184 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 96, -8, 1584, { .fields = { 120, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -8, 1200, { .fields = { 24, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 128, 16, 1200, { .fields = { 112, 192 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 1050, { .fields = { 72, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 1075, { .fields = { 72, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 48, 974, { .fields = { 88, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, 48, 931, { .fields = { 96, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, 40, 1035, { .fields = { 96, 88 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 128, 24, 1102, { .fields = { 120, 136 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 16, 1203, { .fields = { 56, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 64, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 8, 1376, { .fields = { 40, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 0, 1476, { .fields = { 32, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 8, 1618, { .fields = { 24, 192 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 80, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 120, -16, 1237, { .fields = { 104, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 128, -16, 1151, { .fields = { 120, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 136, -8, 1067, { .fields = { 112, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 144, -8, 990, { .fields = { 112, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -8, 959, { .fields = { 104, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -48, 2676, { .fields = { 8, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -48, 2513, { .fields = { 16, 192 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -56, 2167, { .fields = { 64, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, -56, 2248, { .fields = { 8, 24 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 72, -48, 2192, { .fields = { 96, 240 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 72, -32, 2202, { .fields = { 104, 176 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 80, -24, 2196, { .fields = { 104, 32 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -24, 2034, { .fields = { 0, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -16, 1882, { .fields = { 80, 224 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -16, 1887, { .fields = { 8, 40 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -152, 96, 575, { .fields = { 16, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 575, { .fields = { 0, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 575, { .fields = { 112, 72 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 575, { .fields = { 120, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 600, { .fields = { 32, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 575, { .fields = { 16, 208 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, 96, 500, { .fields = { 16, 184 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, 64, 575, { .fields = { 40, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 2394, { .fields = { 120, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -48, 2252, { .fields = { 64, 184 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -56, 2177, { .fields = { 72, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -80, -72, 2583, { .fields = { 40, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Ten sprite ranges for view 5, after a background-strip selector.
///
/// Ranges partition all 97 sources, then an end marker stops the walk. Hidden
/// and exclusion flags start clear; the retained tail bytes have unproven roles.
/// The mutable list remains borrowed for the loaded overlay's drawing lifetime.
static SpriteBatch _gDryfieldJunkYardView5SpriteBatches[] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 5, .field_6 = { 6, 0 } },
    { .firstSprite = 5, .spriteCount = 1, .field_6 = { 1, 0 } },
    { .firstSprite = 6, .spriteCount = 18, .field_6 = { 8, 0 } },
    { .firstSprite = 24, .spriteCount = 29, .field_6 = { 0, 0 } },
    { .firstSprite = 53, .spriteCount = 16, .field_6 = { 5, 0 } },
    { .firstSprite = 69, .spriteCount = 6, .field_6 = { 2, 0 } },
    { .firstSprite = 75, .spriteCount = 10, .field_6 = { 9, 0 } },
    { .firstSprite = 85, .spriteCount = 8, .field_6 = { 4, 0 } },
    { .firstSprite = 93, .spriteCount = 3, .field_6 = { 7, 0 } },
    { .firstSprite = 96, .spriteCount = ARRAY_SIZE(_gDryfieldJunkYardView5SpriteSources) - 96, .field_6 = { 3, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite list for view 6, preserving decoded background strips.
///
/// The zero-count first record is followed by an end marker. The view retains
/// this overlay-owned list in both its batch pointer and unused source slot.
static SpriteBatch _gDryfieldJunkYardView6SpriteBatches[] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Forty-seven texture rectangles and sorting depths for mapped view 7.
///
/// Overlay-owned mutable sources: XY is relative to the view centre, dimensions
/// use pixels, UVs use texels, and depths are unscaled OT values. Batches cover every element;
/// cached packet geometry is a snapshot, while linking reads current depths.
static SpriteSource _gDryfieldJunkYardView7SpriteSources[] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 719, { .fields = { 96, 112 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 64, 553, { .fields = { 80, 96 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -56, -88, 4064, { .fields = { 64, 112 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -120, 2402, { .fields = { 112, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -32, 2312, { .fields = { 96, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -32, 2340, { .fields = { 72, 88 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 8, 1222, { .fields = { 48, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 1098, { .fields = { 0, 248 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -64, 16, 1209, { .fields = { 24, 232 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -120, 16, 1177, { .fields = { 8, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -128, 32, 1056, { .fields = { 120, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -80, 32, 657, { .fields = { 72, 208 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 40, 593, { .fields = { 104, 128 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -136, 40, 597, { .fields = { 104, 144 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 56, 718, { .fields = { 112, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 56, 530, { .fields = { 104, 16 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, -144, 56, 540, { .fields = { 96, 32 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -48, 72, 487, { .fields = { 80, 112 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 72, 491, { .fields = { 72, 48 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -40, 88, 447, { .fields = { 64, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -160, 88, 447, { .fields = { 64, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 128, 16 } }, -32, 104, 408, { .fields = { 56, 80 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 142, 0x3FC0, { .fields = { 128, 16 } }, -160, 104, 411, { .fields = { 56, 96 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -8, 1363, { .fields = { 120, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -8, 1312, { .fields = { 104, 64 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, -8, 1246, { .fields = { 112, 88 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 0, 1148, { .fields = { 96, 120 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1347, { .fields = { 24, 200 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, -16, 1388, { .fields = { 16, 184 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -24, 1012, { .fields = { 56, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, -8, 1146, { .fields = { 16, 192 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 0, 1078, { .fields = { 112, 152 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -8, 995, { .fields = { 72, 216 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 32, 1004, { .fields = { 72, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -24, 2186, { .fields = { 56, 88 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 0, 1521, { .fields = { 48, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 16, 1234, { .fields = { 56, 48 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -8, 1283, { .fields = { 16, 176 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 0, 1243, { .fields = { 88, 56 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 1243, { .fields = { 88, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, -16, 1314, { .fields = { 32, 248 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -8, 1300, { .fields = { 104, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -64, 1156, { .fields = { 40, 160 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -56, 1031, { .fields = { 24, 168 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -48, 1122, { .fields = { 96, 176 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 8, 1084, { .fields = { 64, 0 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 56, 960, { .fields = { 80, 48 } }, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, DRYFIELD_JUNK_YARD_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Three sprite ranges for view 7, after a background-strip selector.
///
/// Ranges partition all 47 sources, then an end marker stops the walk. Hidden
/// and exclusion flags start clear; the retained tail bytes have unproven roles.
/// The mutable list remains borrowed for the loaded overlay's drawing lifetime.
static SpriteBatch _gDryfieldJunkYardView7SpriteBatches[] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 34, .field_6 = { 1, 0 } },
    { .firstSprite = 34, .spriteCount = 8, .field_6 = { 2, 0 } },
    { .firstSprite = 42, .spriteCount = ARRAY_SIZE(_gDryfieldJunkYardView7SpriteSources) - 42, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gDryfieldJunkYardSpriteViews[7] = {
    { { .empty = _gDryfieldJunkYardView1SpriteBatches }, _gDryfieldJunkYardView1SpriteBatches, NULL },
    { { .elements = _gDryfieldJunkYardView2SpriteSources }, _gDryfieldJunkYardView2SpriteBatches, NULL },
    { { .elements = _gDryfieldJunkYardView3SpriteSources }, _gDryfieldJunkYardView3SpriteBatches, NULL },
    { { .elements = _gDryfieldJunkYardView4SpriteSources }, _gDryfieldJunkYardView4SpriteBatches, NULL },
    { { .elements = _gDryfieldJunkYardView5SpriteSources }, _gDryfieldJunkYardView5SpriteBatches, NULL },
    { { .empty = _gDryfieldJunkYardView6SpriteBatches }, _gDryfieldJunkYardView6SpriteBatches, NULL },
    { { .elements = _gDryfieldJunkYardView7SpriteSources }, _gDryfieldJunkYardView7SpriteBatches, NULL },
};

/// Ten directed camera boundaries between views 2..5 and 7.
///
/// Each source/destination pair is 1-based. The last record participates before
/// LAST stops setup; links, coordinate bindings and hit latches remain mutable.
/// Quad positions/radii use room units and normals use 4096 per unit. Keep the
/// owning overlay loaded until the view-boundary list is unlinked.
static WorldCollisionTrigger _gDryfieldJunkYardViewBoundaryTriggers[10] = {
    { NULL, NULL, NULL, { 3568, -1408, 2448, 0 }, { { 467, -2432, -3264, 0 }, { -494, -2432, 3235, 0 }, { 467, 2432, -3264, 0 }, { -494, 2432, 3235, 0 } }, { 4052, 0, 599, 0 }, { 0, 0, 4096, 0 }, 4087, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3680, -1376, 2336, 0 }, { { -517, -2400, 3225, 0 }, { 476, -2400, -3264, 0 }, { -517, 2400, 3225, 0 }, { 476, 2400, -3264, 0 } }, { -4056, 0, -621, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7324, -1328, 2299, 0 }, { { 335, -2352, -3268, 0 }, { -337, -2352, 3267, 0 }, { 335, 2352, -3268, 0 }, { -337, 2352, 3267, 0 } }, { 4074, 0, 418, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7483, -1296, 2174, 0 }, { { -337, -2320, 3267, 0 }, { 335, -2320, -3268, 0 }, { -337, 2320, 3267, 0 }, { 335, 2320, -3268, 0 } }, { -4076, 0, -420, 0 }, { 0, 0, 4096, 0 }, 4015, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3FFF, -1296, 3775, 0 }, { { -407, -2320, 1963, 0 }, { 403, -2320, -1967, 0 }, { -407, 2320, 1963, 0 }, { 403, 2320, -1967, 0 } }, { -4033, 0, -832, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3F40, -1328, 3727, 0 }, { { 400, -2352, -1951, 0 }, { -404, -2352, 1947, 0 }, { 400, 2352, -1951, 0 }, { -404, 2352, 1947, 0 } }, { 4027, 0, 830, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3400, -1344, 928, 0 }, { { 1262, -2352, -2822, 0 }, { -1262, -2352, 2822, 0 }, { 1262, 2352, -2822, 0 }, { -1262, 2352, 2822, 0 } }, { 3740, 0, 1672, 0 }, { 0, 0, 4096, 0 }, 3882, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3430, -1312, 5328, 0 }, { { -1330, -2352, -1782, 0 }, { 1330, -2352, 1782, 0 }, { -1330, 2352, -1782, 0 }, { 1330, 2352, 1782, 0 } }, { 3290, 0, -2457, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3531, -1344, 5393, 0 }, { { 1310, -2352, 1786, 0 }, { -1310, -2352, -1786, 0 }, { 1310, 2352, 1786, 0 }, { -1310, 2352, -1786, 0 } }, { -3316, 0, 2430, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x345F, -1345, 1296, 0 }, { { -1058, -2352, 2442, 0 }, { 1058, -2352, -2442, 0 }, { -1058, 2352, 2442, 0 }, { 1058, 2352, -2442, 0 } }, { -3765, 0, -1632, 0 }, { 0, 0, 4096, 0 }, 3547, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Nineteen room interactions: two warps, two automatic actions and fifteen CAP spots.
///
/// Warp keys pack departure/arrival slots: 0x12 goes to the garage and 0x21 to
/// the trailer. Room actions 1/2 start and finish the companion sequence; the
/// CAP room-message spot sends dialogue command 6, while others index CAP.
/// Setup includes the LAST record and mutates links, coordinates and latches.
/// Positions/radii use room units and normals use 4096 per unit; the overlay
/// must remain loaded until all action triggers are unlinked.
static WorldCollisionTrigger _gDryfieldJunkYardActionTriggers[19] = {
    { NULL, NULL, NULL, { 5248, -48, 336, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_GARAGE, DRYFIELD_JUNK_YARD_GARAGE_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5600, -48, 1568, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_TRAILER_COACH, DRYFIELD_JUNK_YARD_TRAILER_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 432, -64, 4592, 0 }, { { -896, 0, -1056, 0 }, { 896, 0, -1056, 0 }, { -896, 0, 1056, 0 }, { 896, 0, 1056, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6623, -64, 2783, 0 }, { { -935, 0, 3140, 0 }, { -306, 0, -3261, 0 }, { 307, 0, 3262, 0 }, { 936, 0, -3139, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 3268, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_JUNK_YARD_TRIGGER_BEGIN_SEQUENCE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4A1F, -64, 3487, 0 }, { { -449, 0, 1009, 0 }, { -148, 0, -1017, 0 }, { 149, 0, 1018, 0 }, { 450, 0, -1008, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, DRYFIELD_JUNK_YARD_TRIGGER_FINISH_SEQUENCE, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1744, -64, 351, 0 }, { { -1040, 0, -592, 0 }, { 1040, 0, -592, 0 }, { -1040, 0, 849, 0 }, { 1040, 0, 849, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x41A2, -64, 3328, 0 }, { { -2624, 0, -272, 0 }, { 2624, 0, -656, 0 }, { -2624, 0, 656, 0 }, { 2624, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 2697, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 352, -64, 2336, 0 }, { { -896, 0, -1056, 0 }, { 896, 0, -1056, 0 }, { -896, 0, 1056, 0 }, { 896, 0, 1056, 0 } }, { 0, 4100, 0, 0 }, { 3973, 0, -995, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1376, -64, 4096, 0 }, { { -480, 0, -1344, 0 }, { 448, 0, -1280, 0 }, { -480, 0, 1312, 0 }, { 512, 0, 1312, 0 } }, { 0, 4100, 0, 0 }, { -4077, 0, -402, 0 }, 1425, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5088, -64, 3056, 0 }, { { -1032, 0, -528, 0 }, { 1112, 0, -528, 0 }, { -1032, 0, 528, 0 }, { 952, 0, 528, 0 } }, { 0, 4106, 0, 0 }, { 200, 0, 4090, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x51B0, -64, 4144, 0 }, { { -1840, 0, -464, 0 }, { 1840, 0, -464, 0 }, { -1840, 0, 464, 0 }, { 1840, 0, 464, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1894, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4140, -64, 4032, 0 }, { { -2384, 0, -464, 0 }, { 2384, 0, -464, 0 }, { -2384, 0, 464, 0 }, { 2384, 0, 464, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2428, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2960, -64, 3136, 0 }, { { -1680, 0, -416, 0 }, { 1648, 0, -352, 0 }, { -1680, 0, 384, 0 }, { 1712, 0, 384, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, -4091, 0 }, 1750, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, 4160, 0 }, { { -576, 0, -1344, 0 }, { 352, 0, -1280, 0 }, { -576, 0, 1312, 0 }, { 800, 0, 1312, 0 } }, { 0, 4100, 0, 0 }, { 3856, 0, -1380, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 864, -64, 608, 0 }, { { -704, 0, -672, 0 }, { 704, 0, -672, 0 }, { -704, 0, 672, 0 }, { 704, 0, 672, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 972, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3660, -64, 4720, 0 }, { { -239, 0, -1216, 0 }, { 848, 0, -1216, 0 }, { -1455, 0, 1216, 0 }, { 848, 0, 1216, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1894, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3587, -64, 2471, 0 }, { { -318, 0, 1507, 0 }, { -895, 0, -1373, 0 }, { 880, 0, 1326, 0 }, { 335, 0, -1458, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 1634, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5320, -64, 2032, 0 }, { { -552, 0, -704, 0 }, { 632, 0, -704, 0 }, { -552, 0, 704, 0 }, { 472, 0, 704, 0 } }, { 0, 4104, 0, 0 }, { 4050, 0, 601, 0 }, 944, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7520, -64, 4704, 0 }, { { -3216, 0, -624, 0 }, { 3216, 0, -624, 0 }, { -3216, 0, 624, 0 }, { 3216, 0, 624, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 3268, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// One room-space sight-blocking quad, included as the list's LAST record.
///
/// Setup links and enables it; positions/radius use room units and its normal
/// uses 4096 per unit. Retained low flag value 1 has no established meaning.
/// Keep this mutable record alive until the occluder list is unlinked.
static WorldCollisionOccluder _gDryfieldJunkYardSightOccluders[1] = {
    { NULL, NULL, { 0x4980, -544, 1536, 0 }, { { 0, 1024, -1024, 0 }, { 0, -1024, -1024, 0 }, { 0, 1024, 1024, 0 }, { 0, -1024, 1024, 0 } }, { 4096, 0, 0, 0 }, 1448, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// The junk yard's point lights, contributing in every room view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits (`ONE` is full strength). Full strength occurs at each
/// light's position, with squared-distance falloff to its outer radius.
/// The loaded room overlay owns these mutable records: coordinate updates
/// parent them to the view and rebuild transforms; shading queries overwrite
/// attenuation. References must not outlive the loaded overlay.
static WorldCoordPointLight _gDryfieldJunkYardPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 505, -2000, 6031 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3094, 3094, 3094 },
        },
        .inner = 0,
        .outer = 7200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1931, -2000, -3662 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3094, 3094, 3094 },
        },
        .inner = 0,
        .outer = 7200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9505, 161, -2977 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3278 },
        },
        .inner = 0,
        .outer = 9103,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6211, -1300, 4996 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3278 },
        },
        .inner = 0,
        .outer = 6008,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3699, -1637, 7534 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3564, 3564, 3564 },
        },
        .inner = 0,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3CEF, 440, 103 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 4066, ONE, ONE },
        },
        .inner = 0,
        .outer = 5125,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x59EE, -2000, 1018 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 0,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4652, -2000, 6020 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 4066, ONE, ONE },
        },
        .inner = 0,
        .outer = 4000,
    },
};

/// The junk yard's two cone lights, contributing in every room view.
///
/// Positions and falloff radii use integer world units; RGB intensities and
/// axes use 12 fractional bits (`ONE` is full strength or unit length).
/// Each cone's full opening is in 0x1000 units per turn.
/// The loaded room overlay owns these mutable records: coordinate updates
/// parent them to the view and rebuild transforms; shading queries overwrite
/// attenuation. References must not outlive the loaded overlay.
static WorldCoordSpotLight _gDryfieldJunkYardSpotLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { -2140, 0, 3502 }, { 1222, -3850, 746 }, { 3285, 1432, 2002 } }, { -8050, -2992, 2469 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 2043, 2050, 2050 },
        },
        .axis  = { 3496, 745, 1999, 0 },
        .inner = 0x783C,
        .outer = 0x783D,
        .angle = 671,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { -1983, 0, -3594 }, { -1114, -3902, 614 }, { -3417, 1271, 1884 } }, { 0x7CCE, -3078, 2935 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3320, 3320, 3320 },
        },
        .axis  = { -3586, 613, 1881, 0 },
        .inner = 0x4A38,
        .outer = 0x55F0,
        .angle = 671,
    },
};

/// Uninterpreted bytes between the room's cone lights and light descriptor.
///
/// No identified consumer reaches this region. Its original type, internal
/// boundaries and purpose are unproven; preserve the image bytes verbatim.
static u8 _gDryfieldJunkYardUnknownData[648] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x74,
    0xC4,
    0x18,
    0x80,
    0x02,
    0x00,
    0x00,
    0x00,
    0x74,
    0xC7,
    0x00,
    0x00,
    0x49,
    0x00,
    0x00,
    0x10,
    0x4B,
    0x00,
    0x00,
    0x10,
    0x49,
    0x00,
    0x00,
    0x10,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x70,
    0xC8,
    0x18,
    0x80,
    0x70,
    0xC8,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x70,
    0xC8,
    0x18,
    0x80,
    0xC8,
    0x00,
    0x00,
    0x00,
    0xA8,
    0xFF,
    0x60,
    0x00,
    0xFC,
    0x02,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x21,
    0x03,
    0x38,
    0xE0,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x00,
    0x00,
    0x4B,
    0x03,
    0x38,
    0xD8,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xA8,
    0xFF,
    0x38,
    0x00,
    0xE9,
    0x03,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x30,
    0x00,
    0x2D,
    0x04,
    0x38,
    0xB8,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x28,
    0x00,
    0x7B,
    0x04,
    0x38,
    0xB0,
    0x80,
    0x80,
    0x00,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x20,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x1B,
    0x00,
    0x0D,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x70,
    0x00,
    0x6F,
    0x02,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xD9,
    0x02,
    0x00,
    0x38,
    0x80,
    0x80,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x00,
    0x00,
    0x50,
    0x00,
    0x50,
    0x00,
    0x05,
    0x03,
    0x00,
    0x58,
    0x80,
    0x80,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x48,
    0x00,
    0x36,
    0x03,
    0x78,
    0x70,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0x00,
    0x00,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x40,
    0x00,
    0x6E,
    0x03,
    0x00,
    0x30,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x50,
    0x00,
    0x30,
    0x00,
    0xF7,
    0x03,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x00,
    0x00,
    0x50,
    0x00,
    0x28,
    0x00,
    0x4C,
    0x04,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xB1,
    0x04,
    0x08,
    0x10,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x00,
    0x00,
    0xF0,
    0xFF,
    0x08,
    0x28,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x60,
    0xFF,
    0x60,
    0x00,
    0xBA,
    0x02,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0x30,
    0x00,
    0x08,
    0x00,
    0x60,
    0xFF,
    0x58,
    0x00,
    0xC9,
    0x02,
    0x20,
    0x18,
};

/// The room's eight point lights and two cone lights, with no directional lights.
///
/// Borrows the overlay's mutable arrays for view-coordinate updates and model
/// shading. All authored lights accept every view; the collection and arrays
/// must remain loaded for the duration of those uses.
static WorldCoordRoomLights _gDryfieldJunkYardRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldJunkYardPointLights),
    .pointLights           = _gDryfieldJunkYardPointLights,
    .coneLightCount        = ARRAY_SIZE(_gDryfieldJunkYardSpotLights),
    .coneLights            = _gDryfieldJunkYardSpotLights,
};

/// Minimum ambient RGB for room views 1..7, preceded by their element count.
///
/// Colour entries use 16 units per 8-bit level and retain their stored luma.
/// Lighting clamps model ambient colours to these overlay-owned minima;
/// the table must remain loaded while queried and has no end marker.
static WorldCoordRoomAmbientEntry _gDryfieldJunkYardAmbientByView[] = {
    { .viewCount = ARRAY_SIZE(_gDryfieldJunkYardAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 400, 400, 400, 400 } },
    { .color = { 350, 350, 350, 350 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 400, 400, 400, 400 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 450, 450, 450, 450 } },
};

/// Paired type-1 footstep script bases for collision surface class 1.
///
/// Cue 2 uses each base and cue 1 adds one; companion playback adds 100.
/// Scripted jumps share the walk pair. The surface policy borrows this
/// overlay-owned record without modifying it while the room is loaded.
static WorldCollisionFootstepSounds _gDryfieldJunkYardClass1FootstepSounds = {
    .walk         = DRYFIELD_JUNK_YARD_WALK_FOOTSTEP_CUE_2,
    .run          = DRYFIELD_JUNK_YARD_RUN_FOOTSTEP_CUE_2,
    .scriptedJump = DRYFIELD_JUNK_YARD_WALK_FOOTSTEP_CUE_2,
};

/// Blocking, impact-enabled, pushback-enabled policy for silent classes 0 and 2..7.
///
/// The surface-class directory borrows this one overlay-owned record. Its
/// first stored byte is zero; that byte's role remains unproven.
static WorldCollisionSurfaceProperties _gDryfieldJunkYardSilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking, impact-enabled, pushback-enabled policy and footstep cues for class 1.
///
/// The surface-class directory borrows this one overlay-owned record and its
/// sound bases. Its first stored byte is zero, with an unproven role.
static WorldCollisionSurfaceProperties _gDryfieldJunkYardClass1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldJunkYardClass1FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldJunkYardSurfacePropertiesByClass[8] = {
    &_gDryfieldJunkYardSilentSurfaceProperties,
    &_gDryfieldJunkYardClass1SurfaceProperties,
    &_gDryfieldJunkYardSilentSurfaceProperties,
    &_gDryfieldJunkYardSilentSurfaceProperties,
    &_gDryfieldJunkYardSilentSurfaceProperties,
    &_gDryfieldJunkYardSilentSurfaceProperties,
    &_gDryfieldJunkYardSilentSurfaceProperties,
    &_gDryfieldJunkYardSilentSurfaceProperties,
};

void dryfieldJunkYardWireRopeTask(Task* task)
{
    enum { DRYFIELD_JUNK_YARD_WIRE_ROPE_COLLECTED_STATE = 2 };

    const Enemy* placedObject;
    TmdObject*   model;
    s32          objectState;

    placedObject = task->spawnArg2.pointer;
    model        = task->extra.tmd;
    objectState  = areaGetCurrentObjectState((u8)placedObject->placeKey);
    // Clear flags before the branch; collection hides both model and shadow.
    model->flags = 0;
    if (objectState == DRYFIELD_JUNK_YARD_WIRE_ROPE_COLLECTED_STATE) {
        model->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        model->otOffset = 0;
    }
    _dryfieldJunkYardDrawModelGroundShadow(task);
}

/// Draws the room model's ground shadow at its composed root position.
///
/// `task` must own a model with a root coordinate. Models that suppress active
/// drawing or have no primitive buffer draw no shadow. The square's half-side
/// is 416 world-coordinate units and its grayscale shade is 192. The temporary
/// centre is borrowed by the shadow drawer and released before returning.
static void _dryfieldJunkYardDrawModelGroundShadow(Task* task)
{
    enum {
        DRYFIELD_JUNK_YARD_GROUND_SHADOW_HALF_SIZE = 0x1A0,
        DRYFIELD_JUNK_YARD_GROUND_SHADOW_SHADE     = 0xC0,
    };
    ActorRenderGroundShadowCentreScratch* shadowScratch;
    GfxCoord*                             rootCoord;
    TmdObject*                            model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if ((model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) == 0 && model->buffer != NULL) {
        shadowScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorRenderGroundShadowCentreScratch);
        // Compose the root before taking its world-space shadow centre.
        actorRenderComposeCoord(rootCoord);
        shadowScratch->centre.vx = rootCoord->workm.t[0];
        shadowScratch->centre.vy = rootCoord->workm.t[1];
        shadowScratch->centre.vz = rootCoord->workm.t[2];
        effectDrawGroundShadow(&shadowScratch->centre, DRYFIELD_JUNK_YARD_GROUND_SHADOW_HALF_SIZE, DRYFIELD_JUNK_YARD_GROUND_SHADOW_SHADE);
        SCRATCH_STACK_RELEASE_BLOCK(ActorRenderGroundShadowCentreScratch);
    }
}

/// Registers the junk-yard receiver and prepares the companion's entry scene.
///
/// Start in state 0 with the room loaded. If the companion is present, place
/// and animate it before junk-yard progress, then start the unseen return
/// scene after trailer-coach progress reaches 2, or place it for arrival 2.
/// Repeated slot lookups borrow the current companion; advances to idle state 1.
static void _dryfieldJunkYardInitRoomTask(Task* task)
{
    enum {
        DRYFIELD_JUNK_YARD_TRAILER_RETURN_READY = 2,
        DRYFIELD_JUNK_YARD_COMPANION_ARRIVAL    = 2,
        DRYFIELD_JUNK_YARD_RETURN_SCENE_SEEN    = 1,
    };

    task->msgTable = _gDryfieldJunkYardRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) {
        if (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldJunkYardCompanionOpeningPlacement, 0);
            companionWriteAnimationBankIndex(&_gDryfieldJunkYardCompanionOpeningRequest.source.index);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &_gDryfieldJunkYardCompanionOpeningRequest, 0);
        }
        if ((gameFlagGetNibble(GAME_FLAG_JUNK_YARD_RETURN_SCENE_SEEN) == 0) && (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) >= DRYFIELD_JUNK_YARD_TRAILER_RETURN_READY)) {
            gameFlagSetNibble(GAME_FLAG_JUNK_YARD_RETURN_SCENE_SEEN, DRYFIELD_JUNK_YARD_RETURN_SCENE_SEEN);
            evsStartScriptWithSkip(_gDryfieldJunkYardCompanionReturnScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldJunkYardCompanionReturnSkipScript);
        } else if (gGameSession->location.warp == DRYFIELD_JUNK_YARD_COMPANION_ARRIVAL) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_PLACE, &_gDryfieldJunkYardCompanionTrailerPlacement, 0);
        }
    }
    task->state = task->state + 1;
}

/// Runs the junk-yard companion scene or its turn-and-animation sequence.
///
/// Initial state 0 starts the skippable scene, waits for event completion and
/// releases at state 2. Entry at state 3 instead requests a companion yaw turn,
/// waits for motion, plays clip 7 to completion and starts clip 9 before release.
/// Other states do nothing. The room scripts and companion must remain live
/// through the selected sequence; message payloads are borrowed synchronously.
static void _dryfieldJunkYardCompanionSequenceTask(Task* task)
{
    enum { COMPANION_SCENE_START      = 0,
           COMPANION_SCENE_WAIT       = 1,
           COMPANION_SEQUENCE_RELEASE = 2,
           COMPANION_TURN_START       = 3,
           COMPANION_TURN_WAIT        = 4,
           COMPANION_ANIMATION_START  = 5,
           COMPANION_ANIMATION_WAIT   = 6,
           COMPANION_FINAL_ANIMATION  = 7 };

    switch (task->state) {
        case COMPANION_SCENE_START:
            evsStartScriptWithSkip(_gDryfieldJunkYardCompanionOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldJunkYardCompanionOpeningSkipScript);
            task->state = task->state + 1;
            return;
        case COMPANION_SCENE_WAIT:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case COMPANION_TURN_START:
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_TURN_TO_YAW, &_gDryfieldJunkYardCompanionRestFacing, 0);
            task->state = task->state + 1;
            return;
        case COMPANION_TURN_WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case COMPANION_ANIMATION_START:
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &_gDryfieldJunkYardCompanionRestRequest, 0);
            task->state = task->state + 1;
            return;
        case COMPANION_ANIMATION_WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case COMPANION_FINAL_ANIMATION:
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &_gDryfieldJunkYardCompanionRestHoldRequest, 0);
            /* fallthrough */
        case COMPANION_SEQUENCE_RELEASE:
            taskKill(task);
            return;
    }
}

#include "../../shared/junk_yard_cap_msg.inc.c"

/// Refuses every key-item-use request in the junk yard without consuming an item.
///
/// The message carries the inventory item ID in `itemId` and an unused second
/// word. Returns 0, selecting the inventory's "no use now" response; all
/// parameters are ignored.
static s32 _dryfieldJunkYardRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves garage arrivals and gates departure to the trailer coach on companion progress.
///
/// Borrows a complete request and writable reply, which may alias. Returns 2
/// while the companion's final scene is eligible, otherwise 1. Execution starts
/// that scene or commits the first trailer arrival and companion schedule;
/// queries only copy the request and report eligibility. Requires live flags
/// and the room scripts through deferred playback.
static s32 _dryfieldJunkYardResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { TRANSITION_ALLOWED          = 1,
           COMPANION_SCENE_ELIGIBLE    = 2,
           GARAGE_LATE_CHAPTER         = 4,
           GARAGE_DEFAULT_ROOM         = 1,
           GARAGE_LATE_ROOM            = 2,
           JUNK_YARD_SEQUENCE_ACTIVE   = 1,
           JUNK_YARD_SEQUENCE_FINISHED = 2,
           TRAILER_FIRST_ARRIVAL_WARP  = 2,
           TRAILER_PROGRESS_STARTED    = 1,
           COMPANION_TRAILER_SCHEDULE  = 4 };

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_GARAGE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) >= GARAGE_LATE_CHAPTER) {
            reply->roomState = GARAGE_LATE_ROOM;
        } else {
            reply->roomState = GARAGE_DEFAULT_ROOM;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_TRAILER_COACH) {
        if (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == JUNK_YARD_SEQUENCE_ACTIVE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibble(GAME_FLAG_JUNK_YARD_PROGRESS, JUNK_YARD_SEQUENCE_FINISHED);
                evsStartScriptWithSkip(_gDryfieldJunkYardCompanionDepartureScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldJunkYardCompanionFinishSkipScript);
            }
            return COMPANION_SCENE_ELIGIBLE;
        }
        if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) == 0 && request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->warp = TRAILER_FIRST_ARRIVAL_WARP;
            gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS, TRAILER_PROGRESS_STARTED);
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_TRAILER_SCHEDULE);
        }
    }
    return TRANSITION_ALLOWED;
}

/// Starts and finishes the junk-yard companion sequence from room trigger actions.
///
/// Borrows `request` for synchronous dispatch. Action 1 starts the sequence
/// while progress is 0; action 2 advances progress 1 to 2 once the companion's
/// root X reaches 21001 whole room-coordinate units. Progress is committed
/// before task/script allocation, with no rollback on failure. Returns zero;
/// the receiver, message ID and second payload word are unused. The room and
/// its scripts must remain loaded until the spawned sequence finishes.
static s32 _dryfieldJunkYardHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArgument)
{
    enum {
        DRYFIELD_JUNK_YARD_ACTION_BEGIN_SEQUENCE  = 1,
        DRYFIELD_JUNK_YARD_ACTION_FINISH_SEQUENCE = 2,
        DRYFIELD_JUNK_YARD_PROGRESS_INITIAL       = 0,
        DRYFIELD_JUNK_YARD_PROGRESS_SEQUENCE      = 1,
        DRYFIELD_JUNK_YARD_PROGRESS_FINAL_SCENE   = 2,
        DRYFIELD_JUNK_YARD_COMPANION_EXIT_X_MIN   = 21001,
        DRYFIELD_JUNK_YARD_SEQUENCE_TASK          = 0,
    };

    Task* companion;

    if (request->actionId == DRYFIELD_JUNK_YARD_ACTION_BEGIN_SEQUENCE) {
        if (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == DRYFIELD_JUNK_YARD_PROGRESS_INITIAL) {
            gameFlagSetNibble(GAME_FLAG_JUNK_YARD_PROGRESS, DRYFIELD_JUNK_YARD_PROGRESS_SEQUENCE);
            taskSpawnFromTable(_gDryfieldJunkYardCompanionSequenceTasks, DRYFIELD_JUNK_YARD_SEQUENCE_TASK, 0, 0);
        }
    }
    if (request->actionId == DRYFIELD_JUNK_YARD_ACTION_FINISH_SEQUENCE) {
        companion = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
        if ((companion != NULL) && (companion->extra.tmd->coords->coord.t[0] >= DRYFIELD_JUNK_YARD_COMPANION_EXIT_X_MIN) &&
            (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == DRYFIELD_JUNK_YARD_PROGRESS_SEQUENCE)) {
            gameFlagSetNibble(GAME_FLAG_JUNK_YARD_PROGRESS, DRYFIELD_JUNK_YARD_PROGRESS_FINAL_SCENE);
            evsStartScriptWithSkip(_gDryfieldJunkYardCompanionFinishScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gDryfieldJunkYardCompanionFinishSkipScript);
        }
    }
    return 0;
}

/// Sets the live save's scene event for subsequent area-music selection.
///
/// The event-script callback consumes the signed low byte of its argument word.
/// Both the return scene and its skip path supply
/// `DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT`.
static void _dryfieldJunkYardSetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

/// Keeps the junk-yard room task idle, with an optional companion debug hook.
///
/// With debug mode enabled and a live companion, calls the external routine
/// with that task, -140, 10 and the "DOG" string. The routine's effect and the
/// integer arguments' units are unproven. Leaves the room state unchanged and
/// repeats the hook each update; the callback's own task argument is unused.
static void _dryfieldJunkYardIdleRoomTask(Task* unusedTask)
{
    if ((gDisplayState.debugMode != 0) && (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0)) {
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), -0x8C, 0xA, _gDryfieldJunkYardCompanionDebugLabel);
    }
}

void dryfieldJunkYardRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldJunkYardRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

void dryfieldJunkYardEnableViewEffectsTask(Task* unusedTask)
{
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
