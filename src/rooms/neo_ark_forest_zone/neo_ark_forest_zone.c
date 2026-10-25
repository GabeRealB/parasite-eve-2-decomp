#include "rooms/neo_ark_forest_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_forest_zone_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
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

#include "mapui/map_neo_ark.h"

#include "../../shared/room/room_variants.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

/// Selects the forest's twin-trail endpoint table defined in neo_ark_forest_zone_2.c.
///
/// Empty presence flag needed only while including the shared header, which
/// declares the external table of two SVECTOR entries and binds the task's accesses.
#define ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../shared/room/room_visual_effects.h"
#undef ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
#include "../../shared/room/room_events.h"
#include "../../shared/effects/falling_leaves.h"

static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

/// Payload handed to the helper task 0x31 the event may start.
extern RoomFadeStorage gRoomEventFade;

static void _neoArkForestZoneInitializeRoom(Task* task);
static void _neoArkForestZonePauseRoamersState(Task* task);

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest transition-event gate call requested a controller spawn (0/1).
///
/// Every call clears it, including queries and already-seen events. Execution
/// sets it after the spawn attempt even on failure; it does not track task
/// lifetime. There is no current reader. Storage remains one signed byte.
static s8 _gNeoArkForestZoneEventStartRequested = 0;

/// Retained, unreferenced byte containing 123; its role is unproven.
static u8 D_neo_ark_forest_zone_80182E41 = 123;

/// Retained, unreferenced byte containing 4; its role is unproven.
static u8 D_neo_ark_forest_zone_80182E42 = 4;

/// Retained, unreferenced byte containing 20; its role is unproven.
static u8 D_neo_ark_forest_zone_80182E43 = 20;

ActorCommand gRoamerCommand = { 0 };

RoomLatchedEvent gRoomEventLatched = { 0 };

s16 gRoamerReserveHp[5] = {
    0,
    0,
    0,
    0,
    0,
};

#include "../../shared/room/room_event_staged_task.inc.c"

s32 neoArkForestZoneRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Tests and, in execute mode, latches a room-transition event.
///
/// Returns 1 when the nonzero event flag is already set, otherwise 2, including
/// queries. Every call clears the latest-start marker. Only `ROOM_EVENT_EXECUTE`
/// copies the complete eight-byte transition and twelve-byte event, sets a
/// nonzero flag to 1 and spawns the staged controller. Flag 0 stays eligible.
/// Both inputs are borrowed during this call; their copies and CAP/sound
/// resources must remain in the loaded room until the controller finishes.
static inline s32 _neoArkForestZoneStartEvent(const RoomEventMsg* transition, const RoomLatchedEvent* event)
{
    enum {
        ROOM_EVENT_NOT_STARTED  = 0,
        ROOM_EVENT_STARTED      = 1,
        ROOM_EVENT_FLAG_SEEN    = 1,
        ROOM_EVENT_ALREADY_SEEN = 1,
        ROOM_EVENT_ELIGIBLE     = 2
    };

    _gNeoArkForestZoneEventStartRequested = ROOM_EVENT_NOT_STARTED;
    if (gameFlagGetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (transition->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *transition;
            gRoomEventLatched             = *event;
            if (event->flagId != 0) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_SEEN);
            }
            taskSpawnFromTable(&gNeoArkForestZoneStagedEventTaskDesc, 0, 0, 0);
            _gNeoArkForestZoneEventStartRequested = ROOM_EVENT_STARTED;
        }
        return ROOM_EVENT_ELIGIBLE;
    }
    return ROOM_EVENT_ALREADY_SEEN;
}

s32 neoArkForestZoneResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        NEO_ARK_FOREST_ZONE_CAP_WOODLAND_DEPARTURE   = 2,
        NEO_ARK_FOREST_ZONE_AMBIENCE_STOP_CONTROL    = 60,
        NEO_ARK_FOREST_ZONE_WOODLAND_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 3),
    };
    RoomLatchedEvent event;

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_NEO_ARK_FOREST_ZONE_AMBIENCE, NEO_ARK_FOREST_ZONE_AMBIENCE_STOP_CONTROL);
    }
    if (request->areaId != GAME_AREA_NEO_ARK_WOODLAND_PATH) {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    event.capCmd   = NEO_ARK_FOREST_ZONE_CAP_WOODLAND_DEPARTURE;
    event.stageSnd = NEO_ARK_FOREST_ZONE_WOODLAND_DEPARTURE_SOUND;
    event.flagId   = GAME_FLAG_FOREST_ZONE_TO_WOODLAND_PATH_SCENE;
    event.fade     = 0;
    return _neoArkForestZoneStartEvent(reply, &event);
}

s32 neoArkForestZoneIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg)
{
    enum { NEO_ARK_FOREST_ZONE_COMMAND_IGNORED = 0 };

    return NEO_ARK_FOREST_ZONE_COMMAND_IGNORED;
}

s32 neoArkForestZoneHandleAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    enum {
        NEO_ARK_FOREST_ZONE_FIRST_VISIT_ACTION = 1,
        NEO_ARK_FOREST_ZONE_FIRST_VISIT_SEEN   = 1,
        NEO_ARK_FOREST_ZONE_POOL_ABSENT        = -1,
    };
    u8 actionId;

    actionId = request->actionId;
    if (actionId == NEO_ARK_FOREST_ZONE_FIRST_VISIT_ACTION) {
        if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN) == 0 && gGameSession->location.variant == actionId) {
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN, NEO_ARK_FOREST_ZONE_FIRST_VISIT_SEEN);
            evsStartScript(gNeoArkForestZoneFirstVisitScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
    }
    if (gNeoArkForestZoneRoamerPoolBTask != NULL) {
        return TASK_MESSAGE_DISPATCH_POINTER(gNeoArkForestZoneRoamerPoolBTask, messageId, request, secondArg);
    }
    return NEO_ARK_FOREST_ZONE_POOL_ABSENT;
}

s32 neoArkForestZoneForwardActorEvent(Task* unusedTask, s32 messageId, s32 eventValue, s32 secondArg)
{
    enum { NEO_ARK_FOREST_ZONE_ROAMER_TASK_ABSENT = -1 };

    return gNeoArkForestZoneRoamerPoolBTask == NULL
               ? NEO_ARK_FOREST_ZONE_ROAMER_TASK_ABSENT
               : taskMessageDispatch(gNeoArkForestZoneRoamerPoolBTask, messageId, eventValue, secondArg);
}

void neoArkForestZoneStartRoamerAmbush(void)
{
    if (gNeoArkForestZoneRoamerPoolBTask != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(gNeoArkForestZoneRoamerPoolBTask, ACTOR_COMMAND_MESSAGE_APPLY, &gNeoArkForestZoneStartAmbushCommand, 0);
    }
}

/// Installs the forest receiver, starts ambience and creates the roaming-enemy pool-B controller.
///
/// Variant 1 with the first-visit flag clear hides the placed forest actors
/// before advancing state. The scene and room overlay must be live;
/// the scene borrows the four-byte actor-command payload through dispatch.
static void _neoArkForestZoneInitializeRoom(Task* task)
{
    enum { NEO_ARK_FOREST_ZONE_FIRST_VISIT_VARIANT = 1 };

    task->msgTable = gNeoArkForestZoneRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_FOREST_ZONE_AMBIENCE, 0, 0);
    gNeoArkForestZoneRoamerPoolBTask = taskSpawnFromTable(&gNeoArkForestZoneRoamerPoolBTaskDesc, 0, 0, 0);
    if (gGameSession->location.variant == NEO_ARK_FOREST_ZONE_FIRST_VISIT_VARIANT && gameFlagGetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN) == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &gNeoArkForestZoneFirstVisitPauseCommand, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    task->state = task->state + 1;
}

/// Pauses the forest roaming-enemy pool before the first-visit ambush scene.
///
/// Room-task state 1 sends the pause command only in variant 1 while the
/// first-visit flag is clear, then advances to the room task's idle state.
/// The live pool-B task must already have been created by initialization.
static void _neoArkForestZonePauseRoamersState(Task* task)
{
    enum { NEO_ARK_FOREST_ZONE_FIRST_VISIT_VARIANT = 1 };

    if (gGameSession->location.variant == NEO_ARK_FOREST_ZONE_FIRST_VISIT_VARIANT && gameFlagGetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_EVENT_SEEN) == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gNeoArkForestZoneRoamerPoolBTask, ACTOR_COMMAND_MESSAGE_APPLY, &gNeoArkForestZoneFirstVisitPauseCommand, 0);
    }
    task->state = task->state + 1;
}

/// Holds the room setup task in state 2 with its message handlers installed.
static void _neoArkForestZoneSetupIdleState(Task* task)
{
    // Preserve the 16-byte stack frame despite having no runtime work in this state.
    char stackFrame[0x10];
}

/// Forest room callbacks: 0 register/ambience/pool B, 1 pause first-visit roamers, 2 idle, 3 release.
///
/// Copies all four pointers and indexes `Task::state` in 0..3 without a bounds
/// check or terminator. Room and roamer callbacks must remain loaded through exit.
static const TaskFuncTable4 _gNeoArkForestZoneRoomTaskStates = { {
    _neoArkForestZoneInitializeRoom,
    _neoArkForestZonePauseRoamersState,
    _neoArkForestZoneSetupIdleState,
    taskKill,
} };

void neoArkForestZoneRoomTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = _gNeoArkForestZoneRoomTaskStates;
    handlers.funcs[task->state](task);
}

#include "../../shared/effects/falling_leaves_task.inc.c"

void neoArkForestZoneLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/effects/falling_leaves_draw_neo_ark.inc.c"

void neoArkForestZoneConfigureEffectsTask(Task* task)
{
    enum { NEO_ARK_FOREST_ZONE_EFFECTS_INITIALIZE,
           NEO_ARK_FOREST_ZONE_EFFECTS_ACTIVE };

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    if (task->state == NEO_ARK_FOREST_ZONE_EFFECTS_INITIALIZE) {
        gRoomEffectFlashId      = EFFECT_NEO_ARK_FOREST_ZONE_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_NEO_ARK_FOREST_ZONE_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_NEO_ARK_FOREST_ZONE_SPARK_BURST;
        task->state             = NEO_ARK_FOREST_ZONE_EFFECTS_ACTIVE;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkForestZoneRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkForestZoneRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkForestZoneRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
