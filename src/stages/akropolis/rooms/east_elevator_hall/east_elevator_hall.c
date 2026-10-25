#include "stages/akropolis/rooms/east_elevator_hall.h"

#include "config.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/game_debug.h"
#include "gameplay/gameflag.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// The reflection's scale is in this room's rodata, before the shared implementation.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 0
#include "../../../../shared/room/planar_reflection.h"

/// The shared red beacon, as this room's task.
#define RED_BEACON_TASK akropolisEastElevatorHallRedBeaconTask
#include "../../../../shared/effects/red_beacon.h"

#include "assets/rooms/akropolis/east_elevator_hall/declarations.inc"

static void _akropolisEastElevatorHallInitializeRoomTask(Task* task);
static void _akropolisEastElevatorHallUpdatePlayerDebugDisplay(Task* unusedTask);

#include "../../../../shared/room/planar_reflection_rodata.inc.c"

/// Room task states: initialize, the player debug display, release.
static const TaskFuncTable3 _gAkropolisEastElevatorHallRoomTaskStates = {
    { _akropolisEastElevatorHallInitializeRoomTask, _akropolisEastElevatorHallUpdatePlayerDebugDisplay, taskKill },
};

static s32  _akropolisEastElevatorHallResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisEastElevatorHallRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisEastElevatorHallHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32  _akropolisEastElevatorHallHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static void _akropolisEastElevatorHallStartSkipCapSequence(void);

#include "../../../../shared/room/planar_reflection_data.inc.c"

/// Reflection tasks: the player (0) and their attachment (1).
static TaskDesc _gAkropolisEastElevatorHallReflectionTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, akropolisEastElevatorHallPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_EFFECT } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// The reflection task table, for `planar_reflection.inc.c`.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return _gAkropolisEastElevatorHallReflectionTaskDescs;
}

#include "assets/rooms/akropolis/east_elevator_hall/clips.inc"

/// The player's animations in the opening: its main part, its lead-in and its exit.
#include "assets/rooms/akropolis/east_elevator_hall/opening_player_main_bank.inc"
#include "assets/rooms/akropolis/east_elevator_hall/opening_player_lead_in_bank.inc"
#include "assets/rooms/akropolis/east_elevator_hall/opening_player_exit_bank.inc"

#include "assets/rooms/akropolis/east_elevator_hall/events.inc"

/// Messages of the room task.
static TaskMessageEntry _gAkropolisEastElevatorHallRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisEastElevatorHallResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _akropolisEastElevatorHallHandleRoomCommand },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisEastElevatorHallRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisEastElevatorHallHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Set once the opening has started in this visit.
static s32 _gAkropolisEastElevatorHallOpeningEventStarted = 0;
#include "assets/rooms/akropolis/east_elevator_hall/header.inc"
#include "assets/rooms/akropolis/east_elevator_hall/geometry.inc"
#include "assets/rooms/akropolis/east_elevator_hall/view_boundaries.inc"
#include "assets/rooms/akropolis/east_elevator_hall/action_triggers.inc"
#include "assets/rooms/akropolis/east_elevator_hall/placements.inc"
#include "assets/rooms/akropolis/east_elevator_hall/sprites.inc"
#include "assets/rooms/akropolis/east_elevator_hall/lights.inc"
#include "assets/rooms/akropolis/east_elevator_hall/cameras.inc"
#include "assets/rooms/akropolis/east_elevator_hall/surfaces.inc"

#include "../../../../shared/room/planar_reflection.inc.c"

void akropolisEastElevatorHallPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Lets every transition through unchanged.
static s32 _akropolisEastElevatorHallResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisEastElevatorHallRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return 0;
}

/// Records the story progress of the opening as it starts, so skipping it keeps the same state: the player has
/// entered Akropolis, Baldwin's next call is the civilian evacuation, and the patio's cafeteria door is unlocked.
static inline void _akropolisEastElevatorHallRecordOpeningProgress(void)
{
    _gAkropolisEastElevatorHallOpeningEventStarted = 1;
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS, AKROPOLIS_CAFETERIA_ROUTE_ENTERED_AKROPOLIS);
    gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
    gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_CIVILIAN_EVAC);
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_STATE, AKROPOLIS_PATIO_CAFETERIA_DOOR_UNLOCKED);
    gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_AKROPOLIS_CAFETERIA_KEY_RECEIVED);
}

/// Starts the opening, the first time the player passes into the hall.
static s32 _akropolisEastElevatorHallHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if (request->actionId == AKROPOLIS_EAST_ELEVATOR_HALL_OPENING_ACTION &&
        gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) == AKROPOLIS_CAFETERIA_ROUTE_NOT_STARTED &&
        _gAkropolisEastElevatorHallOpeningEventStarted == 0) {
        evsStartScriptWithSkip(_gAkropolisEastElevatorHallOpeningScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisEastElevatorHallOpeningSkipScript);
        _akropolisEastElevatorHallRecordOpeningProgress();
    }
    return 0;
}

/// Room command 2, talking to the SWAT member again: "Go! Hurry...".
static s32 _akropolisEastElevatorHallHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    if (command == AKROPOLIS_EAST_ELEVATOR_HALL_ROOM_COMMAND_URGE_HURRY) {
        evsStartScript(_gAkropolisEastElevatorHallUrgeHurryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Ends the opening's skip script with the opening's last CAP step.
static void _akropolisEastElevatorHallStartSkipCapSequence(void)
{
    capStartSequenceSlot(AKROPOLIS_EAST_ELEVATOR_HALL_SKIP_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
}

/// Installs the room's messages and shows the SWAT member at rest.
static void _akropolisEastElevatorHallInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisEastElevatorHallRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sceneSetPlacedActorDrawMode(AKROPOLIS_EAST_ELEVATOR_HALL_SWAT_MEMBER_ACTOR, ACTOR_MESSAGE_DRAW_SHOW);
    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(AKROPOLIS_EAST_ELEVATOR_HALL_SWAT_MEMBER_ACTOR), ACTOR_MESSAGE_PLAY_ANIMATION, &_gAkropolisEastElevatorHallSwatRestAnimationRequest, 0);
    task->state++;
}

/// The player's label in the debug display; its last byte is never shown.
static const char _gAkropolisEastElevatorHallPlayerDebugLabel[8] = "Player\0\x0F";
#include "assets/rooms/akropolis/east_elevator_hall/markers.inc"

/// Shows the player in the debug display.
static void _akropolisEastElevatorHallUpdatePlayerDebugDisplay(Task* unusedTask)
{
    if (gDisplayState.debugMode != 0) {
        func_807245E4(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        if (gDisplayState.debugMode != 0) {
            func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), AKROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_X,
                          AKROPOLIS_EAST_ELEVATOR_HALL_PLAYER_DEBUG_Y, _gAkropolisEastElevatorHallPlayerDebugLabel);
        }
    }
}

void akropolisEastElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisEastElevatorHallRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// The two sizes of beacon, as their spawn arguments.
#define BEACON_SMALL RED_BEACON_ARG(AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_RATE, AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_SMALL_SIZE)
#define BEACON_LARGE RED_BEACON_ARG(AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_RATE, AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_LARGE_SIZE)

/// The room's effects: player reflections, and in view 2 the six red beacons, which last one frame each.
void akropolisEastElevatorHallReflectionsAndBeaconsTask(Task* task)
{
    enum {
        INITIALIZE = 0,
        UPDATE     = 1,
    };
    GfxCoord* parentCoord;

    parentCoord = task->extra.coordBody->coord;
    switch (task->state) {
        case INITIALIZE:
            taskSpawn(AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK, AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT, AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_FLOOR, 0);
            taskSpawn(AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_BANK, AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_TASK_SLOT, AKROPOLIS_EAST_ELEVATOR_HALL_REFLECTION_PLANE, 0);
            task->state++;
            /* fallthrough */
        case UPDATE:
            if (gGameSession->location.view == AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_VIEW) {
                SVECTOR spawnOffset = _gAkropolisEastElevatorHallFirstBeaconOffset;

                effectSpawn(EFFECT_AKROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, BEACON_SMALL, &spawnOffset);
                spawnOffset.vx = AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_WALL_X;
                spawnOffset.vy = -2437;
                spawnOffset.vz = 85;
                effectSpawn(EFFECT_AKROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, BEACON_SMALL, &spawnOffset);
                spawnOffset.vx = AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_WALL_X;
                spawnOffset.vy = -2689;
                spawnOffset.vz = -458;
                effectSpawn(EFFECT_AKROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, BEACON_LARGE, &spawnOffset);
                spawnOffset.vx = AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_WALL_X;
                spawnOffset.vy = -2707;
                spawnOffset.vz = -1564;
                effectSpawn(EFFECT_AKROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, BEACON_LARGE, &spawnOffset);
                spawnOffset.vx = AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_WALL_X;
                spawnOffset.vy = -1120;
                spawnOffset.vz = -417;
                effectSpawn(EFFECT_AKROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, BEACON_LARGE, &spawnOffset);
                spawnOffset.vx = AKROPOLIS_EAST_ELEVATOR_HALL_BEACON_WALL_X;
                spawnOffset.vy = -1097;
                spawnOffset.vz = -1589;
                effectSpawn(EFFECT_AKROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, parentCoord, BEACON_LARGE, &spawnOffset);
            }
            break;
    }
}

#include "../../../../shared/effects/red_beacon_task.inc.c"
#undef RED_BEACON_TASK

/// Projects the coordinate's position and draws it as one additive gray pixel.
static inline void _akropolisEastElevatorHallDrawPointTile(const GfxCoord* composedCoord, EffectPointTileScratch* tileScratch)
{
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
        setRGB0(tile, AKROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_GRAY, AKROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_GRAY, AKROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_GRAY);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(tileScratch->depth), tile);
        // Prepend the draw-mode command at the same depth so blending is set before the tile.
        gpuSetPrimitiveBlendMode(tile, GPU_BLEND_ADD, tileScratch->depth);
    }
}

void akropolisEastElevatorHallPointTileTask(Task* task)
{
    EffectPointTileScratch* tileScratch;
    GfxCoord*               effectCoord;
    void*                   effectWork;

    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
    _akropolisEastElevatorHallDrawPointTile(effectCoord, tileScratch);
    SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    effectKillTask(effectWork, task);
}
