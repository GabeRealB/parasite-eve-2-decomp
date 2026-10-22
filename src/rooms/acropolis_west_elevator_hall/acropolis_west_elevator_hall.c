#include "rooms/acropolis_west_elevator_hall.h"
#include "../../stages/akropolis/event_flags.h"

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

#include "assets/rooms/akropolis/west_elevator_hall/declarations.inc"

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
#include "assets/rooms/akropolis/west_elevator_hall/markers.inc"

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
};

static AnimationSet _gAkropolisWestElevatorHallAcropolisWestElevatorHallArrivalAnim00Clip;
static TmdSource    _gAkropolisWestElevatorHallAcropolisWestElevatorHallModel02DE8;
static TmdSource    _gAkropolisWestElevatorHallAcropolisWestElevatorHallModel03058;
static void         _acropolisWestElevatorHallDoorLeafTask(Task* task);

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

#include "assets/rooms/akropolis/west_elevator_hall/acropolis_west_elevator_hall_model_02DE8.inc"

#include "assets/rooms/akropolis/west_elevator_hall/acropolis_west_elevator_hall_model_03058.inc"

#include "assets/rooms/akropolis/west_elevator_hall/clips.inc"

/// Model-backed door-leaf descriptors, negative X then positive X, followed by END.
///
/// Both run at priority 192. Spawning reads the descriptor synchronously;
/// geometry and callbacks remain borrowed until each leaf task ends.
static TaskDesc _gAcropolisWestElevatorHallDoorLeafTaskTable[3] = {
    { { { TASK_BODY_TMD, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY } }, _acropolisWestElevatorHallDoorLeafTask, { .model = &_gAkropolisWestElevatorHallAcropolisWestElevatorHallModel02DE8 } },
    { { { TASK_BODY_TMD, ACROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY } }, _acropolisWestElevatorHallDoorLeafTask, { .model = &_gAkropolisWestElevatorHallAcropolisWestElevatorHallModel03058 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// The single borrowed clip installed in the player's equipped-bank extension.
///
/// Copied into set slot `ANIMATION_BANK_BASE_SET_COUNT`; the set and its pools
/// remain loaded while the arrival animation plays.
#include "assets/rooms/akropolis/west_elevator_hall/acropolis_west_elevator_hall_arrival_bank.inc"

#include "assets/rooms/akropolis/west_elevator_hall/events.inc"
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

/// Packed warp slots for the square exit: departure 2 in the high nibble, arrival 1 in the low.
enum { ACROPOLIS_WEST_ELEVATOR_HALL_SQUARE_WARP_SLOTS = (2 << 4) | 1 };

/// Base sound-script entries for the hall's paired walking and running cues.
enum {
    ACROPOLIS_WEST_ELEVATOR_HALL_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 17,
    ACROPOLIS_WEST_ELEVATOR_HALL_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 19,
};

#include "assets/rooms/akropolis/west_elevator_hall/header.inc"
#include "assets/rooms/akropolis/west_elevator_hall/geometry.inc"
#include "assets/rooms/akropolis/west_elevator_hall/view_boundaries.inc"
#include "assets/rooms/akropolis/west_elevator_hall/action_triggers.inc"
#include "assets/rooms/akropolis/west_elevator_hall/sprites.inc"
#include "assets/rooms/akropolis/west_elevator_hall/lights.inc"
#include "assets/rooms/akropolis/west_elevator_hall/cameras.inc"
#include "assets/rooms/akropolis/west_elevator_hall/surfaces.inc"

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
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_ARRIVAL);
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
    if (request->areaId == GAME_AREA_AKROPOLIS_SQUARE && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) == ACROPOLIS_WEST_ELEVATOR_HALL_OPENING_NOT_STARTED && request->queryOnly == ROOM_EVENT_EXECUTE) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, ACROPOLIS_WEST_ELEVATOR_HALL_OPENING_DEPARTED);
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
