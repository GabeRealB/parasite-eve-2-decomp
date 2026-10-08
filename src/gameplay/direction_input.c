#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area_entry.h"
#include "area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/direction.h"
#include "direction.h"
#include "hud_sprites.h"
#include "loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/display.h"
#include "main/gameflow.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/* Define BSS before API headers to preserve first-declaration order. */
s16 D_80114CD0;

u16 Gp_DirFlags;

u16 D_80114CD4;

u16 Gp_DirPhase;

u8 Gp_DirByte;

u8 Gp_DirNibble;

u8 Gp_DirAlt;

u8 Gp_DirAltNibble;

u8 D_80114CDC;

u8 D_80114CDD;

u8 D_80114CDE;

s16 D_80114CE0;

RoomEventMsg Gp_WarpLoc;

s32 D_80114CF0;

s16 D_80114CF4;

u16 Gp_DirFadeLevel;

u8 D_80114CF8;

s32 Gp_AreaIdBits[2];

s16 D_80114D08;

#include "direction_input.h"

#include "gameplay/direction_input.h"

/// Outcomes recognized by the warp query phase after narrowing the room reply to s16.
enum {
    DIRECTION_WARP_QUERY_STAY        = 0, // Execute in this area, then release scripted control
    DIRECTION_WARP_QUERY_DEPART      = 1, // Turn, execute, await sound and save the destination
    DIRECTION_WARP_QUERY_ROOM_ACTION = 2  // Execute the room's replacement action immediately
};

/// Trigger byte packing and the fixed facing target used by Dryfield's driveway exits.
enum {
    DIRECTION_WARP_ENDPOINT_SHIFT       = 4,
    DIRECTION_WARP_ARRIVAL_MASK         = 0xF,
    DIRECTION_WARP_DEPARTURE_FADE_START = 30,
    DIRECTION_WARP_DRIVEWAY_FACING_X    = -1473,
    DIRECTION_WARP_DRIVEWAY_FACING_Z    = 2497
};

/// Five stage counts, followed by three unexplained nonzero bytes.
/// The tail is retained for review, not interpreted as additional stages.
s8 Gp_AreaIdCounts[8] = {
    17,
    38,
    38,
    49,
    33,
    -31,
    -1,
    34,
};

u8 gViewIdentityMap[VIEW_IDENTITY_MAP_LENGTH] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
};

void directionUpdateAction(void)
{
    enum { DIRECTION_INTERACTION_REARM_UPDATES = 10 };
    const Task*          viewGateTask;
    const PlayerStatus*  playerStatus;
    u32                  triggerControl;
    u32                  actionIndex;
    u32                  automaticFlag;
    DirectionActionTable actions;

    actions      = Gp_DirActionFns;
    playerStatus = &gPlayerStatus;
    viewGateTask = gameGetTaskSlot(GAME_TASK_SLOT_VIEW_GATE);
    // View changes rearm manual interaction before this eligible update is counted.
    if (viewGateTask != NULL) {
        if (viewGateTask->spawnArg1.value != gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view) {
            viewChangeStub();
            D_80114D08 = DIRECTION_INTERACTION_REARM_UPDATES;
        }
    }
    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        D_80114D08 = DIRECTION_INTERACTION_REARM_UPDATES;
    }
    if (D_80114CF8 == 0) {
        if (Gp_StateC08.mode == ATTACHMENT_MODE_IDLE) {
            gGameSession->dirActionBusy = 0;
            if (D_80114D08 != 0) {
                D_80114D08 = (u16)D_80114D08 - 1;
            }
            if (worldCollisionReadActionHit(&Gp_DirFlags, &Gp_DirByte, &Gp_DirNibble) != 0) {
                if (D_80114CD0 != (s16)Gp_DirFlags) {
                    D_80114CDC = 1;
                } else {
                    D_80114CDC = 0;
                }
                Gp_DirPhase    = 0;
                triggerControl = Gp_DirFlags;
                automaticFlag  = triggerControl & WORLD_COLLISION_TRIGGER_AUTOMATIC;
                if (gSceneCombatState.signals.bytes.endDelayFrames == 0) {
                    if (automaticFlag && (gDisplayState.pendingMode == DISPLAY_MODE_NONE) && !(gGameSession->padPressed & PAD_BUTTON_TRIANGLE)) {
                        if (!(triggerControl & WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE)) {
                            D_80114CF8 = 1;
                        } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                            D_80114CF8 = 1;
                        }
                    } else if (playerStatus->interactionPressed != 0) {
                        if (!(gGameSession->padPressed & PAD_BUTTON_TRIANGLE)) {
                            if (!(Gp_DirFlags & WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE)) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = DIRECTION_INTERACTION_REARM_UPDATES;
                                }
                            } else if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
                                if (D_80114D08 == 0) {
                                    D_80114CF8 = 1;
                                    D_80114D08 = DIRECTION_INTERACTION_REARM_UPDATES;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Once latched, actions advance regardless of the gates for new requests.
    D_80114CD0 = (s16)Gp_DirFlags;
    if (D_80114CF8 != 0) {
        gGameSession->dirActionBusy = 1;
        actionIndex                 = (u8)Gp_DirFlags;
        if (actionIndex != WORLD_COLLISION_TRIGGER_ACTION_CANCEL) {
            actions.handlers[actionIndex]();
        } else {
            _directionClearTriggerParameters();
            D_80114CF8 = 0;
        }
    } else {
        _directionClearTriggerParameters();
    }
    D_80114CDE = gSceneCombatState.signals.bytes.battlePhase;
}

void directionQueryWarp(void)
{
    Task*                  roomTask;
    Task*                  playerTask;
    PlayerStatus*          playerStatus;
    const GameActor*       playerActor;
    const GameLocationKey* location;
    DirectionWarpEntry     warpEntry;
    ActorTransform         turnRequest;
    SVECTOR                departureFacingPoint;
    SVECTOR                stayFacingPoint;
    s32                    stageId;
    s32                    areaId;
    s16                    queryResult;

    /// Prepares a rotation-only request, reversing arrival yaw or resolving its sentinel.
    ///
    /// Arguments must be side-effect-free lvalues or a live player pointer: each
    /// is evaluated repeatedly. Borrows the registered player task for world-point
    /// bearing. Caller-owned point storage keeps each branch's distinct temporary.
#define DIRECTION_PREPARE_WARP_TURN(request, endpoint, playerActorPtr, point)                                           \
    {                                                                                                                   \
        (request).rot.vx = 0;                                                                                           \
        (request).rot.vz = 0;                                                                                           \
        (request).rot.vy = ((endpoint).player.yaw.word + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK; \
        if ((endpoint).player.yaw.word == ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT ||                                  \
            (endpoint).player.yaw.word == ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT) {                                      \
            (point).vx       = DIRECTION_WARP_DRIVEWAY_FACING_X;                                                        \
            (point).vy       = 0;                                                                                       \
            (point).vz       = DIRECTION_WARP_DRIVEWAY_FACING_Z;                                                        \
            (request).rot.vy = actorAngleTaskYawTowardPoint(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], &(point));     \
        } else if ((endpoint).player.yaw.word == ACTOR_SPAWN_YAW_KEEP_FACING) {                                         \
            (request).rot.vy = (playerActorPtr)->rotation.vy;                                                           \
        }                                                                                                               \
    }

    location     = &gGameSession->location.loc;
    stageId      = location->stage;
    areaId       = location->area;
    roomTask     = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);
    playerTask   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;
    playerActor  = playerTask->work;

    if (gGameSession->eventState != 0) {
        D_80114CF8 = 0;
        _directionClearTriggerParameters();
        return;
    }

    D_80114CF4      = 0;
    Gp_DirFadeLevel = 0;
    // The current endpoint supplies facing and sounds for this departure.
    warpEntry = Gp_WarpTables[stageId - 1][areaId - 1][(Gp_DirNibble >> DIRECTION_WARP_ENDPOINT_SHIFT) - 1];

    Gp_WarpLoc.field_4   = 1;
    Gp_WarpLoc.room      = 1;
    Gp_WarpLoc.queryOnly = ROOM_EVENT_QUERY_ONLY;
    Gp_WarpLoc.areaId    = Gp_DirByte;
    Gp_WarpLoc.warp      = Gp_DirNibble & DIRECTION_WARP_ARRIVAL_MASK;
    Gp_WarpLoc.flagId    = warpEntry.mapFlagId;

    // The room may resolve the reusable request in place while answering the query.
    queryResult = TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, &Gp_WarpLoc, &Gp_WarpLoc);
    D_80114CF4  = queryResult;

    switch (queryResult) {
        case DIRECTION_WARP_QUERY_DEPART:
            if (warpEntry.departureSound != DIRECTION_WARP_SOUND_NONE) {
                D_80114CF0 = warpEntry.departureSound;
            } else {
                D_80114CF0 = 0;
            }
            DIRECTION_PREPARE_WARP_TURN(turnRequest, warpEntry, playerActor, departureFacingPoint);
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &turnRequest, 0);
            if (warpEntry.flags & DIRECTION_WARP_FLAG_FADE_DEPARTURE) {
                Gp_DirFadeLevel = DIRECTION_WARP_DEPARTURE_FADE_START;
            }
            Gp_DirPhase++;
            break;

        case DIRECTION_WARP_QUERY_STAY:
            if (warpEntry.blockedSound != DIRECTION_WARP_SOUND_NONE) {
                D_80114CF0 = warpEntry.blockedSound;
            } else {
                D_80114CF0 = 0;
            }
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                Gp_WarpLoc.field_4   = gSceneCombatState.signals.bytes.battlePhase;
                Gp_WarpLoc.room      = gSceneCombatState.signals.bytes.battlePhase;
                Gp_WarpLoc.queryOnly = ROOM_EVENT_EXECUTE;
                Gp_WarpLoc.areaId    = Gp_DirByte;
                Gp_WarpLoc.warp      = Gp_DirNibble & DIRECTION_WARP_ARRIVAL_MASK;
                Gp_WarpLoc.flagId    = warpEntry.mapFlagId;
                TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, &Gp_WarpLoc, &Gp_WarpLoc);
                D_80114CF8                       = 0;
                Gp_DirNibble                     = 0;
                Gp_DirByte                       = 0;
                Gp_DirFlags                      = 0;
                playerStatus->interactionPressed = 0;
                if (D_80114CF0 != DIRECTION_WARP_SOUND_NONE && playerStatus->hp > 0) {
                    sndEvtRequestScriptStart(D_80114CF0, 0, 0);
                }
                return;
            }
            DIRECTION_PREPARE_WARP_TURN(turnRequest, warpEntry, playerActor, stayFacingPoint);
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &turnRequest, 0);
            Gp_DirPhase++;
            break;

        case DIRECTION_WARP_QUERY_ROOM_ACTION:
            Gp_WarpLoc.field_4   = 1;
            Gp_WarpLoc.room      = 1;
            Gp_WarpLoc.queryOnly = ROOM_EVENT_EXECUTE;
            Gp_WarpLoc.areaId    = Gp_DirByte;
            Gp_WarpLoc.warp      = Gp_DirNibble & DIRECTION_WARP_ARRIVAL_MASK;
            Gp_WarpLoc.flagId    = warpEntry.mapFlagId;
            TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, &Gp_WarpLoc, &Gp_WarpLoc);
            D_80114CF8                       = 0;
            Gp_DirNibble                     = 0;
            Gp_DirByte                       = 0;
            Gp_DirFlags                      = 0;
            playerStatus->interactionPressed = 0;
            break;
    }
#undef DIRECTION_PREPARE_WARP_TURN
}

void directionAwaitWarpTurn(void)
{
    Task* playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    _directionStepDepartureFade();
    if (taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        if (D_80114CF4 != 0) {
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        }
        Gp_DirPhase++;
    }
}

void directionResolveWarp(void)
{
    Task*                  playerTask;
    Task*                  roomTask;
    PlayerStatus*          playerStatus;
    const GameLocationKey* location;
    DirectionWarpEntry     warpEntry;
    RoomEventMsg*          request;

    playerTask   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;
    roomTask     = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);

    location  = &gGameSession->location.loc;
    warpEntry = Gp_WarpTables[location->stage - 1][location->area - 1][(Gp_DirNibble >> DIRECTION_WARP_ENDPOINT_SHIFT) - 1];

    _directionStepDepartureFade();

    // Rebuild the request from the trigger; query-time selector edits do not survive.
    request            = &Gp_WarpLoc;
    request->field_4   = 1;
    request->room      = 1;
    request->queryOnly = ROOM_EVENT_EXECUTE;
    Gp_WarpLoc.areaId  = Gp_DirByte;
    request->warp      = Gp_DirNibble & DIRECTION_WARP_ARRIVAL_MASK;
    request->flagId    = warpEntry.mapFlagId;
    TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, request, request);

    if (D_80114CF0 != DIRECTION_WARP_SOUND_NONE) {
        if (playerStatus->hp > 0) {
            sndEvtRequestScriptStart(D_80114CF0, 0, 0);
        }
    }

    if (D_80114CF4 == DIRECTION_WARP_QUERY_STAY) {
        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        D_80114CF8                       = 0;
        Gp_DirNibble                     = 0;
        Gp_DirByte                       = 0;
        Gp_DirFlags                      = 0;
        playerStatus->interactionPressed = 0;
    } else {
        Gp_DirPhase++;
    }
}

void directionAwaitWarpSound(void)
{
    _directionStepDepartureFade();
    if (D_80114CF0 == 0 || sndScriptHasActiveId(D_80114CF0) == 0) {
        Gp_DirPhase++;
    }
}
