#include "rooms/acropolis_east_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

static const char _gAcropolisEastElevatorHallPlayerDebugLabel[];

/// Keeps the reflection scale at this room's earlier rodata position.
///
/// 0 requires `planar_reflection_rodata.inc.c` before the shared implementation.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 0
#include "../../shared/planar_reflection.h"
/// Binds the shared beacon body to this room's public `void (Task*)` callback.
///
/// Required before `red_beacon.h` and its task fragment; gameplay imports this instance.
#define RED_BEACON_TASK acropolisEastElevatorHallRedBeaconTask
#include "../../shared/red_beacon.h"

static void _acropolisEastElevatorHallInitializeRoomTask(Task* task);
static void _acropolisEastElevatorHallUpdatePlayerDebugDisplay(Task* unusedTask);

/// Scale handed to `ScaleMatrix` to flip the reflection across X.
#include "../../shared/planar_reflection_rodata.inc.c"

/// Room-task states 0..2: initialize, update the player debug display, release.
///
/// Copied by value before unchecked dispatch; keep the state within this range.
static const TaskFuncTable3 _gAcropolisEastElevatorHallRoomTaskStates = {
    { _acropolisEastElevatorHallInitializeRoomTask, _acropolisEastElevatorHallUpdatePlayerDebugDisplay, taskKill },
};

static AnimationSet _gAcropolisEastElevatorHallAnimation02EB8;
static AnimationSet _gAcropolisEastElevatorHallAnimation07824;
static AnimationSet _gAcropolisEastElevatorHallAnimation08600;

static WorldCollisionGrid    _gAcropolisEastElevatorHallCollisionGrid;
static WorldCollisionTrigger _gAcropolisEastElevatorHallViewBoundaryTriggers[];
static WorldCollisionTrigger _gAcropolisEastElevatorHallActionTriggers[];
static WorldCoordRoomLights  _gAcropolisEastElevatorHallRoomLights;
static s32                   _acropolisEastElevatorHallResolveTransitionMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32                   _acropolisEastElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32                   _acropolisEastElevatorHallStartOpeningEvent(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg);
static s32                   _acropolisEastElevatorHallHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedExecutionMode);
static void                  _acropolisEastElevatorHallStartSkipCapSequence(void);

/// Key-item use request sent to the room task by the inventory menu.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

#include "../../shared/planar_reflection_data.inc.c"

/// Execution priority shared by the player and attachment reflection tasks.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PRIORITY = 112 };

/// Task recipes for the hall's player reflection (slot 0) and held models (slot 1).
///
/// Both start bodyless at the same priority and attach their cloned models during
/// initialization. Slot 1 is used by the shared reflection implementation; the
/// hall starts player reflections through gameplay's task bank. There is no
/// terminator. Descriptors are borrowed during spawning; callbacks and source
/// geometry must stay loaded for the spawned tasks' lifetimes.
static TaskDesc _gAcropolisEastElevatorHallReflectionTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, acropolisEastElevatorHallPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return _gAcropolisEastElevatorHallReflectionTaskDescriptors;
}

static AnimationPackedPose _gAcropolisEastElevatorHallAnimation02EB8Bank1[4] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisEastElevatorHallAnimation02EB8Bank4[216] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_bank4.inc"
};

static AnimationRecord _gAcropolisEastElevatorHallAnimation02EB8Records[262] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_records.inc"
};

static u16 _gAcropolisEastElevatorHallAnimation02EB8Indices[20] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_indices.inc"
};

static AnimationSet _gAcropolisEastElevatorHallAnimation02EB8 = {
    _gAcropolisEastElevatorHallAnimation02EB8Records,
    _gAcropolisEastElevatorHallAnimation02EB8Indices,
    { NULL, _gAcropolisEastElevatorHallAnimation02EB8Bank1, NULL, NULL, _gAcropolisEastElevatorHallAnimation02EB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisEastElevatorHallAnimation07824Bank1[156] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_bank1.inc"
};

static AnimationPackedRotation _gAcropolisEastElevatorHallAnimation07824Bank4[1724] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_bank4.inc"
};

static AnimationRecord _gAcropolisEastElevatorHallAnimation07824Records[2487] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_records.inc"
};

static u16 _gAcropolisEastElevatorHallAnimation07824Indices[20] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_indices.inc"
};

static AnimationSet _gAcropolisEastElevatorHallAnimation07824 = {
    _gAcropolisEastElevatorHallAnimation07824Records,
    _gAcropolisEastElevatorHallAnimation07824Indices,
    { NULL, _gAcropolisEastElevatorHallAnimation07824Bank1, NULL, NULL, _gAcropolisEastElevatorHallAnimation07824Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisEastElevatorHallAnimation08600Bank1[21] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_bank1.inc"
};

static AnimationPackedRotation _gAcropolisEastElevatorHallAnimation08600Bank4[370] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_bank4.inc"
};

static AnimationRecord _gAcropolisEastElevatorHallAnimation08600Records[434] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_records.inc"
};

static u16 _gAcropolisEastElevatorHallAnimation08600Indices[20] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_indices.inc"
};

static AnimationSet _gAcropolisEastElevatorHallAnimation08600 = {
    _gAcropolisEastElevatorHallAnimation08600Records,
    _gAcropolisEastElevatorHallAnimation08600Indices,
    { NULL, _gAcropolisEastElevatorHallAnimation08600Bank1, NULL, NULL, _gAcropolisEastElevatorHallAnimation08600Bank4, NULL, NULL, NULL },
};

/// Set-table slots used by the opening's player requests and view-figure clips.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_PLAYER_ANIMATION        = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_WEAPON_ANIMATION                = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_LEAD_IN_ANIMATION   = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_MAIN_ANIMATION      = 2,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_EXIT_ANIMATION      = 3,
    ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_COMMAND_2_ANIMATION = 5,
};

/// Player set table for the opening's main phase after the second CAP cue.
///
/// Slot 0 is absent; slot 1 is the only playable set. No terminator is read.
/// The player borrows this table and its clip until playback is replaced;
/// the room overlay must remain loaded throughout that interval.
static AnimationSet* _gAcropolisEastElevatorHallOpeningPlayerMainAnimationSets[2] = {
    NULL,
    &_gAcropolisEastElevatorHallAnimation07824,
};

/// Player set table for the opening's lead-in phase after the first CAP cue.
///
/// Slot 0 is absent and slot 1 is the only playable set, with no terminator.
/// Borrowed by player playback until replacement; keep the room overlay loaded.
static AnimationSet* _gAcropolisEastElevatorHallOpeningPlayerLeadInAnimationSets[2] = {
    NULL,
    &_gAcropolisEastElevatorHallAnimation02EB8,
};

/// Player set table for the opening's final 80-frame phase.
///
/// Slot 0 is absent and slot 1 is the only playable set, with no terminator.
/// Borrowed by player playback until replacement; keep the room overlay loaded.
static AnimationSet* _gAcropolisEastElevatorHallOpeningPlayerExitAnimationSets[2] = {
    NULL,
    &_gAcropolisEastElevatorHallAnimation08600,
};

/// Installs the opening's main player clip with reset playback and grid collision off.
///
/// The request is borrowed during dispatch; its set table survives through playback.
/// The retained one-frame blend duration is ignored by reset playback.
static AnimationPlayRequest _gAcropolisEastElevatorHallOpeningPlayerMainAnimationRequest = {
    .source               = { .sets = _gAcropolisEastElevatorHallOpeningPlayerMainAnimationSets },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_PLAYER_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the opening's lead-in player clip with reset playback and grid collision off.
///
/// The request is borrowed during dispatch; its set table survives through playback.
/// The retained one-frame blend duration is ignored by reset playback.
static AnimationPlayRequest _gAcropolisEastElevatorHallOpeningPlayerLeadInAnimationRequest = {
    .source               = { .sets = _gAcropolisEastElevatorHallOpeningPlayerLeadInAnimationSets },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_PLAYER_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the opening's final player clip with reset playback and grid collision off.
///
/// The request is borrowed during dispatch; its set table survives through playback.
static AnimationPlayRequest _gAcropolisEastElevatorHallOpeningPlayerExitAnimationRequest = {
    .source               = { .sets = _gAcropolisEastElevatorHallOpeningPlayerExitAnimationSets },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_PLAYER_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the player's equipped-weapon clip 1 during the room's event scripts.
///
/// The interpreter copies the request and overwrites the bank selector with the
/// equipped weapon's bank. The stored selector is unread; collision stays off.
static AnimationPlayRequest _gAcropolisEastElevatorHallWeaponAnimationRequest = {
    .source               = { .index = 1 },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_WEAPON_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts placed view figure 0's lead-in clip for the opening event.
///
/// Only the animation ID is consumed; the other words are retained but ignored.
/// Borrowed only during dispatch to the loaded figure package.
static AnimationPlayRequest _gAcropolisEastElevatorHallOpeningViewFigureLeadInAnimationRequest = {
    .source               = { .index = 0 },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_LEAD_IN_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts placed view figure 0's main opening clip for the 944-frame phase.
///
/// Only the animation ID is consumed; the other words are retained but ignored.
/// Borrowed only during dispatch to the loaded figure package.
static AnimationPlayRequest _gAcropolisEastElevatorHallOpeningViewFigureMainAnimationRequest = {
    .source               = { .index = 0 },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_MAIN_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts placed view figure 0's final opening clip for the 80-frame phase.
///
/// Only the animation ID is consumed; the other words are retained but ignored.
/// Borrowed only during dispatch to the loaded figure package.
static AnimationPlayRequest _gAcropolisEastElevatorHallOpeningViewFigureExitAnimationRequest = {
    .source               = { .index = 0 },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_EXIT_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Body clip restored on entry and after the hall's scripted figure sequences.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_DEFAULT_ANIMATION = 4 };

/// Restarts placed view figure 0's default body clip (animation 4).
///
/// Entry, opening completion/skip and the command-2 scene use the same request.
/// The figure reads only `animationId`, resetting its body tracks synchronously;
/// bank, blend and collision words are ignored. The request belongs to this room
/// overlay and is borrowed only during message dispatch.
static AnimationPlayRequest _gAcropolisEastElevatorHallViewFigureDefaultAnimationRequest = {
    .source               = { .index = 0 },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_DEFAULT_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts placed view figure 0's clip for room command 2 and CAP sequence 2.
///
/// Only the animation ID is consumed; the other words are retained but ignored.
/// Borrowed only during dispatch to the loaded figure package.
static AnimationPlayRequest _gAcropolisEastElevatorHallCommand2ViewFigureAnimationRequest = {
    .source               = { .index = 0 },
    .animationId          = ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_FIGURE_COMMAND_2_ANIMATION,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Scene/audio stream key held throughout the opening event.
///
/// Group 1, stream 3, qualifier 11 select the current folder's stream with
/// second qualifier zero. Keep this key loaded until the event releases it.
static EvsSceneKey _gAcropolisEastElevatorHallOpeningSceneKey = {
    .group    = 1,
    .streamId = 3,
    .subId    = 11,
};

/// Unreferenced scene-key-shaped record; its intended use is unproven.
static EvsSceneKey D_acropolis_east_elevator_hall_80185CBC = { 1, 3, 21 };

/// Unreferenced 24-byte record; the transform interpretation is unproven.
static ActorTransform D_acropolis_east_elevator_hall_80185CC4 = { { 1, 0, 0, 0 }, { 0, 0, 0, 0 } };

/// Player's initial opening walk destination, in whole room-coordinate units.
///
/// The move-to message copies XYZ during dispatch; the angles are unread.
static ActorTransform _gAcropolisEastElevatorHallOpeningPlayerWalkTarget = {
    .pos = { 5051, 0, -1280, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Zero position and orientation for the opening player's main animation phase.
///
/// The player copies room-coordinate XYZ and 4096-per-turn angles during
/// dispatch. Also sent to the view figure, whose table ignores placement.
static ActorTransform _gAcropolisEastElevatorHallOpeningOriginTransform = { 0 };

/// Player placement for the opening's lead-in clip after the first CAP cue.
///
/// Position uses whole room-coordinate units and yaw uses 4096 per turn.
/// Both vectors are copied during dispatch; the record is not retained.
static ActorTransform _gAcropolisEastElevatorHallOpeningPlayerLeadInTransform = {
    .pos = { 1937, 0, -492, 0 },
    .rot = { 0, 1224, 0, 0 },
};

/// Unreferenced 24-byte record; the transform interpretation is unproven.
static ActorTransform D_acropolis_east_elevator_hall_80185D24 = { { 0, 0, 0, 0 }, { 0, 1640, 0, 0 } };

/// Player placement after either normal completion or skipping the opening event.
///
/// Position uses whole room-coordinate units and yaw uses 4096 per turn.
/// Both vectors are copied during dispatch; the record is not retained.
static ActorTransform _gAcropolisEastElevatorHallOpeningPlayerEndTransform = {
    .pos = { 4440, 0, -672, 0 },
    .rot = { 0, 1640, 0, 0 },
};

/// Event-script CAP slots, placed figure selector and sound cue.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_CAP_SEQUENCE      = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_COMMAND_2_CAP_SEQUENCE    = 2,
    ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX        = 0,
    ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_DRAW_ENABLED = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_SOUND             = 0x51020003,
    ACROPOLIS_EAST_ELEVATOR_HALL_SKIP_END_VIEW             = 5,
};

/// First-visit opening event, synchronized to CAP sequence 1 and its scene audio.
///
/// Runs 35 commands and END, with frame waits of 72, 944 and 80 in its animation
/// phases. Borrows the room's requests and the loaded player/figure resources;
/// keep them live until this script or its skip target ends. The event runner
/// hides and restores the HUD. Only player animation playback retains set tables.
static EvsCommand _gAcropolisEastElevatorHallOpeningScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_CAP_SEQUENCE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gAcropolisEastElevatorHallOpeningSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_DRAW_ENABLED }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningOriginTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningViewFigureLeadInAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningPlayerWalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisEastElevatorHallWeaponAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 67 }, { .value = 67 }, { .value = 78 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningPlayerLeadInTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_INSTALL_AND_PLAY }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningPlayerLeadInAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 72 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningOriginTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_INSTALL_AND_PLAY }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningPlayerMainAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningViewFigureMainAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 944 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_INSTALL_AND_PLAY }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningPlayerExitAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningViewFigureExitAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gAcropolisEastElevatorHallViewFigureDefaultAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisEastElevatorHallWeaponAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningPlayerEndTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_RESET_ANIMATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Opening-event skip path that restores view 5, player placement and figure clip 4.
///
/// Runs 14 commands and END: fade out for eight frames, restore actors and
/// scene state, fade back, start CAP sequence 16, release player control and
/// resume area music. Borrows this overlay and its loaded actor resources until END.
static EvsCommand _gAcropolisEastElevatorHallOpeningSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .pointer = &_gAcropolisEastElevatorHallOpeningPlayerEndTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisEastElevatorHallWeaponAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gAcropolisEastElevatorHallViewFigureDefaultAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_SKIP_END_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisEastElevatorHallStartSkipCapSequence }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Room-command-2 event: CAP sequence 2 and view-figure clip 5, then clip 4.
///
/// Eight commands and END; waits for a CAP cue and three further frames before
/// restoring the figure and player control. Selects no scene stream. The runner
/// hides and restores the HUD; keep room, CAP and actor resources loaded until END.
static EvsCommand _gAcropolisEastElevatorHallCommand2Script[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_COMMAND_2_CAP_SEQUENCE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisEastElevatorHallWeaponAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gAcropolisEastElevatorHallCommand2ViewFigureAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_EAST_ELEVATOR_HALL_EVENT_FIGURE_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .pointer = &_gAcropolisEastElevatorHallViewFigureDefaultAnimationRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Room receiver's transition, command, key-item and action message handlers.
///
/// Four entries followed by TASK_MESSAGE_TABLE_END; unsupported IDs return zero.
/// Borrowed by the room task until teardown. Transition payloads are request/reply
/// pointers, commands and key items use integer words, and actions borrow a request.
static TaskMessageEntry _gAcropolisEastElevatorHallRoomMessageTable[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisEastElevatorHallResolveTransitionMessage },
    { ROOM_MESSAGE_COMMAND, _acropolisEastElevatorHallHandleCommandMessage },
    { ACROPOLIS_EAST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM, _acropolisEastElevatorHallRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisEastElevatorHallStartOpeningEvent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Opening-event latch for this loaded overlay visit (0 not started, 1 started).
///
/// Set when the opening starts, so skipping does not clear it. Reloading the
/// overlay restores zero; persistent story progress is stored in game flags.
static s32 _gAcropolisEastElevatorHallOpeningEventStarted = 0;

WorldCollisionRoomResources gAcropolisEastElevatorHallRoomCollisionResources[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT] = {
    {
        .grid                 = &_gAcropolisEastElevatorHallCollisionGrid,
        .viewBoundaryTriggers = _gAcropolisEastElevatorHallViewBoundaryTriggers,
        .actionTriggers       = _gAcropolisEastElevatorHallActionTriggers,
        .occluders            = NULL,
    },
};

u8* gAcropolisEastElevatorHallViewMaps[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT] = {
    gViewIdentityMap,
};

ViewCount gAcropolisEastElevatorHallViewCounts[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT] = { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT };

WorldCoordRoomLighting gAcropolisEastElevatorHallRoomLightingTable[ACROPOLIS_EAST_ELEVATOR_HALL_ROOM_COUNT] = {
    { &_gAcropolisEastElevatorHallRoomLights, NULL },
};

/// Sound-script and view/map selectors for the hall's only warp endpoint.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_ARRIVAL_SOUND   = 0x51020002,
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_DEPARTURE_SOUND = 0x51020001,
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_ARRIVAL_VIEW    = 4,
    ACROPOLIS_EAST_ELEVATOR_HALL_WARP_MAP_FLAG        = 502,
};

DirectionWarpEntry gAcropolisEastElevatorHallWarpEntries[ACROPOLIS_EAST_ELEVATOR_HALL_WARP_COUNT] = {
    {
        .player         = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, .x = -5248, .y = -31, .z = 1068 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_TURN / 4 }, .x = -5248, .y = -31, .z = 1068 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = ACROPOLIS_EAST_ELEVATOR_HALL_WARP_ARRIVAL_SOUND,
        .departureSound = ACROPOLIS_EAST_ELEVATOR_HALL_WARP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = ACROPOLIS_EAST_ELEVATOR_HALL_WARP_ARRIVAL_VIEW,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = ACROPOLIS_EAST_ELEVATOR_HALL_WARP_MAP_FLAG,
    },
};

/// Grid-local face normals, using 4096 for unit length.
static SVECTOR _gAcropolisEastElevatorHallCollisionGridNormals[] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_normals.inc"
};

/// Collision-mesh vertex pool in signed grid-local game coordinates.
static SVECTOR _gAcropolisEastElevatorHallCollisionGridVerts[] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_verts.inc"
};

/// Indexed collision quads using the hall's surface classes 0, 1 and 3.
static WorldCollisionGridFace _gAcropolisEastElevatorHallCollisionGridFaces[] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_faces.inc"
};

/// Signed face-index lists for the grid cells, each ending with -1.
static s16 _gAcropolisEastElevatorHallCollisionGridCells[] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisEastElevatorHallCollisionGridCells[i])
/// X-major 4-by-2 cell directory; each pointer selects a terminated face list.
static s16* _gAcropolisEastElevatorHallCollisionGridTable[] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_table.inc"
};
#undef GRID_CELL

/// The hall's collision mesh and its 4-by-2 XZ spatial index.
///
/// Contains 33 quads, 66 vertices and 10 normals. Cell coordinates are
/// (x + 6390) / 4000 and (z + 3090) / 4000 in room game units; cells are
/// indexed x * 2 + z. Room setup borrows this mutable descriptor and binds its
/// view coordinate. The mesh pools and face lists remain live with the overlay.
static WorldCollisionGrid _gAcropolisEastElevatorHallCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisEastElevatorHallCollisionGridNormals,
    .vertices    = _gAcropolisEastElevatorHallCollisionGridVerts,
    .faces       = _gAcropolisEastElevatorHallCollisionGridFaces,
    .cellFaceIds = _gAcropolisEastElevatorHallCollisionGridTable,
    .xBias       = 6390,
    .zBias       = 3090,
    .cellCountX  = 4,
    .cellCountZ  = 2,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisEastElevatorHallCollisionGridFaces),
};

/// Six directed view boundaries: 4 to 3, 3 to 4, 3 to 2, 2 to 3, 2 to 5, 5 to 2.
///
/// Room setup binds their coordinates, links and enables every record through
/// the final LAST flag. Positions/radii use room game units and normals use Q12.
/// Links and hit latches stay writable; keep the overlay loaded until unlinked.
static WorldCollisionTrigger _gAcropolisEastElevatorHallViewBoundaryTriggers[] = {
    { NULL, NULL, NULL, { -2977, -1024, 783, 0 }, { { -889, -1664, -2485, 0 }, { -889, 1664, -2485, 0 }, { 890, -1664, 2486, 0 }, { 890, 1664, 2486, 0 } }, { -3862, 0, 1381, 0 }, { 0, 0, 0, 0 }, 3114, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3649, -992, 799, 0 }, { { -889, 1664, -2485, 0 }, { -889, -1664, -2485, 0 }, { 890, 1664, 2486, 0 }, { 890, -1664, 2486, 0 } }, { 3860, 0, -1383, 0 }, { 0, 0, 0, 0 }, 3114, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1311, -960, 480, 0 }, { { 0, -1664, -2640, 0 }, { 0, 1664, -2640, 0 }, { 0, -1664, 2640, 0 }, { 0, 1664, 2640, 0 } }, { -4102, 0, 0, 0 }, { 0, 0, 0, 0 }, 3114, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 704, -1056, 512, 0 }, { { 0, 1664, -2640, 0 }, { 0, -1664, -2640, 0 }, { 0, 1664, 2640, 0 }, { 0, -1664, 2640, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 0, 0 }, 3114, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4351, -1056, 127, 0 }, { { 1245, 1664, 2329, 0 }, { 1245, -1664, 2329, 0 }, { -1244, 1664, -2328, 0 }, { -1244, -1664, -2328, 0 } }, { -3618, 0, 1933, 0 }, { 0, 0, 0, 0 }, 3114, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3838, -1088, 222, 0 }, { { -1244, 1664, -2328, 0 }, { -1244, -1664, -2328, 0 }, { 1245, 1664, 2329, 0 }, { 1245, -1664, 2329, 0 } }, { 3616, 0, -1935, 0 }, { 0, 0, 0, 0 }, 3114, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// One-based endpoint slots packed into the departure action's two nibbles.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_DEPARTURE_WARP_SLOT = 1,
    ACROPOLIS_EAST_ELEVATOR_HALL_SQUARE_ARRIVAL_SLOT = 2,
};

/// Hall interactions: one departure warp, five CAP commands and the opening event.
///
/// The warp requests area 1, departure slot 1 and arrival slot 2. CAP indices
/// are 3, 4, 5, 6 and 2; automatic room action 0 starts the opening event.
/// Room setup binds, links and enables all seven records through LAST. Positions
/// and radii use game units, normals use Q12, and live links/hits remain writable.
/// Keep the overlay loaded until the triggers are unlinked.
static WorldCollisionTrigger _gAcropolisEastElevatorHallActionTriggers[] = {
    { NULL, NULL, NULL, { -5232, -32, 992, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { ONE, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_SQUARE, (ACROPOLIS_EAST_ELEVATOR_HALL_DEPARTURE_WARP_SLOT << 4) | ACROPOLIS_EAST_ELEVATOR_HALL_SQUARE_ARRIVAL_SLOT, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2996, -128, 2774, 0 }, { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -ONE, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -160, 1888, 0 }, { { 800, 0, -400, 0 }, { 800, 0, 400, 0 }, { -800, 0, -400, 0 }, { -800, 0, 400, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, -ONE, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3696, -128, 1888, 0 }, { { 656, 0, -400, 0 }, { 656, 0, 400, 0 }, { -656, 0, -400, 0 }, { -656, 0, 400, 0 } }, { 0, ONE, 0, 0 }, { 0, 0, -ONE, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3920, -96, -1472, 0 }, { { 832, 0, -624, 0 }, { 448, 0, 624, 0 }, { -896, 0, -624, 0 }, { -384, 0, 624, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, ONE, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1664, -96, 312, 0 }, { { -1840, 0, -2392, 0 }, { 1776, 0, -2488, 0 }, { -1744, 0, 2440, 0 }, { 1808, 0, 2440, 0 } }, { 0, 4109, 0, 0 }, { ONE, 0, 0, 0 }, 3050, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4911, 0, -689, 0 }, { { -569, 0, -56, 0 }, { -209, 0, -1293, 0 }, { -174, 0, 942, 0 }, { 954, 0, 409, 0 } }, { 0, 4099, 0, 0 }, { -2276, 0, 3406, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Default-layout resource for placed entry 110: actor 110300's view figure.
///
/// File number 103 with the base-10 selector loads base file 110300; the paired
/// placement has low file component 0 and selects the body task. One live record
/// precedes AREA_PLACEMENT_END. Borrow the table during loading and spawning;
/// the actor package owns the task callbacks and model/animation data.
static AreaResource _gAcropolisEastElevatorHallAreaResources[] = {
    { 110, 103, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, VIEW_FIGURE_TASK_BODY, { 0, 0 }, gActor110300ViewFigureTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_east_elevator_hall_80186C50[3] = {
    { NULL, NULL },
    { gMapAkropolisEastElevatorHallVariant1Placements, _gAcropolisEastElevatorHallAreaResources },
    { NULL, NULL },
};

/// View 1's empty sprite list; the zero first count retains background image strips.
///
/// The separate terminal record is required after the renderer skips that header.
static SpriteBatch _gAcropolisEastElevatorHallView1SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 39 depth-sorted foreground texture rectangles.
///
/// Positions and sizes use pixels relative to the view centre; depth is unscaled
/// OT depth. Batches select contiguous source-element ranges. Room storage stays
/// writable and live through cached packet allocation and drawing.
static SpriteSource _gAcropolisEastElevatorHallView2SpriteSources[] = {
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, 8, 1104, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, 32, 1151, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -72, 0, 1246, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -72, 24, 1175, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -72, 48, 1175, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -56, 0, 1306, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -56, 16, 1325, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -56, 32, 1325, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 104, -48, 1209, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 104, -16, 1243, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 104, 16, 1279, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 120, -48, 1184, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 120, -32, 1208, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 120, -8, 1241, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 88, -16, 1475, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 88, 8, 1452, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 88, 32, 1330, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 96, -16, 1253, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 96, 8, 1280, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 96, 32, 1301, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 88, -48, 1475, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 96, -48, 1300, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -16, -88, 1803, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -16, -56, 1851, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -16, -32, 1800, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 80, -88, 1639, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, -64, 1667, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, -48, 1718, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 72, -32, 1727, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -32, -16, 1832, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -32, 0, 1936, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, -16, 1933, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 0, 1937, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 0, -8, 1889, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 24, -8, 1842, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 48, -8, 1775, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 72, -8, 1734, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 64, 16, 1791, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 80, 16, 1765, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
};

/// Source-element boundary between view 2's two authored sprite batches.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_2_FIRST_BATCH_COUNT = 22 };

/// View 2's background header, 22-sprite range, remaining 17-sprite range and end.
///
/// Both ranges participate in cached drawing; the first range's serialized byte
/// field_6[0] is 1, with its role unproven. Keep the mutable list loaded for drawing.
static SpriteBatch _gAcropolisEastElevatorHallView2SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_2_FIRST_BATCH_COUNT, 0, 0, { 1, 0 } },
    { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_2_FIRST_BATCH_COUNT, ARRAY_SIZE(_gAcropolisEastElevatorHallView2SpriteSources) - ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_2_FIRST_BATCH_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 41 foreground rectangles, with separate palette rows for its two ranges.
///
/// Positions/sizes use view-centred pixels and depth is unscaled OT depth; the
/// loaded room owns the writable sources borrowed for packet allocation/drawing.
static SpriteSource _gAcropolisEastElevatorHallView3SpriteSources[] = {
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 104 } }, 64, -48, 1650, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, 80, -56, 1250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 120 } }, 96, -56, 1225, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -160, 88, 651, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -152, 88, 679, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -144, 80, 731, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -136, 72, 792, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -128, 64, 865, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -120, 64, 942, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -112, 48, 1067, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -104, 40, 1209, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -96, 32, 1398, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -88, 24, 1659, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -80, 16, 2050, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 48 } }, -152, 40, 733, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -160, 48, 647, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -144, 40, 769, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -136, 32, 777, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -128, 32, 919, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -120, 24, 971, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -112, 24, 1132, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -104, 16, 1285, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -96, 8, 1487, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -88, 0, 1722, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -80, 0, 2066, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -112, 0, 1137, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -112, -24, 1141, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -112, -48, 1088, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -104, 0, 1151, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -88, -24, 2073, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -88, -40, 2085, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, -96, -64, 1660, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -112, -64, 1203, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -120, -64, 1021, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -96, -88, 1488, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -112, -96, 1075, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -128, -96, 858, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -144, -104, 740, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -160, -112, 655, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -120, -120, 975, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -136, -120, 857, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

/// Source-element boundary between view 3's two authored sprite batches.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_3_FIRST_BATCH_COUNT = 3 };

/// View 3's background header, 3-sprite range, remaining 38-sprite range and end.
///
/// Both ranges participate in cached drawing; the first range's serialized byte
/// field_6[0] is 1, with its role unproven. Keep the mutable list loaded for drawing.
static SpriteBatch _gAcropolisEastElevatorHallView3SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_3_FIRST_BATCH_COUNT, 0, 0, { 1, 0 } },
    { ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_3_FIRST_BATCH_COUNT, ARRAY_SIZE(_gAcropolisEastElevatorHallView3SpriteSources) - ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_3_FIRST_BATCH_COUNT, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 38 depth-sorted foreground texture rectangles.
///
/// Positions/sizes use view-centred pixels and depth is unscaled OT depth; the
/// loaded room owns the writable sources borrowed for packet allocation/drawing.
static SpriteSource _gAcropolisEastElevatorHallView4SpriteSources[] = {
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -16, 32, 1604, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 32, 1576, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -88, 40, 1474, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -120, 40, 1389, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -120, 8, 1348, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -128, -16, 1312, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -128, -40, 1297, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -136, -88, 1207, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -96, -88, 1304, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, -56, -88, 1363, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -16, -88, 1420, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 16, -88, 1507, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -40, 16, 1548, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -40, 0, 1514, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -40, -16, 1483, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -40, -40, 1442, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -24, -40, 1398, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, -40, 1522, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, -24, 1552, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, -8, 1584, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, 8, 1617, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -16, 24, 1603, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 16, 8, 1559, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 16, -16, 1532, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 16, -40, 1569, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 32, -40, 1495, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 32, -16, 1583, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 32, 8, 1622, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, -40, 1350, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, -40, 1370, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, -24, 1393, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, 8, 1360, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 32, 1522, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, 24, 1465, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, 16, 1370, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -96, -8, 1377, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -96, -40, 1397, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, -8, 1328, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

/// View 4's background header, one range covering every source sprite, and end.
///
/// The range is initially visible and included in cached drawing; the room owns
/// the writable list for the duration of allocation and drawing.
static SpriteBatch _gAcropolisEastElevatorHallView4SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisEastElevatorHallView4SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 28 depth-sorted foreground texture rectangles.
///
/// Positions/sizes use view-centred pixels and depth is unscaled OT depth; the
/// loaded room owns the writable sources borrowed for packet allocation/drawing.
static SpriteSource _gAcropolisEastElevatorHallView5SpriteSources[] = {
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 32, 1056, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 48, 1002, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 48, -80, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 48, -56, 775, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 88, 56, 875, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 72, -88, 650, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 72, -48, 800, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 72, -8, 850, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 72 } }, 96, -80, 800, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 64 } }, 96, -8, 800, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 32, 16, 1000, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, 32, 950, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 40, 32, 1000, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, 48, 935, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, 56, 900, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 72, 72, 912, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, 64, 948, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, -32, 800, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, -16, 875, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 0, 941, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -32, 800, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, -16, 850, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 0, 888, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, -32, 760, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, -16, 812, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, 0, 820, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 16, 927, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { getTPage(1, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 56, 16, 880, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
};

/// View 5's background header, one range covering every source sprite, and end.
///
/// The range is initially visible and included in cached drawing; the room owns
/// the writable list for the duration of allocation and drawing.
static SpriteBatch _gAcropolisEastElevatorHallView5SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisEastElevatorHallView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's empty sprite list, retaining background image strips before its end.
///
/// The separate terminal record is required after the renderer skips the header.
static SpriteBatch _gAcropolisEastElevatorHallView6SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's empty sprite list, retaining background image strips before its end.
///
/// The separate terminal record is required after the renderer skips the header.
static SpriteBatch _gAcropolisEastElevatorHallView7SpriteBatches[] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gAcropolisEastElevatorHallSpriteViews[ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT] = {
    { { .empty = _gAcropolisEastElevatorHallView1SpriteBatches }, _gAcropolisEastElevatorHallView1SpriteBatches, NULL },
    { { .elements = _gAcropolisEastElevatorHallView2SpriteSources }, _gAcropolisEastElevatorHallView2SpriteBatches, NULL },
    { { .elements = _gAcropolisEastElevatorHallView3SpriteSources }, _gAcropolisEastElevatorHallView3SpriteBatches, NULL },
    { { .elements = _gAcropolisEastElevatorHallView4SpriteSources }, _gAcropolisEastElevatorHallView4SpriteBatches, NULL },
    { { .elements = _gAcropolisEastElevatorHallView5SpriteSources }, _gAcropolisEastElevatorHallView5SpriteBatches, NULL },
    { { .empty = _gAcropolisEastElevatorHallView6SpriteBatches }, _gAcropolisEastElevatorHallView6SpriteBatches, NULL },
    { { .empty = _gAcropolisEastElevatorHallView7SpriteBatches }, _gAcropolisEastElevatorHallView7SpriteBatches, NULL },
};

/// Four authored point lights for model shading in every east elevator hall view.
///
/// Positions and falloff radii use world units; RGB intensities have 12 fractional
/// bits (`ONE` is full strength). The room overlay owns this writable storage:
/// coordinate updates set its view parent and cached transforms, and lighting
/// queries overwrite attenuation. Borrowed pointers expire when the room unloads.
static WorldCoordPointLight _gAcropolisEastElevatorHallPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3250, -2350, 810 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5324, 4915, 4505 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 110, -2350, 270 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5324, 4915, 4505 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3332,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3400, -2350, 180 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5324, 4915, 4505 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4070, -1700, -1420 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 2000,
    },
};

/// The hall's light collection: four all-view point lights, no directional or cone lights.
///
/// The room-lighting table borrows this one collection. Coordinate updates and
/// queries mutate the referenced lights; keep the collection and overlay loaded.
static WorldCoordRoomLights _gAcropolisEastElevatorHallRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisEastElevatorHallPointLights),
    .pointLights           = _gAcropolisEastElevatorHallPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

ViewCamera gAcropolisEastElevatorHallViewCameras[ACROPOLIS_EAST_ELEVATOR_HALL_VIEW_COUNT] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 100, 30000, -500 } }, 617 },
    { { { { 952, 0, -3983 }, { -880, 3994, -210 }, { 3885, 905, 928 } }, { 1724, 2100, 547 } }, 230 },
    { { { { 1225, 0, 3908 }, { 221, 4089, -69 }, { -3901, 232, 1223 } }, { -3673, 1410, 858 } }, 230 },
    { { { { 1157, 0, 3928 }, { 1144, 3918, -337 }, { -3758, 1192, 1107 } }, { 274, 2690, -202 } }, 230 },
    { { { { -2087, 0, -3524 }, { -2521, 2861, 1493 }, { 2462, 2930, -1458 } }, { -2676, 3435, -318 } }, 257 },
    { { { { 950, 0, 3984 }, { 685, 4034, -163 }, { -3924, 705, 936 } }, { -5446, 1425, 1431 } }, 230 },
    { { { { -869, 0, 4002 }, { 3549, 1892, 771 }, { -1849, 3632, -401 } }, { -5320, 2510, 990 } }, 230 },
};

/// Loaded-bank script bases for the hall's paired walking and running cues.
enum {
    ACROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 17,
    ACROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 19,
};

/// Footstep cue bases for ordinary hall surfaces; scripted jumps reuse walking.
///
/// Cue 2 uses the base and cue 1 adds 1; companion playback adds 100. Surface
/// records borrow this room-owned storage while the overlay is loaded.
static WorldCollisionFootstepSounds _gAcropolisEastElevatorHallFootstepSounds = {
    .walk         = ACROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_WALK,
    .run          = ACROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_RUN,
    .scriptedJump = ACROPOLIS_EAST_ELEVATOR_HALL_FOOTSTEP_WALK,
};

/// Ordinary hall surface policy: block probes, allow weapon impacts and apply pushback.
///
/// All classes except 1 share this record and its footstep cues while the room
/// overlay is loaded. The first byte's role is unproven and retains zero.
static WorldCollisionSurfaceProperties _gAcropolisEastElevatorHallDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisEastElevatorHallFootstepSounds,
};

/// Hall class 1's policy: block probes and apply pushback, without impacts or footstep cues.
///
/// The surface directory borrows this room-owned record; its first byte's role
/// is unproven and retains zero.
static WorldCollisionSurfaceProperties _gAcropolisEastElevatorHallClass1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gAcropolisEastElevatorHallSurfacePropertiesByClass[8] = {
    &_gAcropolisEastElevatorHallDefaultSurfaceProperties,
    &_gAcropolisEastElevatorHallClass1SurfaceProperties,
    &_gAcropolisEastElevatorHallDefaultSurfaceProperties,
    &_gAcropolisEastElevatorHallDefaultSurfaceProperties,
    &_gAcropolisEastElevatorHallDefaultSurfaceProperties,
    &_gAcropolisEastElevatorHallDefaultSurfaceProperties,
    &_gAcropolisEastElevatorHallDefaultSurfaceProperties,
    &_gAcropolisEastElevatorHallDefaultSurfaceProperties,
};

#include "../../shared/planar_reflection.inc.c"

void acropolisEastElevatorHallPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Allows a room transition with the requested destination unchanged.
///
/// The resolve message borrows a readable eight-byte request and a writable
/// reply for this call; they may be the same record. Copies the entire record
/// for both queries and execution requests and returns 1 (transition allowed).
/// The receiving task and message ID are unused; no payload storage is retained.
static s32 _acropolisEastElevatorHallResolveTransitionMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { ROOM_EVENT_RESULT_ALLOWED = 1 };

    *reply = *request;
    return ROOM_EVENT_RESULT_ALLOWED;
}

/// Refuses every key-item use request in this room.
///
/// `itemId` is the collected inventory item's ID and `unusedArg` is the unused
/// second message word. All arguments are ignored. Returns 0, which tells the
/// inventory menu that the item cannot be used here; no item is consumed.
static s32 _acropolisEastElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ROOM_KEY_ITEM_RESULT_REFUSED = 0 };

    return ROOM_KEY_ITEM_RESULT_REFUSED;
}

/// Records the opening event's start and enables its subsequent story interactions.
///
/// Requires the live hall visit and game-flag bank. Latches this visit, selects
/// Acropolis progress 1 and objective 2, resets the follow-up dialogue state,
/// selects dialogue index 3 and unlocks the patio's cafeteria door. Records
/// progress at scene start, so skipping the scene retains the same story state.
static inline void _acropolisEastElevatorHallRecordOpeningProgress(void)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_STARTED         = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_FOLLOW_UP_RESET         = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_NEXT_DIALOGUE_INDEX     = 3,
        ACROPOLIS_EAST_ELEVATOR_HALL_CAFETERIA_DOOR_UNLOCKED = 2,
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_OBJECTIVE       = 2
    };

    _gAcropolisEastElevatorHallOpeningEventStarted = ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_STARTED;
    gameFlagSetNibble(GAME_FLAG_ACROPOLIS_PROGRESS, ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_STARTED);
    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, ACROPOLIS_EAST_ELEVATOR_HALL_FOLLOW_UP_RESET);
    gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACROPOLIS_EAST_ELEVATOR_HALL_NEXT_DIALOGUE_INDEX);
    gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, ACROPOLIS_EAST_ELEVATOR_HALL_CAFETERIA_DOOR_UNLOCKED);
    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_OBJECTIVE);
}

/// Starts the east elevator hall's opening scene once from room action zero.
///
/// Borrows a readable four-byte action request only during dispatch. Starts the
/// skippable scene and records story progress only while Acropolis progress and
/// the visit latch are both zero. Receiver, message ID and second word are unused;
/// no request storage is retained. Always returns zero; the sender ignores it.
static s32 _acropolisEastElevatorHallStartOpeningEvent(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_ACTION_ID   = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_NOT_STARTED = 0
    };

    if (actionRequest->actionId == ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_ACTION_ID &&
        gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) == ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_NOT_STARTED &&
        _gAcropolisEastElevatorHallOpeningEventStarted == ACROPOLIS_EAST_ELEVATOR_HALL_OPENING_NOT_STARTED) {
        evsStartScriptWithSkip(_gAcropolisEastElevatorHallOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisEastElevatorHallOpeningSkipScript);
        _acropolisEastElevatorHallRecordOpeningProgress();
    }
    return 0;
}

/// Runs CAP sequence 2 with the room's scripted actor animation for command 2.
///
/// Other commands do nothing. Receiver, message ID and execution mode are unused;
/// always returns zero. The room and its script resources must remain loaded.
static s32 _acropolisEastElevatorHallHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedExecutionMode)
{
    enum { ACROPOLIS_EAST_ELEVATOR_HALL_COMMAND_CAP_SEQUENCE_2 = 2 };

    if (commandId == ACROPOLIS_EAST_ELEVATOR_HALL_COMMAND_CAP_SEQUENCE_2) {
        evsStartScript(_gAcropolisEastElevatorHallCommand2Script, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Starts CAP sequence 16 at the end of the opening scene's skip script.
///
/// Requires the hall's CAP file loaded; uses transition playback and variant 0.
static void _acropolisEastElevatorHallStartSkipCapSequence(void)
{
    enum { ACROPOLIS_EAST_ELEVATOR_HALL_SKIP_CAP_SEQUENCE = 16 };
    capStartSequenceSlot(ACROPOLIS_EAST_ELEVATOR_HALL_SKIP_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
}

/// Registers the hall's room receiver and starts placed actor 0's initial animation.
///
/// Requires a live bodyless task in state 0, initialized placements and loaded
/// animation resources. Publishes `GAME_TASK_SLOT_ROOM`, enables actor drawing
/// and borrows its animation request synchronously, then enters state 1.
static void _acropolisEastElevatorHallInitializeRoomTask(Task* task)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_INITIAL_PLACED_ACTOR      = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_PLACED_ACTOR_DRAW_ENABLED = 1
    };
    task->msgTable = _gAcropolisEastElevatorHallRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sceneSetPlacedActorDrawMode(ACROPOLIS_EAST_ELEVATOR_HALL_INITIAL_PLACED_ACTOR, ACROPOLIS_EAST_ELEVATOR_HALL_PLACED_ACTOR_DRAW_ENABLED);
    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_EAST_ELEVATOR_HALL_INITIAL_PLACED_ACTOR), ACTOR_MESSAGE_PLAY_ANIMATION, &_gAcropolisEastElevatorHallViewFigureDefaultAnimationRequest, 0);
    task->state++;
}

/// Player diagnostic label, with its eight-byte stored representation retained.
///
/// The visible string ends after "Player". The final byte has no established
/// display meaning; keep it and the rodata position intact.
static const char _gAcropolisEastElevatorHallPlayerDebugLabel[8] = "Player\0\x0F";

/// Updates the player's diagnostic display during the room task's idle state.
///
/// With a nonzero debug mode, calls the player diagnostic hook, then checks the
/// mode again before passing the player label and display position (-140, -50).
/// The receiver is unused; the player slot and diagnostic hooks must be live.
static void _acropolisEastElevatorHallUpdatePlayerDebugDisplay(Task* unusedTask)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_X = -140,
        ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_Y = -50
    };

    if (gDisplayState.debugMode != 0) {
        func_807245E4(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        if (gDisplayState.debugMode != 0) {
            func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_X,
                          ACROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_Y, _gAcropolisEastElevatorHallPlayerDebugLabel);
        }
    }
}

void acropolisEastElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAcropolisEastElevatorHallRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// First red beacon's offset under the effect controller's parent coordinate.
///
/// Signed whole game-coordinate units; copied to writable local storage before
/// spawning the six one-frame beacons in view 2. The parent coordinate must stay live.
static const SVECTOR _gAcropolisEastElevatorHallFirstBeaconOffset = { 0x1600, -0x964, 0x540, 0 };

void acropolisEastElevatorHallReflectionsAndBeaconsTask(Task* task)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_INITIALIZE   = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_UPDATE       = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT = 0x25,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_FLOOR     = 0,
        ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_MIRROR    = 1,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_VIEW          = 2,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE    = 3,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE    = 12,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE    = 4,
        ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE    = 18
    };
    GfxCoord* parentCoord;

    parentCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_INITIALIZE:
            taskSpawn(ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_FLOOR, 0);
            taskSpawn(ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT, ACROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_MIRROR, 0);
            task->state++;
            /* fallthrough */
        case ACROPOLIS_EAST_ELEVATOR_HALL_EFFECTS_UPDATE:
            // Each beacon lasts one frame, so refresh all six while this view is active.
            if (gGameSession->location.loc.view == ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_VIEW) {
                SVECTOR spawnOffset = _gAcropolisEastElevatorHallFirstBeaconOffset;

                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0x985;
                spawnOffset.vz = 0x55;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0xA81;
                spawnOffset.vz = -0x1CA;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0xA93;
                spawnOffset.vz = -0x61C;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0x460;
                spawnOffset.vz = -0x1A1;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
                spawnOffset.vx = 0x1600;
                spawnOffset.vy = -0x449;
                spawnOffset.vz = -0x635;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &spawnOffset);
            }
            break;
    }
}

#include "../../shared/red_beacon_task.inc.c"
#undef RED_BEACON_TASK

/// Projects a view-space translation and queues a single additive gray pixel.
///
/// `composedCoord` must have a current view-space composition. Only its cached
/// x/y/z translation is read, in signed 32-bit game-coordinate units, and each
/// component narrows to signed 16 bits before projection through `GsWSMATRIX`.
/// The GTE projection geometry must already be configured; FLAG is not tested.
/// `tileScratch` borrows one complete, word-aligned `EffectPointTileScratch`
/// block for this call. Neither input nor scratch storage is retained.
///
/// The word-aligned frame arena needs one `TILE_1` even at rejected depths and
/// one additional `DR_TPAGE` when SZ3 / 4 is at least `EFFECT_POINT_TILE_MIN_DEPTH`.
/// The current ordering table must provide 1024 depth tags; scaled depths wrap
/// into those tags. Queued packets remain live until GPU drawing completes.
/// Changes GTE state and leaves additive GPU draw mode active after the
/// accepted pixel.
static inline void _acropolisEastElevatorHallDrawPointTile(const GfxCoord* composedCoord, EffectPointTileScratch* tileScratch)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR                   = 0x80,
        ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_DEPTH_TO_OT_BYTES_SHIFT = 2,
    };

    TILE_1* tile;

    // Narrow the composed view position before perspective projection.
    tileScratch->viewPoint.vx = composedCoord->workm.t[0];
    tileScratch->viewPoint.vy = composedCoord->workm.t[1];
    tileScratch->viewPoint.vz = composedCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->viewPoint);
    gte_rtps();
    // Reserve the tile even when its depth will reject it.
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile1(tile);
    gte_stsxy(&tile->x0);
    gte_stszotz(&tileScratch->depth);
    if (tileScratch->depth >= EFFECT_POINT_TILE_MIN_DEPTH) {
        setRGB0(tile, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)tileScratch->depth << gDisplayState.otDepthShift) >> ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                tile);
        // Prepend the draw-mode command at the same depth so blending is set before the tile.
        gpuSetPrimitiveBlendMode(tile, GPU_BLEND_ADD, tileScratch->depth);
    }
}

void acropolisEastElevatorHallPointTileTask(Task* task)
{
    EffectPointTileScratch* tileScratch;
    GfxCoord*               effectCoord;
    void*                   effectWork;

    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
    _acropolisEastElevatorHallDrawPointTile(effectCoord, tileScratch);
    SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    effectKillTask(effectWork, task);
}
