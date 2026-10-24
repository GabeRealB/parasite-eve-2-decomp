#include "rooms/akropolis_west_elevator_hall.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room_common.h"

#include "assets/rooms/akropolis/west_elevator_hall/declarations.inc"

/// Room-coordinate X where both elevator-door leaves meet when shut, in whole units.
#define AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X (-1000)

/// Whole room-coordinate units added to a door leaf's travel per motion update.
#define AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_SPEED 20

/// Inclusive upper clamp of a retracted door leaf's travel, in room-coordinate units.
#define AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX 720

/// Work block of one leaf of the elevator's sliding door, held in `Task::work`.
typedef struct {
    s32 travel; // World units slid from the shut position, 0 to `AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX`
} _AkropolisWestElevatorHallDoorLeafWork;
STATIC_ASSERT_SIZEOF(_AkropolisWestElevatorHallDoorLeafWork, 4);

static Task* _gAkropolisWestElevatorHallDoorLeafTasks[2];

/// Keeps the reflection scale at this room's earlier rodata position.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 0
#include "../../../../shared/room/planar_reflection.h"
/// Binds the shared beacon body to this room's public `void (Task*)` callback.
#define RED_BEACON_TASK akropolisWestElevatorHallRedBeaconTask
#include "../../../../shared/effects/red_beacon.h"

static void _akropolisWestElevatorHallUpdateArrivalEvent(Task* unusedTask);
static void _akropolisWestElevatorHallInitializeRoomTask(Task* task);
static void _akropolisWestElevatorHallInitializeDoorLeaf(Task* task);
static void _akropolisWestElevatorHallUpdateDoorLeaf(Task* task);

/// Scale applied to held-object reflections in slots 2 and up: it mirrors them across X.
#include "../../../../shared/room/planar_reflection_rodata.inc.c"

/// Room-task states: 0 register/spawn leaves, 1 maintain the arrival event, 2 kill.
static const TaskFuncTable3 _gAkropolisWestElevatorHallRoomTaskStates = {
    { _akropolisWestElevatorHallInitializeRoomTask, _akropolisWestElevatorHallUpdateArrivalEvent, taskKill },
};

/// Door-leaf states: 0 initialize, 1 slide/draw, 2 kill and release its work.
static const TaskFuncTable3 _gAkropolisWestElevatorHallDoorLeafStates = {
    { _akropolisWestElevatorHallInitializeDoorLeaf, _akropolisWestElevatorHallUpdateDoorLeaf, taskKill },
};
#include "assets/rooms/akropolis/west_elevator_hall/markers.inc"

static s32 _akropolisWestElevatorHallOpenDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32 _akropolisWestElevatorHallCloseDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32 _akropolisWestElevatorHallResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _akropolisWestElevatorHallRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _akropolisWestElevatorHallStartBayLightingMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

/// Commands sent by the arrival script to the room and room-effect tasks.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_OPEN_DOOR          = 5100,
    AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_CLOSE_DOOR         = 5101,
    AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_START_BAY_LIGHTING = 3100,
};

/// Signed travel requests stored in a door leaf's first spawn argument.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE = -1,
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_OPEN  = 1,
};

/// Arrival-event phase for this overlay load; entering through warp 1 starts it.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WAITING  = 0,
    AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_RUNNING  = 1,
    AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_FINISHED = 2,
};

/// Door-leaf descriptor and live-handle indices, ordered by retraction along X.
enum {
    AKROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX = 0,
    AKROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX = 1,
    AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY  = 192,
};

static AnimationSet _gAkropolisWestElevatorHallArrivalAnim00Clip;
static TmdSource    _gAkropolisWestElevatorHallDoorLeafLeftModel;
static TmdSource    _gAkropolisWestElevatorHallDoorLeafRightModel;
static void         _akropolisWestElevatorHallDoorLeafTask(Task* task);

#include "../../../../shared/room/planar_reflection_data.inc.c"

/// Execution priority shared by the hall's reflected player and held models.
enum { AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_PRIORITY = 112 };

/// Spawn recipes for the hall's player reflection and its attachment reflections.
static TaskDesc _gAkropolisWestElevatorHallReflectionTaskTable[2] = {
    { { { TASK_BODY_NONE, AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, akropolisWestElevatorHallPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_PRIORITY } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return _gAkropolisWestElevatorHallReflectionTaskTable;
}

#include "assets/rooms/akropolis/west_elevator_hall/door_leaf_left.inc"

#include "assets/rooms/akropolis/west_elevator_hall/door_leaf_right.inc"

#include "assets/rooms/akropolis/west_elevator_hall/clips.inc"

/// Model-backed door-leaf descriptors, negative X then positive X, followed by END.
static TaskDesc _gAkropolisWestElevatorHallDoorLeafTaskTable[3] = {
    { { { TASK_BODY_TMD, AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY } }, _akropolisWestElevatorHallDoorLeafTask, { .model = &_gAkropolisWestElevatorHallDoorLeafLeftModel } },
    { { { TASK_BODY_TMD, AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_PRIORITY } }, _akropolisWestElevatorHallDoorLeafTask, { .model = &_gAkropolisWestElevatorHallDoorLeafRightModel } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// The single borrowed clip installed in the player's equipped-bank extension.
#include "assets/rooms/akropolis/west_elevator_hall/arrival_bank.inc"

#include "assets/rooms/akropolis/west_elevator_hall/events.inc"

/// One-shot arrival-event latch (0 waiting, 1 running, 2 finished) for this overlay load.
static s32 _gAkropolisWestElevatorHallArrivalPhase = AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WAITING;

/// Room messages: open/close the door, resolve transitions and reject key items, then END.
static TaskMessageEntry _gAkropolisWestElevatorHallRoomMessages[5] = {
    { AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_OPEN_DOOR, _akropolisWestElevatorHallOpenDoorMessage },
    { AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_CLOSE_DOOR, _akropolisWestElevatorHallCloseDoorMessage },
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisWestElevatorHallResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisWestElevatorHallRejectKeyItemUse },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Room-effect message to start the bay-light ramp, followed by END.
static TaskMessageEntry _gAkropolisWestElevatorHallViewEffectMessages[2] = {
    { AKROPOLIS_WEST_ELEVATOR_HALL_MESSAGE_START_BAY_LIGHTING, _akropolisWestElevatorHallStartBayLightingMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Black RGB555 palette used while the elevator bay is unlit.
static u16 _gAkropolisWestElevatorHallBayUnlitClut[256] = { 0 };

/// Authored 256-color RGB555 palette for the illuminated elevator bay.
static u16 _gAkropolisWestElevatorHallBayLitClut[256] = {
#include "assets/rooms/akropolis/west_elevator_hall/bay_lit_palette.inc"
};

/// Complete RGB555 bay palette interpolated between black and the lit colors.
static u16 _gAkropolisWestElevatorHallBayBlendedClut[256] = { 0 };

/// VRAM row of the elevator bay's 256-color lookup table.
enum { AKROPOLIS_WEST_ELEVATOR_HALL_BAY_CLUT_Y = 270 };

/// Uploads the full blended bay CLUT to VRAM, followed by the list terminator.
static GpuImageUpload _gAkropolisWestElevatorHallBayClutUploads[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, AKROPOLIS_WEST_ELEVATOR_HALL_BAY_CLUT_Y, ARRAY_SIZE(_gAkropolisWestElevatorHallBayBlendedClut), 1 }, (u_long*)_gAkropolisWestElevatorHallBayBlendedClut },
    { GPU_IMAGE_UPLOAD_END, 0, { 0, 0, 0, 0 }, NULL },
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
static Task* _gAkropolisWestElevatorHallDoorLeafTasks[2] = { NULL };

#include "../../../../shared/room/planar_reflection.inc.c"

void akropolisWestElevatorHallPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Starts the arrival scene once, on arrival through its warp, and begins the Akropolis chapter of the story.
static void _akropolisWestElevatorHallUpdateArrivalEvent(Task* unusedTask)
{
    enum { AKROPOLIS_WEST_ELEVATOR_HALL_SESSION_EVENT_IDLE = 0 };
    // Unused initialized words and reservation reproduce the original 0x230-byte frame.
    s32 retainedWords[2] = { 0, 4 };
    u8  retainedStackSpace[0x210];
    u8  arrivalWarp;

    if (_gAkropolisWestElevatorHallArrivalPhase == AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WAITING) {
        arrivalWarp = gGameSession->location.warp;
        if (arrivalWarp == AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WARP) {
            _gAkropolisWestElevatorHallArrivalPhase = arrivalWarp;
            evsStartScriptWithSkip(_gAkropolisWestElevatorHallArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisWestElevatorHallArrivalSkipScript);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_ARRIVAL);
            gameFlagSetNibble(EVENT_FLAG_STORY_CHAPTER, EVENT_FLAG_STORY_CHAPTER_AKROPOLIS);
            gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_OBJECTIVE);
        }
    }
    if (_gAkropolisWestElevatorHallArrivalPhase == AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_RUNNING && gGameSession->eventState == AKROPOLIS_WEST_ELEVATOR_HALL_SESSION_EVENT_IDLE) {
        _gAkropolisWestElevatorHallArrivalPhase = AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_FINISHED;
    }
}

/// Dispatches one mirrored leaf of the elevator's sliding door.
static void _akropolisWestElevatorHallDoorLeafTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisWestElevatorHallDoorLeafStates;
    stateHandlers.funcs[task->state](task);
}

/// Requests both elevator-door leaves to open on subsequent updates and returns 0.
static s32 _akropolisWestElevatorHallOpenDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _gAkropolisWestElevatorHallDoorLeafTasks[AKROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX]->spawnArg1.value = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_OPEN;
    _gAkropolisWestElevatorHallDoorLeafTasks[AKROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX]->spawnArg1.value = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_OPEN;
    return 0;
}

/// Requests both elevator-door leaves to close on subsequent updates and returns 0.
static s32 _akropolisWestElevatorHallCloseDoorMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _gAkropolisWestElevatorHallDoorLeafTasks[AKROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX]->spawnArg1.value = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE;
    _gAkropolisWestElevatorHallDoorLeafTasks[AKROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX]->spawnArg1.value = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSE;
    return 0;
}

/// The square's door: the first departure starts the patio's cutscene progress and arrives through the square's scene
/// warp.
static s32 _akropolisWestElevatorHallResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_SQUARE && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) == 0 && request->queryOnly == ROOM_EVENT_EXECUTE) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS, AKROPOLIS_PATIO_CUTSCENE_START);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_WEST_ELEVATOR_HALL_DEPARTURE_SCENE_EVENT;
        reply->warp                                         = AKROPOLIS_WEST_ELEVATOR_HALL_SQUARE_ARRIVAL_WARP;
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisWestElevatorHallRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Installs the room's messages and spawns the lift door's two leaves.
static void _akropolisWestElevatorHallInitializeRoomTask(Task* task)
{
    enum {
        AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_REST          = 0,
        AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_NEGATIVE = -1,
        AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_POSITIVE = 1,
    };
    task->msgTable = _gAkropolisWestElevatorHallRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gAkropolisWestElevatorHallDoorLeafTasks[AKROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX] =
        taskSpawnFromTable(_gAkropolisWestElevatorHallDoorLeafTaskTable, AKROPOLIS_WEST_ELEVATOR_HALL_NEGATIVE_LEAF_INDEX, AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_REST, AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_NEGATIVE);
    _gAkropolisWestElevatorHallDoorLeafTasks[AKROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX] =
        taskSpawnFromTable(_gAkropolisWestElevatorHallDoorLeafTaskTable, AKROPOLIS_WEST_ELEVATOR_HALL_POSITIVE_LEAF_INDEX, AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_REST, AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_SIDE_POSITIVE);
    task->state = task->state + 1;
}

void akropolisWestElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisWestElevatorHallRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Places a drawable leaf at the closed doorway in the current view hierarchy.
static inline void _akropolisWestElevatorHallPlaceClosedDoorLeaf(TmdObject* doorModel, GfxCoord* doorCoord)
{
    enum {
        AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Y = -20,
        AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Z = 2420,
    };

    doorModel->flags        = 0;
    doorCoord->parent       = &gGfxViewCoord;
    doorCoord->coord.t[0]   = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X;
    doorCoord->coord.t[1]   = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Y;
    doorCoord->coord.t[2]   = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_Z;
    doorCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Initializes a door leaf on the shut line and advances to its sliding state.
static void _akropolisWestElevatorHallInitializeDoorLeaf(Task* task)
{
    TmdObject*                              doorModel;
    GfxCoord*                               doorCoord;
    _AkropolisWestElevatorHallDoorLeafWork* leafWork;

    doorModel = task->extra.tmd;
    doorCoord = doorModel->coords;
    leafWork  = memCalloc(sizeof(*leafWork), 0);
    if (leafWork == NULL) {
        taskKill(task);
        return;
    }
    task->work       = leafWork;
    leafWork->travel = 0;
    _akropolisWestElevatorHallPlaceClosedDoorLeaf(doorModel, doorCoord);
    task->state++;
}

/// Integrates and clamps one door leaf's travel, then places it on its X side.
static inline void _akropolisWestElevatorHallSlideDoorLeaf(const Task* task, _AkropolisWestElevatorHallDoorLeafWork* leafWork, GfxCoord* doorCoord)
{
    leafWork->travel += task->spawnArg1.value * AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_SPEED;
    if (leafWork->travel < 0) {
        leafWork->travel = 0;
    }
    if (leafWork->travel > AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX) {
        leafWork->travel = AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_LEAF_TRAVEL_MAX;
    }
    doorCoord->coord.t[0] = (leafWork->travel * task->spawnArg2.value) + AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_CLOSED_X;
}

/// Slides one mirrored door leaf and updates its visibility and lighting.
static void _akropolisWestElevatorHallUpdateDoorLeaf(Task* task)
{
    enum { AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_VIEW = 5 };
    VECTOR                                  worldPosition;
    TmdObject*                              doorModel;
    GfxCoord*                               doorCoord;
    _AkropolisWestElevatorHallDoorLeafWork* leafWork;

    leafWork  = task->work;
    doorModel = task->extra.tmd;
    doorCoord = doorModel->coords;

    _akropolisWestElevatorHallSlideDoorLeaf(task, leafWork, doorCoord);
    if (gGameSession->location.view == AKROPOLIS_WEST_ELEVATOR_HALL_DOOR_VIEW) {
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

void akropolisWestElevatorHallViewEffectsTask(Task* task)
{
    enum {
        AKROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_INITIALIZE    = 0,
        AKROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_UPDATE        = 1,
        AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_VIEW           = 2,
        AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_VIEW             = 5,
        AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_BANK       = 1,
        AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_INDEX = 0x25,
        AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_RATE     = 4,
        AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_SIZE     = 24,
        AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_RATE     = 3,
        AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_SIZE     = 8,
    };
    SVECTOR   beaconPosition;
    SVECTOR   lightPosition;
    GfxCoord* parentCoord;

    /// Emits all three view-2 beacons, snapshotting each position before reuse.
#define AKROPOLIS_WEST_ELEVATOR_HALL_SPAWN_VIEW_BEACONS()                                                                                                                                                          \
    {                                                                                                                                                                                                              \
        beaconPosition = _gAkropolisWestElevatorHallFirstBeaconPosition;                                                                                                                                           \
        effectSpawn(EFFECT_AKROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_RATE, AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_LARGE_SIZE), &beaconPosition); \
        beaconPosition.vx = -0x1800;                                                                                                                                                                               \
        beaconPosition.vy = -0x4F0;                                                                                                                                                                                \
        beaconPosition.vz = -0x600;                                                                                                                                                                                \
        effectSpawn(EFFECT_AKROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_RATE, AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &beaconPosition); \
        beaconPosition.vx = -0x1800;                                                                                                                                                                               \
        beaconPosition.vy = -0x4F0;                                                                                                                                                                                \
        beaconPosition.vz = -0x2C0;                                                                                                                                                                                \
        effectSpawn(EFFECT_AKROPOLIS_WEST_ELEVATOR_HALL_RED_BEACON, parentCoord, RED_BEACON_ARG(AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_RATE, AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_SMALL_SIZE), &beaconPosition); \
    }

    parentCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case AKROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_INITIALIZE:
            task->msgTable = _gAkropolisWestElevatorHallViewEffectMessages;
            gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
            taskSpawn(AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_BANK, AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_INDEX, 0, 0);
            taskSpawn(AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_BANK, AKROPOLIS_WEST_ELEVATOR_HALL_REFLECTION_TASK_INDEX, 1, 0);
            task->state = task->state + 1;
            return;
        case AKROPOLIS_WEST_ELEVATOR_HALL_EFFECTS_UPDATE:
            if (gGameSession->location.view == AKROPOLIS_WEST_ELEVATOR_HALL_BEACON_VIEW) {
                AKROPOLIS_WEST_ELEVATOR_HALL_SPAWN_VIEW_BEACONS();
            }
            if (gGameSession->location.view == AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_VIEW) {
                lightPosition = _gAkropolisWestElevatorHallBayLightPosition;
                effectSpawn(EFFECT_AKROPOLIS_WEST_ELEVATOR_HALL_LIGHT, parentCoord, 0, &lightPosition);
            }
            return;
    }
#undef AKROPOLIS_WEST_ELEVATOR_HALL_SPAWN_VIEW_BEACONS
}

void akropolisWestElevatorHallBayLightingTask(Task* task)
{
    enum {
        AKROPOLIS_WEST_ELEVATOR_HALL_BAY_LIGHTING_VIEW = 5,
        AKROPOLIS_WEST_ELEVATOR_HALL_LIGHT_RAMPING     = 0,
        AKROPOLIS_WEST_ELEVATOR_HALL_LIGHT_HOLDING     = 1
    };
    EffectWork* work;
    s32         colorIndex;
    s32         paletteChanged;

    /// Blends and uploads the full bay palette, borrowing the result until GPU completion.
#define AKROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE(litWeightQ12, colorIndex)                                                                   \
    for ((colorIndex) = 0; (colorIndex) < (s32)ARRAY_SIZE(_gAkropolisWestElevatorHallBayBlendedClut); (colorIndex) += GPU_RGB555_CLUT_ROW_COLORS) { \
        gpuBlendRgb555ClutRow(&_gAkropolisWestElevatorHallBayLitClut[(colorIndex)],                                                                 \
                              &_gAkropolisWestElevatorHallBayUnlitClut[(colorIndex)], (litWeightQ12),                                               \
                              &_gAkropolisWestElevatorHallBayBlendedClut[(colorIndex)]);                                                            \
    }                                                                                                                                               \
    gpuUploadImages(_gAkropolisWestElevatorHallBayClutUploads)

    work           = task->spawnArg2.pointer;
    paletteChanged = 0;
    // The signed halfwords hold a Q12 lit weight and a ramp-complete latch here.
    if (work->angle == AKROPOLIS_WEST_ELEVATOR_HALL_LIGHT_RAMPING) {
        work->scale = work->scale + ONE / 2;
        if (work->scale == ONE) {
            work->angle = AKROPOLIS_WEST_ELEVATOR_HALL_LIGHT_HOLDING;
        }
        paletteChanged = 1;
    }

    if (paletteChanged != 0) {
        AKROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE(work->scale, colorIndex);
    }

    // Restore the palette before releasing the effect, even during its ramp.
    if (gGameSession->location.view != AKROPOLIS_WEST_ELEVATOR_HALL_BAY_LIGHTING_VIEW) {
        AKROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE(0, colorIndex);
        effectKillTask(work, task);
    }
#undef AKROPOLIS_WEST_ELEVATOR_HALL_UPLOAD_BAY_PALETTE
}

#include "../../../../shared/effects/red_beacon_task.inc.c"
#undef RED_BEACON_TASK

/// Queues one displaced framebuffer row at the distortion strip's fixed X origin.
static inline void _akropolisWestElevatorHallQueueScanlineCopy(RECT* sourceRect, s32 destinationY, s32 otIndex)
{
    enum { AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DESTINATION_X = 80 };
    DR_MOVE* movePacket;

    movePacket     = gGpuPrimCursor;
    gGpuPrimCursor = movePacket + 1;
    SetDrawMove(movePacket, sourceRect, AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DESTINATION_X, destinationY);
    addPrim(&gGpuCurrentOt[otIndex], movePacket);
}

void akropolisWestElevatorHallScanlineDistortionTask(Task* task)
{
    enum {
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_BUFFER_STRIDE = 272,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN        = 80,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ROWS          = 82,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_WIDTH         = 120,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_OT_INDEX      = 0x72,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_RANDOM_PHASE  = 0x80,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MASK  = 0x3F,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MIN   = 192,
        AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR       = 256
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
    stripTopY  = gDisplayState.drawBuffer * AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_BUFFER_STRIDE + AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN;

    // Bit 7 selects 128-frame intervals with a separate random divisor per row.
    for (rowIndex = 0; rowIndex < AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ROWS; rowIndex++) {
        otIndex         = AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_OT_INDEX;
        sourceY         = rowIndex;
        sourceY        += stripTopY;
        displacementQ12 = ONE / 2 - rcos((gDisplayState.animFrame + rowIndex * 2) * 16);
        if (gDisplayState.animFrame & AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_RANDOM_PHASE) {
            RANDOM_LCG_ADVANCE();
            sourceX = displacementQ12 / (s32)((RANDOM_LCG_DRAW() & AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MASK) + AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR_MIN) + AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN;
        } else {
            sourceX = displacementQ12 / AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_DIVISOR + AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_ORIGIN;
        }

        sourceRect.x = sourceX;
        sourceRect.y = sourceY;
        sourceRect.w = AKROPOLIS_WEST_ELEVATOR_HALL_DISTORTION_WIDTH;
        sourceRect.h = 1;
        _akropolisWestElevatorHallQueueScanlineCopy(&sourceRect, rowIndex + stripTopY, otIndex);
    }

    effectKillTask(effectWork, task);
}

/// Sets the hall light's square billboard coordinates around its projected center.
static inline void _akropolisWestElevatorHallSetLightGlowBounds(POLY_FT4* glowQuad, const RoomGlowSpriteScratch* glowScratch)
{
    glowQuad->x0 = glowQuad->x2 = glowScratch->screenPos.vx - glowScratch->halfExtent;
    glowQuad->x1 = glowQuad->x3 = glowScratch->screenPos.vx + glowScratch->halfExtent;
    glowQuad->y0 = glowQuad->y1 = glowScratch->screenPos.vy - glowScratch->halfExtent;
    glowQuad->y2 = glowQuad->y3 = glowScratch->screenPos.vy + glowScratch->halfExtent;
}

/// Projects the hall light's composed view-space origin into an additive textured billboard.
static inline void _akropolisWestElevatorHallDrawLightGlow(const GfxCoord* effectCoord, RoomGlowSpriteScratch* glowScratch)
{
    enum {
        AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_MIN_DEPTH     = 17,
        AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_TEXTURE_8_BIT = 1,
        AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_TPAGE         = getTPage(AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_TEXTURE_8_BIT, GPU_BLEND_ADD, 704, 0),
        AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_CLUT          = getClut(0, 270),
        AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX        = 103,   // Inclusive last texel of the 104-by-104 image
        AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_SIZE_FACTOR   = 0x6700 // Pixel half-extent numerator in SZ3 / 4 depth units
    };
    POLY_FT4* glowQuad;

    // The effect's parent chain has already transformed this center into view space.
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
    if (glowScratch->otz >= AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_MIN_DEPTH) {
        glowQuad->tpage = AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_TPAGE;
        glowQuad->clut  = AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_CLUT;
        glowQuad->u0    = 0;
        glowQuad->v0    = 0;
        glowQuad->u1    = AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        glowQuad->v1    = 0;
        glowQuad->u2    = 0;
        glowQuad->v2    = AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        glowQuad->u3    = AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        glowQuad->v3    = AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_UV_MAX;
        setSemiTrans(glowQuad, 1);
        // Raw texture mode bypasses RGB modulation, so the color bytes are unused.
        setShadeTex(glowQuad, 1);
        glowScratch->halfExtent = AKROPOLIS_WEST_ELEVATOR_HALL_GLOW_SIZE_FACTOR / glowScratch->otz;
        _akropolisWestElevatorHallSetLightGlowBounds(glowQuad, glowScratch);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(glowScratch->otz),
                glowQuad);
    }
}

void akropolisWestElevatorHallLightGlowTask(Task* task)
{
    RoomGlowSpriteScratch* glowScratch;
    GfxCoord*              effectCoord;
    EffectWork*            effectWork;

    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    glowScratch = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    _akropolisWestElevatorHallDrawLightGlow(effectCoord, glowScratch);
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(effectWork, task);
}

/// Starts the bay's palette-lighting effect and returns 0.
static s32 _akropolisWestElevatorHallStartBayLightingMessage(Task* unusedTask, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    effectSpawn(EFFECT_ACROPOLIS_WEST_ELEVATOR_BAY_LIGHTS, NULL, 0, NULL);
    return 0;
}
