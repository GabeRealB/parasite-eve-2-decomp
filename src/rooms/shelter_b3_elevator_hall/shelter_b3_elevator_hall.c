#include "rooms/shelter_b3_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b3_elevator_hall_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/shelter_elevator.h"

/// Set when the gate latched a request and spawned the task that runs it.
/// Descriptor of the task that runs a latched request.
extern TaskDesc gRoomEventTaskDesc;
extern TaskDesc D_shelter_b3_elevator_hall_80182A2C[];
extern TaskDesc D_shelter_b3_elevator_hall_80182A68[];
/// The room's message table, which its CAP scripts index.
extern TaskMessageEntry D_shelter_b3_elevator_hall_80182A38[];
extern SVECTOR          D_shelter_b3_elevator_hall_80182A74[];
extern SVECTOR          D_shelter_b3_elevator_hall_80182AB4[];
extern SVECTOR          D_shelter_b3_elevator_hall_80182AF4[];
/// Per-palette right shifts applied to the halo's level for red, green and
/// blue, selected by the palette index in the spawn argument.

static void _shelterB3ElevatorHallInitRoomTask(Task* task);
static void _shelterB3ElevatorHallIdleRoomTask(Task* task);

static void _shelterB3ElevatorHallElevatorTransitTask(Task* task);
static s32  _shelterB3ElevatorHallRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _shelterB3ElevatorHallResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB3ElevatorHallIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _shelterB3ElevatorHallIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB3ElevatorHallHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg);

enum { SHELTER_B3_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b3_elevator_hall_80182A2C[1] = {
    { { { TASK_BODY_NONE, 32 } }, shelterElevatorTask, { .value = 0 } },
};

TaskMessageEntry D_shelter_b3_elevator_hall_80182A38[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB3ElevatorHallResolveRoomTransition },
    { SHELTER_B3_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM, _shelterB3ElevatorHallRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB3ElevatorHallIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB3ElevatorHallIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, _shelterB3ElevatorHallHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b3_elevator_hall_80182A68[1] = {
    { { { TASK_BODY_NONE, 32 } }, _shelterB3ElevatorHallElevatorTransitTask, { .value = 0 } },
};

SVECTOR D_shelter_b3_elevator_hall_80182A74[8] = {
    { -5771, -3058, 151, 0 },
    { -5771, -3058, -1003, 0 },
    { -5653, -3058, 151, 0 },
    { -5653, -3058, -1003, 0 },
    { -3341, -3058, -1525, 0 },
    { -2185, -3058, -1525, 0 },
    { -3341, -3058, -1644, 0 },
    { -2185, -3058, -1644, 0 },
};

SVECTOR D_shelter_b3_elevator_hall_80182AB4[8] = {
    { -357, -3058, -1525, 0 },
    { 798, -3058, -1525, 0 },
    { -357, -3058, -1644, 0 },
    { 798, -3058, -1644, 0 },
    { 2609, -3058, -1525, 0 },
    { 3763, -3058, -1525, 0 },
    { 2609, -3058, -1644, 0 },
    { 3763, -3058, -1644, 0 },
};

SVECTOR D_shelter_b3_elevator_hall_80182AF4[8] = {
    { 3863, -2122, 3666, 0 },
    { 5022, -2122, 3666, 0 },
    { 3863, -2032, 3742, 0 },
    { 5022, -2032, 3742, 0 },
    { 7230, -2032, 3742, 0 },
    { 8394, -2032, 3742, 0 },
    { 7230, -2122, 3666, 0 },
    { 8394, -2122, 3666, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x0695 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/shelter_elevator_task.inc.c"

/// State table of the room's message-driven task: publish the message table,
/// idle, then kill the task.
static const TaskFuncTable3 D_shelter_b3_elevator_hall_8017D5F0 = {
    {
        _shelterB3ElevatorHallInitRoomTask,
        _shelterB3ElevatorHallIdleRoomTask,
        taskKill,
    },
};

/// Offers elevator travel and either opens floor selection or enters the cab.
///
/// Runs at states 0..6 with loaded CAP and sound resources, holding player and
/// scene actors. A nonzero access flag opens the floor-selection task. Otherwise
/// CAP choice 21 commits B2 elevator room 1/warp 1, waits for the ride sound and
/// reloads gameplay; other choices resume actors and kill this task. Owns no work.
static void _shelterB3ElevatorHallElevatorTransitTask(Task* task)
{
    enum {
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_HOLD,
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_WAIT_ENTRY_CAP,
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_OFFER,
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_WAIT_OFFER_CAP,
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_CHOOSE,
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_WAIT_SOUND,
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_RELOAD,
        SHELTER_B3_ELEVATOR_HALL_SELECT_FLOOR_CAP_COMMAND = 4,
        SHELTER_B3_ELEVATOR_HALL_ENTER_CAB_CAP_COMMAND    = 3,
        SHELTER_B3_ELEVATOR_HALL_ENTER_CAB_CHOICE         = 0x15,
        SHELTER_B3_ELEVATOR_HALL_CAB_WARP                 = 1,
        SHELTER_B3_ELEVATOR_HALL_CAB_ROOM                 = 1,
        SHELTER_B3_ELEVATOR_HALL_FLOOR_SELECT_TASK_INDEX  = 0,
        SHELTER_B3_ELEVATOR_HALL_DEFAULT_SPRITE_VARIANT   = 1
    };

    switch (task->state) {
        case SHELTER_B3_ELEVATOR_HALL_TRANSIT_HOLD:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state++;
            break;
        case SHELTER_B3_ELEVATOR_HALL_TRANSIT_WAIT_ENTRY_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B3_ELEVATOR_HALL_TRANSIT_OFFER:
            if (gameFlagGetNibble(GAME_FLAG_0CF) != 0) {
                capRunCommand(SHELTER_B3_ELEVATOR_HALL_SELECT_FLOOR_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE);
                taskSpawnFromTable(D_shelter_b3_elevator_hall_80182A2C, SHELTER_B3_ELEVATOR_HALL_FLOOR_SELECT_TASK_INDEX, SOUND_SHELTER_B3_ELEVATOR_RIDE, 0);
                taskKill(task);
            } else {
                capRunCommandWithTransition(SHELTER_B3_ELEVATOR_HALL_ENTER_CAB_CAP_COMMAND);
            }
            task->state++;
            break;
        case SHELTER_B3_ELEVATOR_HALL_TRANSIT_WAIT_OFFER_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B3_ELEVATOR_HALL_TRANSIT_CHOOSE:
            if (capGetVariantKey() == SHELTER_B3_ELEVATOR_HALL_ENTER_CAB_CHOICE) {
                // The accepted ride selects the cab before waiting for its sound.
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B2_ELEVATOR;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = SHELTER_B3_ELEVATOR_HALL_CAB_WARP;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = SHELTER_B3_ELEVATOR_HALL_CAB_ROOM;
            } else {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
            }
            task->state++;
            break;
        case SHELTER_B3_ELEVATOR_HALL_TRANSIT_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B3_ELEVATOR_RIDE) == 0) {
                task->state++;
            }
            break;
        case SHELTER_B3_ELEVATOR_HALL_TRANSIT_RELOAD:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant = SHELTER_B3_ELEVATOR_HALL_DEFAULT_SPRITE_VARIANT;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}

/// Refuses every key-item use request, returning zero without consuming the item.
static s32 _shelterB3ElevatorHallRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Resolves room departures, gates the control-room door and starts elevator travel.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with complete borrowed eight-byte
/// records, which may alias. The door returns the event gate's 0/1/2 result.
/// Other ordinary departures return 1; the elevator returns 0 and only execute
/// requests enable access, play the first-use CAP and spawn the transit task.
/// The receiver and message ID are unused. Keep this overlay loaded through travel.
static s32 _shelterB3ElevatorHallResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_CAP_COMMAND   = 1,
        SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_FIRST_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 5),
        SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_SECOND_SOUND  = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 3),
        SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_NO_COLLECTION = 0,
        SHELTER_B3_ELEVATOR_HALL_FIRST_USE_CAP_COMMAND      = 2,
        SHELTER_B3_ELEVATOR_HALL_ELEVATOR_ENABLED           = 1,
        SHELTER_B3_ELEVATOR_HALL_TRANSIT_TASK_INDEX         = 0,
        SHELTER_B3_ELEVATOR_HALL_TRANSITION_ALLOWED         = 1,
        SHELTER_B3_ELEVATOR_HALL_TRANSITION_HANDLED         = 0
    };
    RoomEventReq doorEvent;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM) {
        doorEvent.capCmd        = SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_CAP_COMMAND;
        doorEvent.missingCapCmd = SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_CAP_COMMAND;
        doorEvent.firstSnd      = SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_FIRST_SOUND;
        doorEvent.secondSnd     = SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_SECOND_SOUND;
        doorEvent.flagId        = GAME_FLAG_B3_INCINERATOR_CONTROL_DOOR_UNLOCKED;
        doorEvent.collectedBit  = SHELTER_B3_ELEVATOR_HALL_CONTROL_DOOR_NO_COLLECTION;
        return _roomEventGate(&doorEvent, reply);
    }
    if (request->areaId != GAME_AREA_SHELTER_B2_ELEVATOR) {
        return SHELTER_B3_ELEVATOR_HALL_TRANSITION_ALLOWED;
    }
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED) == 0) {
            capRunCommandWithTransition(SHELTER_B3_ELEVATOR_HALL_FIRST_USE_CAP_COMMAND);
            gameFlagSetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED, SHELTER_B3_ELEVATOR_HALL_ELEVATOR_ENABLED);
        }
        taskSpawnFromTable(D_shelter_b3_elevator_hall_80182A68, SHELTER_B3_ELEVATOR_HALL_TRANSIT_TASK_INDEX, SOUND_SHELTER_B3_ELEVATOR_RIDE, 0);
    }
    return SHELTER_B3_ELEVATOR_HALL_TRANSITION_HANDLED;
}

/// Ignores room commands and both argument words, returning zero.
static s32 _shelterB3ElevatorHallIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores the borrowed room-action request, returning zero without reading it.
static s32 _shelterB3ElevatorHallIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Requests the elevator ride sound for cue 1; every cue returns zero.
static s32 _shelterB3ElevatorHallHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum { ELEVATOR_RIDE_CUE = 1 };

    if (cueId == ELEVATOR_RIDE_CUE) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_ELEVATOR_RIDE, 0, 0);
    }
    return 0;
}

/// Installs the room's message handlers and registers its task as the room receiver.
///
/// State 0 advances to the idle state; no task work or body is allocated.
static void _shelterB3ElevatorHallInitRoomTask(Task* task)
{
    task->msgTable = D_shelter_b3_elevator_hall_80182A38;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Keeps the room receiver alive in state 1 while messages perform its work.
static void _shelterB3ElevatorHallIdleRoomTask(Task* task)
{
}

void shelterB3ElevatorHallRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b3_elevator_hall_8017D5F0;
    states.funcs[task->state](task);
}

/// Draws the hall's four additive strip glows from consecutive endpoint pairs.
///
/// Borrows eight word-aligned world-space endpoints during the call, pairing
/// indices 0/1, 2/3, 4/5 and 6/7. Each end's pixel radius is 384 * 64 / depth,
/// where depth is camera Z / 4 and must be nonzero for accepted projections.
/// A negative GTE flag at either end rejects that strip. RGB intensity is 32
/// on even animation frames and 40 on odd frames, fading to a black rim.
///
/// Requires the composed view matrix, scratch-stack room for
/// `OverlayPointPairScratch`, and the current frame's arena and ordering table.
/// Queues up to 24 `POLY_G4` packets and 24 additive blend commands; packets
/// remain in the frame arena until GPU drawing completes.
static inline void _shelterB3ElevatorHallDrawGlowStrips(const SVECTOR stripEndpoints[8])
{
    enum { STRIP_RADIUS_SCALE = 0x180,
           STRIP_RGB_NIBBLES  = 0x222 };

    _glowDrawCapsule(&stripEndpoints[0], STRIP_RADIUS_SCALE, STRIP_RGB_NIBBLES);
    _glowDrawCapsule(&stripEndpoints[2], STRIP_RADIUS_SCALE, STRIP_RGB_NIBBLES);
    _glowDrawCapsule(&stripEndpoints[4], STRIP_RADIUS_SCALE, STRIP_RGB_NIBBLES);
    _glowDrawCapsule(&stripEndpoints[6], STRIP_RADIUS_SCALE, STRIP_RGB_NIBBLES);
}

void shelterB3ElevatorHallDrawGlowsTask(Task* task)
{
    enum { GLOW_INITIALIZE,
           GLOW_DRAW };

    // Publish this loaded room's effect callbacks before drawing its fixed glows.
    if (task->state == GLOW_INITIALIZE) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B3_ELEVATOR_HALL_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B3_ELEVATOR_HALL_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B3_ELEVATOR_HALL_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B3_ELEVATOR_HALL_SPARK_EMITTER;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B3_ELEVATOR_HALL_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B3_ELEVATOR_HALL_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B3_ELEVATOR_HALL_ORANGE_BURST_2;
        task->state               = GLOW_DRAW;
    }
    switch ((u8)viewGetMappedIndex()) {
        case 3: {
            const SVECTOR* view3StripPoints = D_shelter_b3_elevator_hall_80182A74;
            _shelterB3ElevatorHallDrawGlowStrips(view3StripPoints);
        } break;
        case 4: {
            const SVECTOR* view4StripPoints = D_shelter_b3_elevator_hall_80182AB4;
            _shelterB3ElevatorHallDrawGlowStrips(view4StripPoints);
        } break;
        case 6: {
            const SVECTOR* view6StripPoints = D_shelter_b3_elevator_hall_80182AF4;
            _shelterB3ElevatorHallDrawGlowStrips(view6StripPoints);
        } break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void shelterB3ElevatorHallRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void shelterB3ElevatorHallRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB3ElevatorHallRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void shelterB3ElevatorHallRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}
