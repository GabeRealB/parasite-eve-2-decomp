#include "rooms/shelter_b2_pod_access_tunnel.h"

#include "types.h"

#include "shelter_b2_pod_access_tunnel_private.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/direction.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/display.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_events.h"

extern u8 D_shelter_b2_pod_access_tunnel_80185708;

extern RoomFadeStorage  gRoomEventFade;
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

s32 D_shelter_b2_pod_access_tunnel_801856A0[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionFootstepSounds D_shelter_b2_pod_access_tunnel_801856AC = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856B8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856C0[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856C8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_access_tunnel_801856D0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_pod_access_tunnel_801856AC },
};

WorldCollisionSurfaceProperties* D_shelter_b2_pod_access_tunnel_801856D8[8] = {
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856C0,
    D_shelter_b2_pod_access_tunnel_801856C8,
    D_shelter_b2_pod_access_tunnel_801856D0,
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856B8,
    D_shelter_b2_pod_access_tunnel_801856B8,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 D_shelter_b2_pod_access_tunnel_80185708 = 0;

/// Three bytes stored after the flag; nothing references them.
u8 D_shelter_b2_pod_access_tunnel_80185709 = 83;

u8 D_shelter_b2_pod_access_tunnel_8018570A = 70;

u8 D_shelter_b2_pod_access_tunnel_8018570B = 179;

RoomLatchedEvent gRoomEventLatched = { 0 };

static void _shelterB2PodAccessTunnelInitializeRoomTask(Task* task);

static void _shelterB2PodAccessTunnelIdleRoomTask(Task* unusedTask);

#include "../../shared/room_event_staged_task.inc.c"

/// Latches an eligible departure event and starts its staged task on execution.
///
/// Returns 2 for an eligible event, including a query, or 1 for direct departure
/// when its nonzero flag is already set. Every call clears the latest-start byte.
/// Only `message->queryOnly == ROOM_EVENT_EXECUTE` copies the complete eight-byte
/// message and twelve-byte event, sets a nonzero flag to 1 and raises that byte
/// after spawning. Flag IDs must be 0..503, including the always-eligible zero.
/// Inputs are borrowed for this call. The singleton copies and room CAP/sound
/// resources must stay live and unchanged until the staged task ends.
static __inline__ s32 _shelterB2PodAccessTunnelStartEvent(const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    D_shelter_b2_pod_access_tunnel_80185708 = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *message;
            gRoomEventLatched   = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&D_shelter_b2_pod_access_tunnel_80183BC0, 0, 0, 0);
            D_shelter_b2_pod_access_tunnel_80185708 = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

s32 shelterB2PodAccessTunnelResolveRoomEventMessage(Task* unusedTask, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DEPARTURE_BLOCKED      = 0,
           DEPARTURE_DIRECT       = 1,
           DEPARTURE_HANDLED      = 2,
           REFUSAL_FLAG_VALUE     = 2,
           SEPTIC_PROMPT_PROGRESS = 2,
           LATE_STORY_CHAPTER     = 6,
           CAP_R48_LOCKED_EARLY   = 2,
           CAP_R48_LOCKED_LATE    = 6,
           CAP_SEPTIC_PROMPT      = 4,
           CAP_SEPTIC_DEPARTURE   = 5,
           EVENT_NO_FADE          = 0,
           SOUND_SEPTIC_DEPARTURE = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 1) };

    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_R48) {
        if (gameFlagGetNibble(GAME_FLAG_B2_POD_TUNNEL_R48_DOOR_UNLOCKED) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, REFUSAL_FLAG_VALUE);
                capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < LATE_STORY_CHAPTER ? CAP_R48_LOCKED_EARLY : CAP_R48_LOCKED_LATE);
            }
            return DEPARTURE_BLOCKED;
        }
    }
    if (request->areaId == GAME_AREA_SHELTER_B2_SEPTIC_TANK) {
        if (gameFlagGetNibble(GAME_FLAG_118) == SEPTIC_PROMPT_PROGRESS) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(CAP_SEPTIC_PROMPT);
            }
            return DEPARTURE_HANDLED;
        }
        event.capCmd   = CAP_SEPTIC_DEPARTURE;
        event.stageSnd = SOUND_SEPTIC_DEPARTURE;
        event.flagId   = GAME_FLAG_B2_POD_TUNNEL_TO_SEPTIC_SCENE;
        event.fade     = EVENT_NO_FADE;
        return _shelterB2PodAccessTunnelStartEvent(reply, &event);
    }
    return DEPARTURE_DIRECT;
}

/// The three states `shelterB2PodAccessTunnelRoomTask` dispatches
/// the room task through: set-up, an idle tick, and removal.
static const TaskFuncTable3 D_shelter_b2_pod_access_tunnel_8017D5D8 = {
    { _shelterB2PodAccessTunnelInitializeRoomTask, _shelterB2PodAccessTunnelIdleRoomTask, taskKill },
};

/// Commits the B1 pod arrival and requests a captured-frame session reload.
///
/// Requires a live ride task with actor/player control held. Commits area,
/// arrival and room even if reload allocation fails; kills the ride immediately.
static inline void _shelterB2PodAccessTunnelCommitB1Ride(Task* task)
{
    enum { RIDE_ARRIVAL_WARP                = 3,
           RIDE_ARRIVAL_ROOM                = 1,
           RIDE_NEXT_SESSION_SPRITE_VARIANT = 1 };

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = RIDE_ARRIVAL_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = RIDE_ARRIVAL_ROOM;
    gDisplayState.spriteVariant                                = RIDE_NEXT_SESSION_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

void shelterB2PodAccessTunnelRideToB1Task(Task* task)
{
    enum { RIDE_PROMPT,
           RIDE_WAIT_CAP,
           RIDE_INTERPRET_CHOICE,
           RIDE_WAIT_SOUND,
           RIDE_DEPART,
           RIDE_CAP_PROMPT           = 1,
           RIDE_CAP_ALTERNATE_PROMPT = 3,
           RIDE_CHOICE_ACCEPT        = 10,
           RIDE_CHOICE_MARK_POD      = 1,
           RIDE_MAP_MARK_VISIBLE     = 2,
           RIDE_MAP_MARK_HIDDEN      = 0 };

    switch (task->state) {
        case RIDE_PROMPT:
            capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_0FC) != 0 ? RIDE_CAP_ALTERNATE_PROMPT : RIDE_CAP_PROMPT);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state++;
            return;
        case RIDE_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            return;
        case RIDE_INTERPRET_CHOICE:
            if (capGetVariantKey() != RIDE_CHOICE_ACCEPT) {
                if (capGetVariantKey() == RIDE_CHOICE_MARK_POD) {
                    gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, RIDE_MAP_MARK_VISIBLE);
                }
                // Resume actors before releasing this task, then return player control.
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                return;
            }
            sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_TUNNEL_RIDE_TO_B1, 0, 0);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, RIDE_MAP_MARK_HIDDEN);
            task->state++;
            return;
        case RIDE_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B2_POD_TUNNEL_RIDE_TO_B1) == 0) {
                task->state++;
            }
            return;
        case RIDE_DEPART:
            // The reload takes over while actor and player control remain held.
            _shelterB2PodAccessTunnelCommitB1Ride(task);
            break;
    }
}

s32 shelterB2PodAccessTunnelRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 shelterB2PodAccessTunnelHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum { COMMAND_RIDE_TO_B1 = 1 };

    if (commandId == COMMAND_RIDE_TO_B1) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(&D_shelter_b2_pod_access_tunnel_80183BFC, 0, 0, 0);
    }
    return 0;
}

s32 shelterB2PodAccessTunnelIgnoreActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* unusedRequest, s32 unusedArg)
{
    return 0;
}

s32 shelterB2PodAccessTunnelHandleSoundMessage(Task* unusedTask, s32 messageId, s32 cueKey, s32 unusedArg)
{
    enum { SHELTER_B2_POD_ACCESS_TUNNEL_SOUND_CUE_CONFIRM = 4 };

    if (cueKey == SHELTER_B2_POD_ACCESS_TUNNEL_SOUND_CUE_CONFIRM) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
    }
    return 0;
}

/// Publishes the room's message task and suppresses automatic music in variant 22.
///
/// Runs in state 0 and advances to idle state 1. Variant 22 replaces the session
/// flow flags with the ending/area music skip bits. The overlay's message table
/// must stay live while this task is registered in `GAME_TASK_SLOT_ROOM`.
static void _shelterB2PodAccessTunnelInitializeRoomTask(Task* task)
{
    enum { ROOM_VARIANT_SKIP_MUSIC = 22 };

    task->msgTable = D_shelter_b2_pod_access_tunnel_80183BCC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == ROOM_VARIANT_SKIP_MUSIC) {
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
    }
    task->state++;
}

/// Keeps the initialized room task live to receive messages without per-frame work.
static void _shelterB2PodAccessTunnelIdleRoomTask(Task* unusedTask)
{
}

void shelterB2PodAccessTunnelRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_shelter_b2_pod_access_tunnel_8017D5D8;
    handlers.funcs[task->state](task);
}
