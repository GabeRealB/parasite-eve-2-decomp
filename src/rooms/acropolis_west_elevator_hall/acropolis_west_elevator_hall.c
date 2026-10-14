#include "rooms/acropolis_west_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
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

#include "rooms/room.h"

#include "rooms/room_common.h"

/// Room-coordinate X where both elevator-door leaves meet when shut, in whole units.
#define ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X (-1000)

/// Whole room-coordinate units added to a door leaf's travel per motion update.
///
/// The task's signed direction multiplies this step: -1 closes, 0 rests and
/// 1 opens. Mirroring affects placement along X, not the accumulated travel.
#define ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_SPEED 20

/// Inclusive upper clamp of a retracted door leaf's travel, in room-coordinate units.
///
/// Travel is clamped to 0..720 before the selected side (-1 or 1) places the
/// leaf along X from its shut position. At 20 units per update this takes
/// 36 motion updates from shut to fully open.
#define ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX 720

/// Work block of one leaf of the elevator's sliding door, held in `Task::work`.
///
/// The door is two mirrored leaves, each its own task with its own block. The
/// task's `spawnArg1` selects opening (1), closing (-1) or rest (0), and its
/// `spawnArg2` the side the leaf retracts to (-1 or 1 along X).
typedef struct {
    s32 travel; // World units slid from the shut position, 0 to `ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX`
} _AcropolisWestElevatorHallDoorLeafWork;
STATIC_ASSERT_SIZEOF(_AcropolisWestElevatorHallDoorLeafWork, 4);

static Task* _gAcropolisWestElevatorHallDoorLeafTasks[2];

/// Keeps the reflection scale at this room's earlier rodata position.
///
/// 0 requires `planar_reflection_rodata.inc.c` before the shared implementation.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 0
#include "../../shared/room/planar_reflection.h"
/// Binds the shared beacon body to this room's public `void (Task*)` callback.
///
/// Required before `red_beacon.h` and its task fragment; gameplay imports this instance.
#define RED_BEACON_TASK acropolisWestElevatorHallRedBeaconTask
#include "../../shared/effects/red_beacon.h"

static void _acropolisWestElevatorHallUpdateArrivalEvent(Task* unusedTask);
static void _acropolisWestElevatorHallInitRoomTask(Task* task);
static void _acropolisWestElevatorHallInitializeDoorLeaf(Task* task);
static void _acropolisWestElevatorHallUpdateDoorLeaf(Task* task);

/// Scale applied to held-object reflections in slots 2 and up: it mirrors
/// them across X.
#include "../../shared/room/planar_reflection_rodata.inc.c"

/// Room-task states: 0 register/spawn leaves, 1 maintain the arrival event, 2 kill.
///
/// Dispatch copies all three callbacks by value and indexes the copy without
/// checking bounds. The room overlay must remain loaded through each call.
static const TaskFuncTable3 _gAcropolisWestElevatorHallRoomTaskStates = {
    { _acropolisWestElevatorHallInitRoomTask, _acropolisWestElevatorHallUpdateArrivalEvent, taskKill },
};

/// Door-leaf states: 0 initialize, 1 slide/draw, 2 kill and release its work.
///
/// Dispatch copies all three callbacks by value; the unchecked state is 0..2.
/// Requires the leaf model and this overlay to remain loaded until teardown.
static const TaskFuncTable3 _gAcropolisWestElevatorHallDoorLeafStates = {
    { _acropolisWestElevatorHallInitializeDoorLeaf, _acropolisWestElevatorHallUpdateDoorLeaf, taskKill },
};

/// Parent-coordinate position of the first of view 2's three red beacons.
///
/// XYZ are signed whole game units. Each spawn snapshots those components
/// before the temporary position is reused for the remaining beacons.
static const SVECTOR _gAcropolisWestElevatorHallFirstBeaconPosition = { -0x1518, -0x720, 0xAC, 0 };

/// Parent-coordinate position of the elevator-bay light glow in view 5.
///
/// XYZ are signed whole game units; spawning snapshots those three components.
static const SVECTOR _gAcropolisWestElevatorHallBayLightPosition = { -0x79, -0x876, 0x703, 0 };

static s32 _acropolisWestElevatorHallOpenDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32 _acropolisWestElevatorHallCloseDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32 _acropolisWestElevatorHallResolveRoomEventMessage(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisWestElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _acropolisWestElevatorHallStartBayLightingMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Commands sent by the arrival script to the room and room-effect tasks.
enum {
    ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_OPEN_DOOR          = 5100,
    ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_CLOSE_DOOR         = 5101,
    ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_START_BAY_LIGHTING = 3100,
};

/// Signed travel requests stored in a door leaf's first spawn argument.
enum {
    ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE = -1,
    ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_OPEN  = 1,
};

/// Key-item use request sent to the room task by the inventory menu.
enum { ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Arrival-event phase for this overlay load; entering through warp 1 starts it.
enum {
    ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WAITING  = 0,
    ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_RUNNING  = 1,
    ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_FINISHED = 2,
};

/// Door-leaf descriptor and live-handle indices, ordered by retraction along X.
enum {
    ACROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX = 0,
    ACROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX = 1,
    ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY  = 192,
};

/// View slots, clip and audio cues used by the elevator arrival and its skip path.
enum {
    ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_BAY_VIEW       = 5,
    ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_HALL_VIEW      = 2,
    ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_DOOR_VIEW       = 4,
    ACROPOLIS_WEST_ELEVATOR_HALL_PLAYER_IDLE_CLIP       = 1,
    ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_MUSIC          = 30,
    ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_SOUND          = 0x51110003,
    ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE_SOUND       = 0x51110004,
    ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ARRIVAL_SOUND   = 0x51110002,
    ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_DEPARTURE_SOUND = 0x51110001,
    ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ENTRY_MAP_FLAG  = 503,
};

static AnimationSet _gAcropolisWestElevatorHallAnimation06F80;
static TmdSource    _gAcropolisWestElevatorHallModel02DE8;
static TmdSource    _gAcropolisWestElevatorHallModel03058;
static void         _acropolisWestElevatorHallDoorLeafTask(Task* task);

static WorldCollisionGrid    _gAcropolisWestElevatorHallCollisionGrid;
static WorldCollisionTrigger _gAcropolisWestElevatorHallViewBoundaryTriggers[4];
static WorldCollisionTrigger _gAcropolisWestElevatorHallActionTriggers[5];
static WorldCoordRoomLights  _gAcropolisWestElevatorHallRoomLights;

#include "../../shared/room/planar_reflection_data.inc.c"

/// Execution priority shared by the hall's reflected player and held models.
enum { ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_PRIORITY = 112 };

/// Spawn recipes for the hall's player reflection and its attachment reflections.
///
/// Slots 0 and 1 start bodyless at priority 112; their callbacks own model clones.
/// There is no terminator. Borrowed descriptors remain valid while this room is loaded.
static TaskDesc _gAcropolisWestElevatorHallReflectionTaskTable[2] = {
    { { { TASK_BODY_NONE, ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, acropolisWestElevatorHallPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return _gAcropolisWestElevatorHallReflectionTaskTable;
}

static TmdBone _gAcropolisWestElevatorHallModel02DE8Skeleton[1] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_skeleton.inc"
};

static u32 _gAcropolisWestElevatorHallModel02DE8PartVerts[1] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_partVerts.inc"
};

static SVECTOR _gAcropolisWestElevatorHallModel02DE8Verts[24] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_verts.inc"
};

static u32 _gAcropolisWestElevatorHallModel02DE8Stream[89] = {
#include "assets/acropolis_west_elevator_hall_model_02DE8_stream.inc"
};

static TmdSource _gAcropolisWestElevatorHallModel02DE8 = {
    0,
    576,
    0,
    1,
    _gAcropolisWestElevatorHallModel02DE8PartVerts,
    _gAcropolisWestElevatorHallModel02DE8Verts,
    &_gAcropolisWestElevatorHallModel02DE8Verts[24],
    _gAcropolisWestElevatorHallModel02DE8Skeleton,
    _gAcropolisWestElevatorHallModel02DE8Stream,
};

static TmdBone _gAcropolisWestElevatorHallModel03058Skeleton[1] = {
#include "assets/acropolis_west_elevator_hall_model_03058_skeleton.inc"
};

static u32 _gAcropolisWestElevatorHallModel03058PartVerts[1] = {
#include "assets/acropolis_west_elevator_hall_model_03058_partVerts.inc"
};

static SVECTOR _gAcropolisWestElevatorHallModel03058Verts[24] = {
#include "assets/acropolis_west_elevator_hall_model_03058_verts.inc"
};

static u32 _gAcropolisWestElevatorHallModel03058Stream[89] = {
#include "assets/acropolis_west_elevator_hall_model_03058_stream.inc"
};

static TmdSource _gAcropolisWestElevatorHallModel03058 = {
    0,
    576,
    0,
    1,
    _gAcropolisWestElevatorHallModel03058PartVerts,
    _gAcropolisWestElevatorHallModel03058Verts,
    &_gAcropolisWestElevatorHallModel03058Verts[24],
    _gAcropolisWestElevatorHallModel03058Skeleton,
    _gAcropolisWestElevatorHallModel03058Stream,
};

static AnimationPackedPose _gAcropolisWestElevatorHallAnimation06F80Bank1[100] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_bank1.inc"
};

static AnimationPackedRotation _gAcropolisWestElevatorHallAnimation06F80Bank4[1644] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_bank4.inc"
};

static AnimationRecord _gAcropolisWestElevatorHallAnimation06F80Records[1990] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_records.inc"
};

static u16 _gAcropolisWestElevatorHallAnimation06F80Indices[20] = {
#include "assets/acropolis_west_elevator_hall_animation_06F80_indices.inc"
};

static AnimationSet _gAcropolisWestElevatorHallAnimation06F80 = {
    _gAcropolisWestElevatorHallAnimation06F80Records,
    _gAcropolisWestElevatorHallAnimation06F80Indices,
    { NULL, _gAcropolisWestElevatorHallAnimation06F80Bank1, NULL, NULL, _gAcropolisWestElevatorHallAnimation06F80Bank4, NULL, NULL, NULL },
};

/// Model-backed door-leaf descriptors, negative X then positive X, followed by END.
///
/// Both run at priority 192. Spawning reads the descriptor synchronously;
/// geometry and callbacks remain borrowed until each leaf task ends.
static TaskDesc _gAcropolisWestElevatorHallDoorLeafTaskTable[3] = {
    { { { TASK_BODY_TMD, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY } }, _acropolisWestElevatorHallDoorLeafTask, { .model = &_gAcropolisWestElevatorHallModel02DE8 } },
    { { { TASK_BODY_TMD, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY } }, _acropolisWestElevatorHallDoorLeafTask, { .model = &_gAcropolisWestElevatorHallModel03058 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// The single borrowed clip installed in the player's equipped-bank extension.
///
/// Copied into set slot `ANIMATION_BANK_BASE_SET_COUNT`; the set and its pools
/// remain loaded while the arrival animation plays.
static AnimationSet* _gAcropolisWestElevatorHallArrivalAnimationSets[1] = {
    &_gAcropolisWestElevatorHallAnimation06F80,
};

/// Installs exactly one arrival-clip pointer in the player's bank extension.
///
/// Message dispatch borrows this request and source table synchronously;
/// playback continues borrowing the copied animation-set pointer.
static AnimationBankCopyRequest _gAcropolisWestElevatorHallArrivalAnimationBankCopyRequest = { { .sets = _gAcropolisWestElevatorHallArrivalAnimationSets }, ARRAY_SIZE(_gAcropolisWestElevatorHallArrivalAnimationSets) };

/// Plays the installed arrival clip from the equipped bank with collision disabled.
///
/// The event interpreter copies the request and replaces `source.index` before
/// dispatch. Playback resets immediately, so `blendFrames` is unused.
static AnimationPlayRequest _gAcropolisWestElevatorHallArrivalAnimationRequest = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Off-stage player placement while the elevator bay lights and doors open.
///
/// Position uses whole room units; Euler angles use 4096 units per turn.
/// The placement message copies XYZ and all three angles synchronously.
static ActorTransform _gAcropolisWestElevatorHallPlayerArrivalHiddenTransform = { { 10000, 0, 2527, 0 }, { 0, 2048, 0, 0 } };

/// Player placement at the opened doorway before the arrival clip starts.
///
/// Position uses whole room units; Euler angles use 4096 units per turn.
/// The placement message copies XYZ and all three angles synchronously.
static ActorTransform _gAcropolisWestElevatorHallPlayerDoorwayTransform = { { 220, 0, 2650, 0 }, { 0, 2048, 0, 0 } };

/// Unreferenced 24-byte record equal to the doorway transform; its role is unproven.
static ActorTransform D_acropolis_west_elevator_hall_801845DC = { { 220, 0, 2650, 0 }, { 0, 2048, 0, 0 } };

/// Final player placement shared by normal arrival and the skip path.
///
/// Position uses whole room units; Euler angles use 4096 units per turn.
/// The placement message copies XYZ and all three angles synchronously.
static ActorTransform _gAcropolisWestElevatorHallPlayerArrivalEndTransform = { { -1388, 0, 922, 0 }, { 0, 2048, 0, 0 } };

/// Resets the player's equipped-bank idle clip and disables world collision.
///
/// Used before normal arrival and after skipped placement. The interpreter
/// replaces `source.index` in its local copy; `blendFrames` is unused on reset.
static AnimationPlayRequest _gAcropolisWestElevatorHallPlayerIdleAnimationRequest = { { .index = 1 }, ACROPOLIS_WEST_ELEVATOR_HALL_PLAYER_IDLE_CLIP, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Warp-1 arrival: light the bay, open the doors, play the arrival clip and resume play.
///
/// The 26-command stream includes END; waits count interpreter updates. It and
/// its payloads remain borrowed until the event ends. Views are 1-based slots.
static EvsCommand _gAcropolisWestElevatorHallArrivalScript[26] = {
    // Take scripted control, install the room clip and park the player outside the scene.
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 67 }, { .value = 67 }, { .value = 78 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_RUN_MOVEMENT }, { .value = true }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisWestElevatorHallPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisWestElevatorHallArrivalAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisWestElevatorHallPlayerArrivalHiddenTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_BAY_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM_EFFECT }, { .value = 0 }, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_START_BAY_LIGHTING }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM }, { .value = 0 }, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_OPEN_DOOR }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Play the installed clip at the doorway, then close the leaves behind the player.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisWestElevatorHallPlayerDoorwayTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisWestElevatorHallArrivalAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM }, { .value = 0 }, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_CLOSE_DOOR }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore hall presentation and normal control before the final placement.
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_HALL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_RUN_MOVEMENT }, { .value = false }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 130 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_RESET_ANIMATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisWestElevatorHallPlayerArrivalEndTransform } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skipped arrival: fade to the final placement, restore the hall and resume play.
///
/// The 13-command stream includes END and performs scene cleanup without
/// issuing door commands. Commands and payloads stay loaded until event end.
static EvsCommand _gAcropolisWestElevatorHallArrivalSkipScript[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisWestElevatorHallPlayerArrivalEndTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisWestElevatorHallPlayerIdleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_HALL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// One-shot arrival-event latch (0 waiting, 1 running, 2 finished) for this overlay load.
///
/// Set to running only on warp-1 entry and finished when the session event is
/// idle. Reloading the overlay restores waiting; ordinary room-task ticks do not.
static s32 _gAcropolisWestElevatorHallArrivalPhase = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WAITING;

/// Room messages: open/close the door, resolve transitions and reject key items, then END.
///
/// Dispatch borrows this table for the room task's lifetime. Door commands
/// require both leaf handles live; transition messages borrow request/reply records.
static TaskMessageEntry _gAcropolisWestElevatorHallRoomMessages[5] = {
    { ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_OPEN_DOOR, _acropolisWestElevatorHallOpenDoorMessage },
    { ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_CLOSE_DOOR, _acropolisWestElevatorHallCloseDoorMessage },
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisWestElevatorHallResolveRoomEventMessage },
    { ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM, _acropolisWestElevatorHallRejectKeyItemMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Room-effect message to start the bay-light ramp, followed by END.
///
/// Dispatch borrows the table while the view-effects task is live. Its sole
/// handler ignores both payload words and returns zero after spawning the effect.
static TaskMessageEntry _gAcropolisWestElevatorHallViewEffectMessages[2] = {
    { ACROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_START_BAY_LIGHTING, _acropolisWestElevatorHallStartBayLightingMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Black RGB555 palette used while the elevator bay is unlit.
static u16 _gAcropolisWestElevatorHallBayUnlitClut[256] = { 0 };

/// Authored 256-colour RGB555 palette for the illuminated elevator bay.
static u16 _gAcropolisWestElevatorHallBayLitClut[256] = {
#include "assets/acropolis_west_elevator_hall_clut_07644.inc"
};

/// Complete RGB555 bay palette interpolated between black and the lit colours.
///
/// Written in sixteen-colour rows. The GPU borrows all 512 bytes as packed
/// words on upload; keep this room loaded and the colours intact through completion.
static u16 _gAcropolisWestElevatorHallBayBlendedClut[256] = { 0 };

/// VRAM row of the elevator bay's 256-colour lookup table.
enum { ACROPOLIS_WEST_ELEVATOR_HALL_BAY_CLUT_Y = 270 };

/// Uploads the full blended bay CLUT to VRAM, followed by the list terminator.
///
/// The word pointer is the GPU transfer view of the 16-bit colour array.
/// Queuing copies the rectangle but borrows the colour storage until completion.
static GpuImageUpload _gAcropolisWestElevatorHallBayClutUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, ACROPOLIS_WEST_ELEVATOR_HALL_BAY_CLUT_Y, ARRAY_SIZE(_gAcropolisWestElevatorHallBayBlendedClut), 1 }, (u_long*)_gAcropolisWestElevatorHallBayBlendedClut },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
};

WorldCollisionRoomResources gAcropolisWestElevatorHallCollisionRoomResources[1] = {
    { &_gAcropolisWestElevatorHallCollisionGrid, _gAcropolisWestElevatorHallViewBoundaryTriggers, _gAcropolisWestElevatorHallActionTriggers, NULL },
};

u8* gAcropolisWestElevatorHallViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gAcropolisWestElevatorHallViewCounts[1] = { ARRAY_SIZE(gAcropolisWestElevatorHallSpriteViews) };

WorldCoordRoomLighting gAcropolisWestElevatorHallRoomLightingTable[1] = {
    { &_gAcropolisWestElevatorHallRoomLights, NULL },
};

DirectionWarpEntry gAcropolisWestElevatorHallWarpEntries[2] = {
    { { { .word = 2048 }, -1234, -30, 1924 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -1234, -30, 1924 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_BAY_VIEW, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 5384, -30, 1039 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 5384, -30, 1039 }, { 0, 0, 0, 0 }, ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ARRIVAL_SOUND, ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_DEPARTURE_SOUND, DIRECTION_WARP_SOUND_NONE, ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_DOOR_VIEW, DIRECTION_WARP_FLAG_NONE, ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ENTRY_MAP_FLAG },
};

/// Grid-local plane normals, with 4096 representing unit length.
static SVECTOR _gAcropolisWestElevatorHallCollisionGridNormals[7] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_normals.inc"
};

/// Collision-grid vertices in signed game-coordinate units.
static SVECTOR _gAcropolisWestElevatorHallCollisionGridVerts[29] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_verts.inc"
};

/// Indexed grid faces using room surface classes 0 and 3.
static WorldCollisionGridFace _gAcropolisWestElevatorHallCollisionGridFaces[14] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_faces.inc"
};

/// Serialized signed face-index lists, terminated by WORLD_COLLISION_GRID_CELL_END.
///
/// The 52 halfwords retain the full asset extent, including words outside live lists.
static s16 _gAcropolisWestElevatorHallCollisionGridCells[52] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_cells.inc"
};

/// Selects a collision cell's face-index list for the generated directory.
///
/// Captures `_gAcropolisWestElevatorHallCollisionGridCells` (52 s16 elements).
/// `halfwordOffset` must be a constant s16-element offset to a list start
/// within that pool; the list must also end there at
/// `WORLD_COLLISION_GRID_CELL_END`. Evaluates the offset once and returns a
/// borrowed `s16*` without reading or changing the list.
/// The pool must stay loaded while the directory is used.
/// This grid's `cell_face_list_macro` manifest setting selects the binding;
/// it is defined only around the table include.
#define ACROPOLIS_WEST_ELEVATOR_HALL_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gAcropolisWestElevatorHallCollisionGridCells[(halfwordOffset)])

/// X-major cell directory: three X cells by two Z cells, pointing into the lists.
static s16* _gAcropolisWestElevatorHallCollisionGridTable[6] = {
#include "assets/acropolis_west_elevator_hall_collision_07D3C_table.inc"
};
#undef ACROPOLIS_WEST_ELEVATOR_HALL_CELL_FACE_LIST_AT_OFFSET

/// Room collision mesh covering a 3-by-2 XZ grid of 4000-unit cells.
///
/// Cell coordinates add 5480 to room X and 2000 to room Z before division.
/// Room setup binds the view coordinate; the record and pools remain writable
/// and borrowed while this overlay's grid is active.
static WorldCollisionGrid _gAcropolisWestElevatorHallCollisionGrid = {
    NULL,
    _gAcropolisWestElevatorHallCollisionGridNormals,
    _gAcropolisWestElevatorHallCollisionGridVerts,
    _gAcropolisWestElevatorHallCollisionGridFaces,
    _gAcropolisWestElevatorHallCollisionGridTable,
    5480,
    2000,
    3,
    2,
    4000,
    ARRAY_SIZE(_gAcropolisWestElevatorHallCollisionGridFaces),
};

/// Directed view boundaries for 2 to 3, 3 to 2, 4 to 3 and 3 to 4.
///
/// The fourth record carries LAST and remains active. Room setup binds the
/// view coordinate and links these writable nodes until room teardown.
static WorldCollisionTrigger _gAcropolisWestElevatorHallViewBoundaryTriggers[4] = {
    { NULL, NULL, NULL, { -994, -1504, 95, 0 }, { { -616, -1504, -2431, 0 }, { -616, 1504, -2431, 0 }, { 617, -1504, 2432, 0 }, { 617, 1504, 2432, 0 } }, { -3972, 0, 1006, 0 }, { 0, 0, 0, 0 }, 2918, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1281, -1536, 111, 0 }, { { -605, 1504, -2321, 0 }, { -605, -1504, -2321, 0 }, { 606, 1504, 2322, 0 }, { 606, -1504, 2322, 0 } }, { 3966, 0, -1036, 0 }, { 0, 0, 0, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2943, -1504, 511, 0 }, { { -2, 1504, -2612, 0 }, { -2, -1504, -2612, 0 }, { -3, 1504, 2603, 0 }, { -3, -1504, 2603, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 0, 0 }, 3007, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3135, -1440, 479, 0 }, { { 0, 1504, 2608, 0 }, { 0, -1504, 2608, 0 }, { 0, 1504, -2608, 0 }, { 0, -1504, -2608, 0 } }, { -4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 3007, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Packed warp slots for the square exit: departure 2 in the high nibble, arrival 1 in the low.
enum { ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_WARP_SLOTS = (2 << 4) | 1 };

/// Facing-gated interaction quads: the square exit and room CAP commands 3, 1, 2, 4.
///
/// The warp packs departure slot 2 and arrival slot 1. The fifth quad carries
/// LAST and remains active. Room setup binds and links the writable nodes;
/// their coordinate positions, corner offsets and radii use game units.
static WorldCollisionTrigger _gAcropolisWestElevatorHallActionTriggers[5] = {
    { NULL, NULL, NULL, { 5184, -48, 1152, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -ONE, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_ACROPOLIS_SQUARE, ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3936, -48, 1839, 0 }, { { 864, 0, -480, 0 }, { 864, 0, 481, 0 }, { -864, 0, -480, 0 }, { -864, 0, 481, 0 } }, { 0, 4097, 0, 0 }, { 201, 0, -4091, 0 }, 987, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -976, -76, 1840, 0 }, { { -1152, 0, -496, 0 }, { 1152, 0, -496, 0 }, { -1152, 0, 496, 0 }, { 1152, 0, 496, 0 } }, { 0, 4095, 0, 0 }, { 601, 0, -4052, 0 }, 1254, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2976, -71, 2688, 0 }, { { -1056, 0, -480, 0 }, { 1056, 0, -480, 0 }, { -1056, 0, 480, 0 }, { 1056, 0, 480, 0 } }, { 0, 4097, 0, 0 }, { -402, 0, -4077, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5248, -64, -80, 0 }, { { -416, 0, -1392, 0 }, { 416, 0, -1392, 0 }, { -416, 0, 1392, 0 }, { 416, 0, 1392, 0 } }, { 0, 4120, 0, 0 }, { ONE, 0, 0, 0 }, 1448, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Empty view-1 sprite list retaining decoded-background presentation.
///
/// The zero-count first range selects background strips; the second record ends the list.
static SpriteBatch _gAcropolisWestElevatorHallView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-2 foreground sprite rectangles, sampled texture regions and sorting depths.
///
/// The ranges cover all 26 sources. Positions are view-centred pixels;
/// depths are unscaled ordering values. The records remain writable while loaded.
static SpriteSource _gAcropolisWestElevatorHallView2SpriteSources[26] = {
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -160, 24, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -128, 24, 1200, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -104, 32, 1200, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 40, 64, 1800, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -104, 64, 1850, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -64, 40, 1868, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, 40, 1867, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -16, 40, 1866, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 8, 40, 1865, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -88, 40, 1867, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -104, 40, 1867, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, 16, 1912, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, -8, 1922, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 24, -40, 1956, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -72, -40, 1951, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -72, 16, 1919, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -72, -8, 1927, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -72, -32, 1956, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 32, 40, 1850, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 40, 24, 1831, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 40, 0, 1831, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 40, -24, 1831, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -104, 32, 1853, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -96, 16, 1858, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -96, 0, 1858, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -96, -24, 1858, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
};

/// View-2 background selection and source ranges [0, 3) and [3, 26), then END.
static SpriteBatch _gAcropolisWestElevatorHallView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, ARRAY_SIZE(_gAcropolisWestElevatorHallView2SpriteSources) - 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-3 foreground sprite rectangles, sampled texture regions and sorting depths.
///
/// The ranges cover all 63 sources. Positions are view-centred pixels;
/// depths are unscaled ordering values. The records remain writable while loaded.
static SpriteSource _gAcropolisWestElevatorHallView3SpriteSources[63] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 495, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 8, 1500, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 64, 637, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 80, 72, 637, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 104, 625, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 104, 625, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 72, 88, 625, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, 88, 625, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 80, 628, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 80, 628, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 104, 625, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 152, -120, 628, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 144, -80, 643, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 144, -40, 657, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, 136, -8, 688, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 136, 24, 698, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 136, 48, 711, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 136, 64, 683, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 152, 88, 640, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 144, 88, 612, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 136, 88, 625, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 128, 64, 775, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, 104, 637, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 120, 56, 850, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, 96, 850, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 112, 88, 882, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 112, 48, 882, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 104, 40, 956, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 104, 80, 956, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 96, 64, 1025, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 96, 32, 1045, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 88, 24, 1109, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 88, 56, 1093, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 80, 48, 1218, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 80, 16, 1284, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 80, -16, 1254, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 80, -48, 1228, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, 80, -96, 1205, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 48, -72, 1840, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 48, -48, 1916, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 48, -24, 1948, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, -8, 2375, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 8, 2130, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 56, -8, 1888, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, 16, 1877, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 64, -8, 1702, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 64, 24, 1588, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 72, 8, 1397, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 72, 32, 1379, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 24, 0, 2382, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 24, -24, 2301, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, 24, -48, 2356, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, 24, -72, 2263, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, -16, -64, 2385, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, -64, 2482, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, -40, 2527, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, -16, 2577, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -16, 0, 2504, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 0, -16, 2481, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 0, -24, 2451, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 8, -40, 2403, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 0, -40, 2437, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -16, -40, 2452, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
};

/// View-3 background selection and ranges [0, 11), [11, 49), [49, 63), then END.
static SpriteBatch _gAcropolisWestElevatorHallView3SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 38, 0, 0, { 2, 0 } },
    { 49, ARRAY_SIZE(_gAcropolisWestElevatorHallView3SpriteSources) - 49, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-4 foreground sprite rectangles, sampled texture regions and sorting depths.
///
/// The ranges cover all 60 sources. Positions are view-centred pixels;
/// depths are unscaled ordering values. The records remain writable while loaded.
static SpriteSource _gAcropolisWestElevatorHallView4SpriteSources[60] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -112, 678, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 120, -104, 841, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -88, 1107, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -56, 1129, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -40, 1213, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -16, 1222, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 8, 1202, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, -56, 745, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 725, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, 0, 744, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 128, 24, 761, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 128, 40, 762, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 56, 782, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 32, 1292, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 32, 1266, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 32, 1041, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 112, 40, 932, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 56, 842, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, 56, 1168, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 72, 1017, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 88, 901, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 96, 853, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 72, 788, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 72, 722, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 88, 677, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 104, 629, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -16, -40, 1422, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -40, 1379, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, -40, 1343, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 1413, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -8, 1444, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 8, 1465, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, 24, 1486, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 40, 1502, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 40, 1435, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 32, 1550, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, 32, 1560, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 16, 1598, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -24, 1561, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -64, -80, 1449, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -80, 1471, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, -24, 1531, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 16, 1541, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 1583, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 1547, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -80, 1436, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -16, -80, 1332, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 40, 1388, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 24, 1379, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 0, 1391, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 1300, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 48, -80, 1250, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -88, 1085, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -40, 1198, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 0, 1247, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 40, 1285, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 40, 1427, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 24, 1394, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 64, 0, 1269, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -40, 1289, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

/// View-4 background selection and source ranges [0, 26) and [26, 60), then END.
static SpriteBatch _gAcropolisWestElevatorHallView4SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 1, 0 } },
    { 26, ARRAY_SIZE(_gAcropolisWestElevatorHallView4SpriteSources) - 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View-5 foreground sprite rectangles, sampled texture regions and sorting depths.
///
/// One range selects all 26 elements. Positions are view-centred pixels;
/// depths are unscaled ordering values. The records remain writable while loaded.
static SpriteSource _gAcropolisWestElevatorHallView5SpriteSources[26] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 40, 1003, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 48, 955, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 943, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 873, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 873, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 874, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, -160, -120, 571, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 128, 48 } }, -64, -120, 497, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, 64, -120, 617, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 24 } }, -160, -72, 700, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 16 } }, -160, -48, 700, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 16 } }, -160, -32, 700, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, -160, -16, 700, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 0, 750, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 16, 775, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 24 } }, -160, 32, 775, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 56, 775, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 24 } }, 64, -72, 700, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, -48, 700, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, -32, 700, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 16 } }, 56, -16, 700, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, 48, 0, 750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, 48, 16, 775, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, 48, 32, 775, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, 48, 48, 775, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 64, 775, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
};

/// View-5 background selection and one full-source range, then END.
static SpriteBatch _gAcropolisWestElevatorHallView5SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisWestElevatorHallView5SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gAcropolisWestElevatorHallSpriteViews[5] = {
    { { .empty = _gAcropolisWestElevatorHallView1SpriteBatches }, _gAcropolisWestElevatorHallView1SpriteBatches, NULL },
    { { .elements = _gAcropolisWestElevatorHallView2SpriteSources }, _gAcropolisWestElevatorHallView2SpriteBatches, NULL },
    { { .elements = _gAcropolisWestElevatorHallView3SpriteSources }, _gAcropolisWestElevatorHallView3SpriteBatches, NULL },
    { { .elements = _gAcropolisWestElevatorHallView4SpriteSources }, _gAcropolisWestElevatorHallView4SpriteBatches, NULL },
    { { .elements = _gAcropolisWestElevatorHallView5SpriteSources }, _gAcropolisWestElevatorHallView5SpriteBatches, NULL },
};

/// Authored point lights for model shading in every west elevator hall view.
///
/// Positions and inner/outer falloff radii use integer world units; RGB intensities
/// have 12 fractional bits (`ONE` is full intensity). The room overlay owns this
/// writable array: coordinate updates set its parent and cached transforms, and
/// lighting queries overwrite attenuation. Borrowed pointers expire on room unload.
static WorldCoordPointLight _gAcropolisWestElevatorHallPointLights[] = {
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -4540, -1500, -1830 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 4840, -1500, 2900 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5410, -1660, 2170 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5410, -1660, -90 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5020, -1760, 1040 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3031, 3112, 3276 },
        },
        .inner = 500,
        .outer = 2500,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3800, -1880, 2030 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 2048, 409, 409 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 0, -1960, 0 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 901, 983, 1146 },
        },
        .inner = 1000,
        .outer = 7000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1000, -1340, 3000 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3686, 3358, 2867 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -3800, -370, 2030 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 2703, 2785, 2375 },
        },
        .inner = 500,
        .outer = 1000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -6310, -1650, 100 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1700, -1650, -2800 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 1690, -1650, -2420 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 5550, -1650, -2190 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 1884, 2048, 2129 },
        },
        .inner = 500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { -1000, -1640, 1700 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3686, 3358, 2867 },
        },
        .inner = 500,
        .outer = 2000,
    },
    {
        .head = {
            .transform = {
                .lighting = {
                    .composeStamp = GRAPHICS_COORD_DIRTY,
                    .local        = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, .t = { 3000, -1440, 2500 } },
                    .composed     = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                    .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                    .attenuation  = 0,
                    .parent       = NULL,
                },
            },
            .color = { 3276, 2949, 2457 },
        },
        .inner = 500,
        .outer = 1500,
    },
};

/// The hall's point-light collection, shared by all five views.
///
/// No directional or cone lights are supplied. Coordinate and shading updates
/// borrow the writable point-light array while the room remains loaded.
static WorldCoordRoomLights _gAcropolisWestElevatorHallRoomLights = {
    0,
    NULL,
    ARRAY_SIZE(_gAcropolisWestElevatorHallPointLights),
    _gAcropolisWestElevatorHallPointLights,
    0,
    NULL,
};

ViewCamera gAcropolisWestElevatorHallViewCameras[5] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -100, 0x7530, -500 } }, 617 },
    { { { { -27, 0, 4095 }, { -657, 4042, -4 }, { -4042, -657, -27 } }, { -1918, 620, -769 } }, 257 },
    { { { { 792, 0, -4018 }, { -689, 4035, -135 }, { 3959, 702, 780 } }, { 3941, 2050, 660 } }, 230 },
    { { { { 1311, 0, -3880 }, { -766, 4015, -259 }, { 3803, 809, 1285 } }, { -338, 2340, 430 } }, 230 },
    { { { { 4095, 0, 0 }, { 0, 3401, -2281 }, { 0, 2281, 3401 } }, { 1000, 2820, 268 } }, 257 },
};

/// Base sound-script entries for the hall's paired walking and running cues.
enum {
    ACROPOLIS_WEST_ELEVATOR_HALL_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 17,
    ACROPOLIS_WEST_ELEVATOR_HALL_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 19,
};

/// Footstep cue bases for every hall surface; scripted jumps reuse the walking pair.
///
/// Cue 1 uses base + 1; cue 2 uses the base. Companion playback adds 100.
/// Surface records borrow this room-owned data while the overlay is loaded.
static WorldCollisionFootstepSounds _gAcropolisWestElevatorHallFootstepSounds = {
    ACROPOLIS_WEST_ELEVATOR_HALL_FOOTSTEP_WALK,
    ACROPOLIS_WEST_ELEVATOR_HALL_FOOTSTEP_RUN,
    ACROPOLIS_WEST_ELEVATOR_HALL_FOOTSTEP_WALK,
};

/// Shared policy for all hall surfaces: block probes, allow impacts and apply pushback.
///
/// Every surface class uses the same footstep cues. The first byte's role is unproven.
static WorldCollisionSurfaceProperties _gAcropolisWestElevatorHallSurfaceProperties = {
    0,
    WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    &_gAcropolisWestElevatorHallFootstepSounds,
};

WorldCollisionSurfaceProperties* gAcropolisWestElevatorHallSurfacePropertiesByClass[8] = {
    &_gAcropolisWestElevatorHallSurfaceProperties,
    &_gAcropolisWestElevatorHallSurfaceProperties,
    &_gAcropolisWestElevatorHallSurfaceProperties,
    &_gAcropolisWestElevatorHallSurfaceProperties,
    &_gAcropolisWestElevatorHallSurfaceProperties,
    &_gAcropolisWestElevatorHallSurfaceProperties,
    &_gAcropolisWestElevatorHallSurfaceProperties,
    &_gAcropolisWestElevatorHallSurfaceProperties,
};

/// Borrowed live door-leaf task handles, negative X first and positive X second.
///
/// Room initialization stores both spawn results. Door messages require both
/// non-NULL and live; teardown does not clear them. Reset on each overlay load.
static Task* _gAcropolisWestElevatorHallDoorLeafTasks[2] = { NULL };

#include "../../shared/room/planar_reflection.inc.c"

void acropolisWestElevatorHallPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Starts the warp-1 elevator arrival event once per overlay load.
///
/// Runs in room-task state 1. Starts the arrival and skip scripts with HUD
/// hiding/restoration and initializes the chapter, dialogue and objective flags.
/// The overlay latch advances from waiting (0) to running (1), then finished (2)
/// when the session event state is idle. The task argument is unused; the session,
/// live game flags and this overlay's script resources must remain valid.
static void _acropolisWestElevatorHallUpdateArrivalEvent(Task* unusedTask)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WARP       = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_SESSION_EVENT_IDLE = 0,
        ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_DIALOGUE   = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_STORY_CHAPTER      = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_OBJECTIVE  = 1,
    };
    // Unused initialized words and reservation reproduce the original 0x230-byte frame.
    s32 retainedWords[2] = { 0, 4 };
    u8  retainedStackSpace[0x210];
    u8  arrivalWarp;

    if (_gAcropolisWestElevatorHallArrivalPhase == ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WAITING) {
        arrivalWarp = gGameSession->location.warp;
        if (arrivalWarp == ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WARP) {
            _gAcropolisWestElevatorHallArrivalPhase = arrivalWarp;
            evsStartScriptWithSkip(_gAcropolisWestElevatorHallArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisWestElevatorHallArrivalSkipScript);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_DIALOGUE);
            gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, ACROPOLIS_WEST_ELEVATOR_HALL_STORY_CHAPTER);
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_OBJECTIVE);
        }
    }
    if (_gAcropolisWestElevatorHallArrivalPhase == ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_RUNNING && gGameSession->eventState == ACROPOLIS_WEST_ELEVATOR_HALL_SESSION_EVENT_IDLE) {
        _gAcropolisWestElevatorHallArrivalPhase = ACROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_FINISHED;
    }
}

/// Dispatches one mirrored leaf of the elevator's sliding door.
///
/// Requires a live TMD body and state 0 initialize, 1 slide/draw, or 2 kill;
/// the state index is unchecked. `spawnArg1.value` requests rest (0), opening
/// (1) or closing (-1); `spawnArg2.value` selects its X side (-1 or 1).
/// Initialization owns a travel block in `Task::work`, released by `taskKill`.
/// Leaf models and callbacks must remain loaded through task teardown.
static void _acropolisWestElevatorHallDoorLeafTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAcropolisWestElevatorHallDoorLeafStates;
    stateHandlers.funcs[task->state](task);
}

/// Requests both elevator-door leaves to open on subsequent updates and returns 0.
///
/// Message 5100 ignores both payload words and the receiver. Both leaf task slots
/// must hold live tasks; this only sets their signed travel request and does not
/// wait for the opening to finish.
static s32 _acropolisWestElevatorHallOpenDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _gAcropolisWestElevatorHallDoorLeafTasks[ACROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX]->spawnArg1.value = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_OPEN;
    _gAcropolisWestElevatorHallDoorLeafTasks[ACROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX]->spawnArg1.value = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_OPEN;
    return 0;
}

/// Requests both elevator-door leaves to close on subsequent updates and returns 0.
///
/// Message 5101 ignores both payload words and the receiver. Both leaf task slots
/// must hold live tasks; this only sets their signed travel request and does not
/// wait for the closing to finish.
static s32 _acropolisWestElevatorHallCloseDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _gAcropolisWestElevatorHallDoorLeafTasks[ACROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX]->spawnArg1.value = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE;
    _gAcropolisWestElevatorHallDoorLeafTasks[ACROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX]->spawnArg1.value = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE;
    return 0;
}

/// Resolves a room transition, arming the first departure into Acropolis Square.
///
/// Message `ROOM_EVENT_MESSAGE_RESOLVE` borrows a complete eight-byte request
/// and writable reply, which may alias. Copies the request before resolving it;
/// query mode commits no story or scene-event changes. The first executed square
/// transition sets opening progress to 1, arms scene event 1 and selects arrival
/// warp 7. Requires live flags and save state; always returns 1 (transition allowed).
static s32 _acropolisWestElevatorHallResolveRoomEventMessage(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_OPENING_NOT_STARTED   = 0,
        ACROPOLIS_WEST_ELEVATOR_HALL_OPENING_DEPARTED      = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_DEPARTURE_SCENE_EVENT = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ARRIVAL_WARP   = 7,
        ACROPOLIS_WEST_ELEVATOR_HALL_TRANSITION_ALLOWED    = 1,
    };

    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_SQUARE && gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) == ACROPOLIS_WEST_ELEVATOR_HALL_OPENING_NOT_STARTED && request->queryOnly == ROOM_EVENT_EXECUTE) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS, ACROPOLIS_WEST_ELEVATOR_HALL_OPENING_DEPARTED);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_WEST_ELEVATOR_HALL_DEPARTURE_SCENE_EVENT;
        reply->warp                                         = ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ARRIVAL_WARP;
    }
    return ACROPOLIS_WEST_ELEVATOR_HALL_TRANSITION_ALLOWED;
}

/// Refuses every key-item use request in this room without consuming the item.
///
/// `itemId` is the collected inventory item's ID; `unusedArg` is the unused
/// second message word. All arguments are ignored. The zero result tells the
/// inventory menu to display its item-cannot-be-used response.
static s32 _acropolisWestElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ACROPOLIS_WEST_ELEVATOR_HALL_KEY_ITEM_REFUSED = 0 };

    return ACROPOLIS_WEST_ELEVATOR_HALL_KEY_ITEM_REFUSED;
}

/// Registers the hall's room task and spawns its two stationary elevator-door leaves.
///
/// Called in state 0 with both door models loaded. Descriptor 0 retracts along
/// negative X and descriptor 1 along positive X, each with its own travel work.
/// Saves both spawn results and advances to state 1 even on failure. Door
/// messages subsequently require both saved handles to be live.
static void _acropolisWestElevatorHallInitRoomTask(Task* task)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_REST          = 0,
        ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_NEGATIVE = -1,
        ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_POSITIVE = 1,
    };
    task->msgTable = _gAcropolisWestElevatorHallRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gAcropolisWestElevatorHallDoorLeafTasks[ACROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX] =
        taskSpawnFromTable(_gAcropolisWestElevatorHallDoorLeafTaskTable, ACROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_REST, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_NEGATIVE);
    _gAcropolisWestElevatorHallDoorLeafTasks[ACROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX] =
        taskSpawnFromTable(_gAcropolisWestElevatorHallDoorLeafTaskTable, ACROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_REST, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_POSITIVE);
    task->state = task->state + 1;
}

void acropolisWestElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAcropolisWestElevatorHallRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Places a drawable leaf at the closed doorway in the current view hierarchy.
///
/// Borrows a live leaf model and its root coordinate, clearing all model draw
/// flags and parenting the root to the current view. Sets the room-coordinate
/// translation to (-1000, -20, 2420) in whole units and invalidates composition.
/// Rotation and scale remain intact; no composition or draw is performed here.
static inline void _acropolisWestElevatorHallPlaceClosedDoorLeaf(TmdObject* doorModel, GfxCoord* doorCoord)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Y = -20,
        ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Z = 2420,
    };

    doorModel->flags        = 0;
    doorCoord->parent       = &gGfxViewCoord;
    doorCoord->coord.t[0]   = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X;
    doorCoord->coord.t[1]   = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Y;
    doorCoord->coord.t[2]   = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Z;
    doorCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Initializes a door leaf on the shut line and advances to its sliding state.
///
/// Called in state 0 with a live TMD body and root coordinate. Owns a four-byte
/// primary-heap travel block in `Task::work` until `taskKill`; allocation failure
/// kills the task immediately. World position is (-1000, -20, 2420), parented
/// to the current view coordinate; the two leaves initially meet at this X.
static void _acropolisWestElevatorHallInitializeDoorLeaf(Task* task)
{
    TmdObject*                              doorModel;
    GfxCoord*                               doorCoord;
    _AcropolisWestElevatorHallDoorLeafWork* leafWork;

    doorModel = task->extra.tmd;
    doorCoord = doorModel->coords;
    leafWork  = memCalloc(sizeof(*leafWork), 0);
    if (leafWork == NULL) {
        taskKill(task);
        return;
    }
    task->work       = leafWork;
    leafWork->travel = 0;
    _acropolisWestElevatorHallPlaceClosedDoorLeaf(doorModel, doorCoord);
    task->state++;
}

/// Integrates and clamps one door leaf's travel, then places it on its X side.
///
/// Borrows initialized task/work/coordinate storage. The task's first spawn
/// word selects motion (-1/0/1), and its second selects the side (-1 or 1).
/// Distances are whole world units. Composition remains the caller's.
static inline void _acropolisWestElevatorHallSlideDoorLeaf(const Task* task, _AcropolisWestElevatorHallDoorLeafWork* leafWork, GfxCoord* doorCoord)
{
    leafWork->travel += task->spawnArg1.value * ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_SPEED;
    if (leafWork->travel < 0) {
        leafWork->travel = 0;
    }
    if (leafWork->travel > ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX) {
        leafWork->travel = ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX;
    }
    doorCoord->coord.t[0] = (leafWork->travel * task->spawnArg2.value) + ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X;
}

/// Slides one mirrored door leaf and updates its visibility and lighting.
///
/// State 1 borrows initialized travel work and a live model. `spawnArg1.value`
/// is -1 closing, 0 stopped or 1 opening at 20 world units per frame;
/// `spawnArg2.value` is the leaf's X side (-1 or 1). Travel stays in 0..720.
/// Only view 5 draws the model. Composition precedes the full lighting query.
static void _acropolisWestElevatorHallUpdateDoorLeaf(Task* task)
{
    enum { ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_VIEW = 5 };
    VECTOR                                  worldPosition;
    TmdObject*                              doorModel;
    GfxCoord*                               doorCoord;
    _AcropolisWestElevatorHallDoorLeafWork* leafWork;

    leafWork  = task->work;
    doorModel = task->extra.tmd;
    doorCoord = doorModel->coords;

    _acropolisWestElevatorHallSlideDoorLeaf(task, leafWork, doorCoord);
    if (gGameSession->location.view == ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_VIEW) {
        doorModel->flags = 0;
    } else {
        doorModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Sample lighting from the newly composed world position, even when hidden.
    doorCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(doorCoord);
    worldPosition.vx = doorCoord->workm.t[0];
    worldPosition.vy = doorCoord->workm.t[1];
    worldPosition.vz = doorCoord->workm.t[2];
    worldCoordSetModelLighting(doorModel, &worldPosition, 0, 3);
}

void acropolisWestElevatorHallViewEffectsTask(Task* task)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_INITIALIZE    = 0,
        ACROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_UPDATE        = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_VIEW           = 2,
        ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_VIEW             = 5,
        ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_BANK       = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_INDEX = 0x25,
        ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_RATE     = 4,
        ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_SIZE     = 24,
        ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_RATE     = 3,
        ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_SIZE     = 8,
    };
    SVECTOR   beaconPosition;
    SVECTOR   lightPosition;
    GfxCoord* parentCoord;

    /// Emits all three view-2 beacons, snapshotting each position before reuse.
    ///
    /// Captures the writable SVECTOR `beaconPosition` and live GfxCoord pointer
    /// `parentCoord` and this function's rate/size constants. Positions count
    /// parent-coordinate units; spawn copies XYZ and borrows the parent.
    /// Expands to one statement block and leaves beaconPosition at the last beacon.
#define ACROPOLIS_WEST_ELEVATOR_HALL_SPAWN_VIEW_BEACONS()                                                                                                                                                          \
    {                                                                                                                                                                                                              \
        beaconPosition = _gAcropolisWestElevatorHallFirstBeaconPosition;                                                                                                                                           \
        effectSpawn(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_RATE, ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &beaconPosition); \
        beaconPosition.vx = -0x1800;                                                                                                                                                                               \
        beaconPosition.vy = -0x4F0;                                                                                                                                                                                \
        beaconPosition.vz = -0x600;                                                                                                                                                                                \
        effectSpawn(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_RATE, ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &beaconPosition); \
        beaconPosition.vx = -0x1800;                                                                                                                                                                               \
        beaconPosition.vy = -0x4F0;                                                                                                                                                                                \
        beaconPosition.vz = -0x2C0;                                                                                                                                                                                \
        effectSpawn(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_RATE, ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &beaconPosition); \
    }

    parentCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case ACROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_INITIALIZE:
            task->msgTable = _gAcropolisWestElevatorHallViewEffectMessages;
            gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
            taskSpawn(ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_BANK, ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_INDEX, 0, 0);
            taskSpawn(ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_BANK, ACROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_INDEX, 1, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_UPDATE:
            if (gGameSession->location.view == ACROPOLIS_WEST_ELEVATOR_HALL_BEACON_VIEW) {
                ACROPOLIS_WEST_ELEVATOR_HALL_SPAWN_VIEW_BEACONS();
            }
            if (gGameSession->location.view == ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_VIEW) {
                lightPosition = _gAcropolisWestElevatorHallBayLightPosition;
                effectSpawn(EFFECT_ACROPOLIS_WEST_ELEVATOR_HALL_LIGHT, parentCoord, 0, &lightPosition);
            }
            return;
    }
#undef ACROPOLIS_WEST_ELEVATOR_HALL_SPAWN_VIEW_BEACONS
}

void acropolisWestElevatorHallBayLightingTask(Task* task)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_BAY_LIGHTING_VIEW = 5,
        ACROPOLIS_WEST_ELEVATOR_HALL_LIGHT_RAMPING     = 0,
        ACROPOLIS_WEST_ELEVATOR_HALL_LIGHT_HOLDING     = 1
    };
    EffectWork* work;
    s32         colorIndex;
    s32         paletteChanged;

    /// Blends and uploads the full bay palette, borrowing the result until GPU completion.
    ///
    /// Captures this room's source palettes, destination and upload table.
    /// `litWeightQ12` is a side-effect-free Q12 expression in 0..ONE, read once
    /// for each sixteen-colour row. `colorIndex` is a writable s32 loop counter
    /// whose address and value have no side effects; it exits at ARRAY_SIZE.
    /// Expands to a loop and an upload: invoke only inside an explicit compound
    /// statement. Neither argument may alter control flow.
#define ACROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE(litWeightQ12, colorIndex)                                                                   \
    for ((colorIndex) = 0; (colorIndex) < (s32)ARRAY_SIZE(_gAcropolisWestElevatorHallBayBlendedClut); (colorIndex) += GPU_RGB555_CLUT_ROW_COLORS) { \
        gpuBlendRgb555ClutRow(&_gAcropolisWestElevatorHallBayLitClut[(colorIndex)],                                                                 \
                              &_gAcropolisWestElevatorHallBayUnlitClut[(colorIndex)], (litWeightQ12),                                               \
                              &_gAcropolisWestElevatorHallBayBlendedClut[(colorIndex)]);                                                            \
    }                                                                                                                                               \
    gpuUploadImages(_gAcropolisWestElevatorHallBayClutUploads)

    work           = task->spawnArg2.pointer;
    paletteChanged = 0;
    // The signed halfwords hold a Q12 lit weight and a ramp-complete latch here.
    if (work->angle == ACROPOLIS_WEST_ELEVATOR_HALL_LIGHT_RAMPING) {
        work->scale = work->scale + ONE / 2;
        if (work->scale == ONE) {
            work->angle = ACROPOLIS_WEST_ELEVATOR_HALL_LIGHT_HOLDING;
        }
        paletteChanged = 1;
    }

    if (paletteChanged != 0) {
        ACROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE(work->scale, colorIndex);
    }

    // Restore the palette before releasing the effect, even during its ramp.
    if (gGameSession->location.view != ACROPOLIS_WEST_ELEVATOR_HALL_BAY_LIGHTING_VIEW) {
        ACROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE(0, colorIndex);
        effectKillTask(work, task);
    }
#undef ACROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE
}

#include "../../shared/effects/red_beacon_task.inc.c"
#undef RED_BEACON_TASK

/// Queues one displaced framebuffer row at the distortion strip's fixed X origin.
///
/// `sourceRect` must be word-aligned with height 1 and positive width; all
/// rectangle coordinates, extents and `destinationY` count VRAM pixels.
/// Both regions must fit in VRAM. The rectangle is copied into the packet
/// during this call and may then expire; its mutable type follows the SDK API.
/// Requires word-aligned arena space for one `DR_MOVE` and a current ordering
/// table containing tag index `otIndex` (the caller uses 0x72, not a byte offset).
/// Queued packet storage must remain live until GPU completion.
static inline void _acropolisWestElevatorHallQueueScanlineCopy(RECT* sourceRect, s32 destinationY, s32 otIndex)
{
    enum { ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DESTINATION_X = 80 };
    DR_MOVE* movePacket;

    movePacket     = gGpuPrimCursor;
    gGpuPrimCursor = movePacket + 1;
    SetDrawMove(movePacket, sourceRect, ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DESTINATION_X, destinationY);
    addPrim(&gGpuCurrentOt[otIndex], movePacket);
}

void acropolisWestElevatorHallScanlineDistortionTask(Task* task)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_BUFFER_STRIDE = 272,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN        = 80,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ROWS          = 82,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_WIDTH         = 120,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_OT_INDEX      = 0x72,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_RANDOM_PHASE  = 0x80,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MASK  = 0x3F,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MIN   = 192,
        ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR       = 256
    };
    RECT        sourceRect;
    EffectWork* effectWork;
    s32         otIndex;
    s32         rowIndex;
    s32         stripTopY;
    s32         sourceX;
    s32         sourceY;
    s32         displacementQ12;

    effectWork = task->spawnArg2.pointer;
    stripTopY  = gDisplayState.drawBuffer * ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_BUFFER_STRIDE + ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN;

    // Bit 7 selects 128-frame intervals with a separate random divisor per row.
    for (rowIndex = 0; rowIndex < ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ROWS; rowIndex++) {
        otIndex         = ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_OT_INDEX;
        sourceY         = rowIndex;
        sourceY        += stripTopY;
        displacementQ12 = ONE / 2 - rcos((gDisplayState.animFrame + rowIndex * 2) * 16);
        if (gDisplayState.animFrame & ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_RANDOM_PHASE) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            sourceX         = displacementQ12 / (s32)(((gRandomLcgState >> 16) & ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MASK) + ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MIN) + ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN;
        } else {
            sourceX = displacementQ12 / ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR + ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN;
        }

        sourceRect.x = sourceX;
        sourceRect.y = sourceY;
        sourceRect.w = ACROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_WIDTH;
        sourceRect.h = 1;
        _acropolisWestElevatorHallQueueScanlineCopy(&sourceRect, rowIndex + stripTopY, otIndex);
    }

    effectKillTask(effectWork, task);
}

/// Sets the hall light's square billboard coordinates around its projected centre.
///
/// Borrows a writable `glowQuad` and a read-only `glowScratch` for this call.
/// Only `screenPos` and `halfExtent` need initialization in the scratch record;
/// both count pixels, with a nonnegative half-extent (the caller supplies
/// 1..1551). Centre plus/minus half-extent must fit signed 32-bit arithmetic;
/// each result narrows to a signed 16-bit packet coordinate. Vertex indices
/// 0..3 are top-left, top-right, bottom-left and bottom-right, respectively.
/// Writes only the eight coordinate halfwords, preserving other packet fields.
static inline void _acropolisWestElevatorHallSetLightGlowBounds(POLY_FT4* glowQuad, const RoomGlowSpriteScratch* glowScratch)
{
    glowQuad->x0 = glowQuad->x2 = glowScratch->screenPos.vx - glowScratch->halfExtent;
    glowQuad->x1 = glowQuad->x3 = glowScratch->screenPos.vx + glowScratch->halfExtent;
    glowQuad->y0 = glowQuad->y1 = glowScratch->screenPos.vy - glowScratch->halfExtent;
    glowQuad->y2 = glowQuad->y3 = glowScratch->screenPos.vy + glowScratch->halfExtent;
}

/// Projects the hall light's composed view-space origin into an additive textured billboard.
///
/// Borrows the composed coordinate and a word-aligned writable scratch block;
/// the caller supplies GTE projection settings and `GsWSMATRIX`. The origin
/// narrows to signed 16-bit game units before projection. Depth is SZ3 / 4
/// (0..16383); values below 17 reject the glow, without testing GTE flags.
/// Accepted points use a pixel half-extent of 0x6700 / depth and wrap their
/// scaled sorting depth into the current 1024-tag ordering table.
/// Reserves one word-aligned `POLY_FT4` even on rejection. Scratch storage may
/// expire on return; queued packet and texture storage live until GPU completion.
static inline void _acropolisWestElevatorHallDrawLightGlow(const GfxCoord* effectCoord, RoomGlowSpriteScratch* glowScratch)
{
    enum {
        ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_MIN_DEPTH     = 17,
        ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_TEXTURE_8_BIT = 1,
        ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_TPAGE         = getTPage(ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_TEXTURE_8_BIT, GPU_BLEND_ADD, 704, 0),
        ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_CLUT          = getClut(0, 270),
        ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX        = 103,   // Inclusive last texel of the 104-by-104 image
        ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_SIZE_FACTOR   = 0x6700 // Pixel half-extent numerator in SZ3 / 4 depth units
    };
    POLY_FT4* glowQuad;

    // The effect's parent chain has already transformed this centre into view space.
    glowScratch->worldPos.vx = effectCoord->workm.t[0];
    glowScratch->worldPos.vy = effectCoord->workm.t[1];
    glowScratch->worldPos.vz = effectCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&glowScratch->worldPos);
    gte_rtps();
    // Packet reservation precedes depth rejection, including for invisible points.
    glowQuad       = gGpuPrimCursor;
    gGpuPrimCursor = glowQuad + 1;
    setPolyFT4(glowQuad);
    gte_stsxy(&glowScratch->screenPos);
    gte_stszotz(&glowScratch->otz);
    if (glowScratch->otz >= ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_MIN_DEPTH) {
        glowQuad->tpage = ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_TPAGE;
        glowQuad->clut  = ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_CLUT;
        glowQuad->u0    = 0;
        glowQuad->v0    = 0;
        glowQuad->u1    = ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        glowQuad->v1    = 0;
        glowQuad->u2    = 0;
        glowQuad->v2    = ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        glowQuad->u3    = ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        glowQuad->v3    = ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        setSemiTrans(glowQuad, 1);
        // Raw texture mode bypasses RGB modulation, so the colour bytes are unused.
        setShadeTex(glowQuad, 1);
        glowScratch->halfExtent = ACROPOLIS_WEST_ELEVATOR_HALL_GLOW_SIZE_FACTOR / glowScratch->otz;
        _acropolisWestElevatorHallSetLightGlowBounds(glowQuad, glowScratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)glowScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                glowQuad);
    }
}

void acropolisWestElevatorHallLightGlowTask(Task* task)
{
    RoomGlowSpriteScratch* glowScratch;
    GfxCoord*              effectCoord;
    EffectWork*            effectWork;

    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    glowScratch = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    _acropolisWestElevatorHallDrawLightGlow(effectCoord, glowScratch);
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(effectWork, task);
}

/// Starts the bay's palette-lighting effect and returns 0.
///
/// Room-effect message 3100 ignores the receiver and both payload words. Spawns
/// an unparented counted effect with a zero payload; the effect ramps over two
/// updates, holds in view 5 and restores the palette on leaving that view.
/// Requires this room's effect resources to stay loaded until effect teardown.
static s32 _acropolisWestElevatorHallStartBayLightingMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    effectSpawn(EFFECT_ACROPOLIS_WEST_ELEVATOR_BAY_LIGHTS, NULL, 0, NULL);
    return 0;
}
