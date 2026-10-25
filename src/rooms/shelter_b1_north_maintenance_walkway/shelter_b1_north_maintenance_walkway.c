#include "rooms/shelter_b1_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "shelter_b1_north_maintenance_walkway_private.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"

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
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_events.h"

static void _shelterB1NorthMaintenanceWalkwaySetSceneSpriteVisibility(u8 sceneSeen);

static s32  _shelterB1NorthMaintenanceWalkwayResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static void _shelterB1NorthMaintenanceWalkwayPostBattleSceneTask(Task* task);
static void _shelterB1NorthMaintenanceWalkwayInitRoomTask(Task* task);
static s32  _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Bodyless priority-32 recipe for this room's staged departure task.
///
/// The gate reads index 0 synchronously with zero metadata and unused payloads.
/// The deferred task borrows the latched event and destination through CAP,
/// optional sound/fade and reload; retain them and the loaded room throughout.
static TaskDesc _gShelterB1NorthMaintenanceWalkwayStagedEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Room-message routes: stage eligible departures, refuse key items, ignore actions and commands.
///
/// 4 live entries followed by TASK_MESSAGE_TABLE_END/NULL. The room controller
/// borrows this table; message-specific payloads are consumed synchronously.
/// Keep the room overlay and callback code loaded while the receiver is live.
static TaskMessageEntry _gShelterB1NorthMaintenanceWalkwayRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1NorthMaintenanceWalkwayResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless post-battle scene watcher spawned on entry to placement variant 2.
///
/// A single priority-32 descriptor, spawned at index 0 with zero payloads.
/// Watches battle engagement, actor completion and the display handoff before
/// requesting CAP command 1. Borrows this room's callback and scene resources.
static TaskDesc _gShelterB1NorthMaintenanceWalkwayPostBattleSceneTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB1NorthMaintenanceWalkwayPostBattleSceneTask, { .value = 0 } };

/// World-space lamp endpoint pairs followed by the red disc centre.
///
/// The first 18 signed world-coordinate points form consecutive lamp
/// pairs; point 18 is the red disc centre. Interior view bases keep
/// every endpoint and centre within this one table. Drawers borrow positions
/// only during the current frame; the room overlay owns the storage.
static SVECTOR _gShelterB1NorthMaintenanceWalkwayGlowPoints[19] = {
    { 894, -197, -2271, 0 },
    { 894, -197, -3102, 0 },
    { 894, -197, 376, 0 },
    { 894, -197, -396, 0 },
    { 894, -197, 2648, 0 },
    { 894, -197, 2036, 0 },
    { 3106, -197, -2271, 0 },
    { 3106, -197, -3102, 0 },
    { 3106, -197, 376, 0 },
    { 3106, -197, -396, 0 },
    { 3106, -197, 2648, 0 },
    { 3106, -197, 2036, 0 },
    { 653, -197, 2896, 0 },
    { -31, -197, 2896, 0 },
    { -42, -197, 5113, 0 },
    { 597, -197, 5113, 0 },
    { -1562, -197, 5113, 0 },
    { -2493, -197, 5113, 0 },
    { 749, -1283, 2310, 0 },
};

static void _shelterB1NorthMaintenanceWalkwayRoomIdle(Task* task);

/// Binds the shared enemy-effect selectors to this room's implementations.
///
/// Stores packed bank-6 task IDs for later `effectSpawn` calls. Call after
/// room-effect initialization clears the selectors and before enemies use them.
/// This overlay must remain loaded while the selected IDs are used and while
/// their spawned tasks are live.
static inline void _shelterB1NorthMaintenanceWalkwayBindEffectTasks(void)
{
    gRoomEffectMoteId         = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_MOTE;
    gRoomEffectHaloId         = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_HALO;
    gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST;
    gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPARK_EMITTER;
    gRoomEffectFlashId        = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_FLASH;
    gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_TWIN_TRAIL;
    gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SPARK_BURST;
    gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_GLOW_DISC;
    gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_FLYING_SPARK;
    gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_ORANGE_BURST_2;
}

/// Latches an eligible departure event and starts its staged task on execution.
///
/// Returns 2 when the room handles the departure, including an eligible query;
/// returns 1 for ordinary departure when a nonzero event flag is already set.
/// Every call clears the latest-start byte. Only `ROOM_EVENT_EXECUTE` copies
/// both records, writes 1 to a nonzero flag and raises that byte after spawning.
/// Borrows complete records for this call; flag IDs must be 0..503. The room's
/// singleton copies and CAP/sound resources must remain live until its task ends;
/// do not latch another event while that task still uses them.
static inline s32 _shelterB1NorthMaintenanceWalkwayStartEvent(const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    gShelterB1NorthMaintenanceWalkwayStagedEventStarted = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            ROOM_EVENT_STAGED_DESTINATION = *message;
            gRoomEventLatched             = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&_gShelterB1NorthMaintenanceWalkwayStagedEventTaskDesc, 0, 0, 0);
            gShelterB1NorthMaintenanceWalkwayStagedEventStarted = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

#include "../../shared/room/room_event_staged_task.inc.c"

/// Resolves walkway departures and starts one-shot storeroom or quarters scenes.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with complete borrowed request/reply
/// records, which may alias. Copies the request before resolving the reply's
/// room. Returns 2 for an eligible departure scene, including queries, and 1
/// for ordinary travel or a scene already seen. Execution latches the resolved
/// reply and event and marks its flag before requesting the staged task; spawn
/// failure does not undo the flag. Queries suppress these changes. Requires
/// the Shelter map overlay and this room's CAP/sound resources. Do not replace
/// the singleton snapshots while a staged departure still uses them.
static s32 _shelterB1NorthMaintenanceWalkwayResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { CAP_COMMAND_DEPART_STOREROOM = 3,
           CAP_COMMAND_DEPART_QUARTERS  = 2,
           FLAG_DEPART_STOREROOM        = 0x14D,
           FLAG_DEPART_QUARTERS         = 0x14E,
           TRANSITION_DIRECT            = 1 };
    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_STOREROOM) {
        event.capCmd   = CAP_COMMAND_DEPART_STOREROOM;
        event.stageSnd = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 1);
        event.flagId   = FLAG_DEPART_STOREROOM;
        event.fade     = 0;
        return _shelterB1NorthMaintenanceWalkwayStartEvent(reply, &event);
    }
    if (request->areaId == GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS) {
        event.capCmd   = CAP_COMMAND_DEPART_QUARTERS;
        event.stageSnd = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 3);
        event.flagId   = FLAG_DEPART_QUARTERS;
        event.fade     = 0;
        return _shelterB1NorthMaintenanceWalkwayStartEvent(reply, &event);
    }
    return TRANSITION_DIRECT;
}

/// Waits for combat to finish, then requests the walkway's post-battle CAP scene.
///
/// Starts in state 0. Battle engagement requests a hidden weapon re-equip;
/// zero battle references starts the end delay and a 62-tick countdown. Once
/// no display mode is pending, requests CAP command 1 and kills this task,
/// even if CAP is busy and the request is ignored. Requires live room resources.
static void _shelterB1NorthMaintenanceWalkwayPostBattleSceneTask(Task* task)
{
    enum { POST_BATTLE_WAIT_ENGAGEMENT,
           POST_BATTLE_WAIT_ACTORS,
           POST_BATTLE_WAIT_PRESENTATION,
           POST_BATTLE_DELAY_TICKS = 62,
           CAP_COMMAND_POST_BATTLE = 1 };
    // The unused vector retains the original 32-byte stack frame.
    SVECTOR unusedVector;

    switch (task->state) {
        case POST_BATTLE_WAIT_ENGAGEMENT:
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
                gGameSession->flowFlags |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
                gGameSession->flowFlags |= GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON;
                task->state++;
            }
            break;
        case POST_BATTLE_WAIT_ACTORS:
            if (gSceneCombatState.battleRefs == 0) {
                gSceneCombatState.signals.bytes.endDelayFrames = SCENE_COMBAT_END_DELAY_FRAMES;
                task->killCountdown                            = POST_BATTLE_DELAY_TICKS;
                task->state++;
            }
            break;
        case POST_BATTLE_WAIT_PRESENTATION:
            // The scene request waits for both the combat delay and presentation handoff.
            if (task->killCountdown == 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    capSpawnEventIfIdle(CAP_COMMAND_POST_BATTLE, CAP_EVENT_NO_FLAGS);
                    taskKill(task);
                }
            } else {
                task->killCountdown--;
            }
            break;
    }
}

/// Rejects key-item use in this room, returning 0 without changing the item or room.
static s32 _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores room commands and both payload words, returning 0.
static s32 _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores room actions without reading the borrowed request, returning 0.
static s32 _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room receiver, starts variant 2's scenes and restores scene sprites.
///
/// Publishes the live task in `GAME_TASK_SLOT_ROOM` and borrows its message table.
/// Variant 2 always starts the post-battle watcher; its entry CAP is requested
/// once, with the flag set even if CAP is busy. Advances to the idle state.
static void _shelterB1NorthMaintenanceWalkwayInitRoomTask(Task* task)
{
    enum { SCENE_ROOM_VARIANT     = 2,
           SCENE_NOT_STARTED      = 0,
           SCENE_STARTED          = 1,
           CAP_COMMAND_ROOM_ENTRY = 4 };

    task->msgTable = _gShelterB1NorthMaintenanceWalkwayRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.variant == SCENE_ROOM_VARIANT) {
        taskSpawnFromTable(&_gShelterB1NorthMaintenanceWalkwayPostBattleSceneTaskDesc, 0, 0, 0);
        if (gameFlagGetNibble(GAME_FLAG_NORTH_MAINTENANCE_WALKWAY_SCENE) == SCENE_NOT_STARTED) {
            gameFlagSetNibble(GAME_FLAG_NORTH_MAINTENANCE_WALKWAY_SCENE, SCENE_STARTED);
            capSpawnEventIfIdle(CAP_COMMAND_ROOM_ENTRY, CAP_EVENT_NO_FLAGS);
        }
    }
    _shelterB1NorthMaintenanceWalkwaySetSceneSpriteVisibility(gameFlagGetNibble(GAME_FLAG_B2_NORTH_WALKWAY_SCENE_SEEN));
    task->state++;
}

/// Keeps the initialized room task idle until another owner changes its state.
static void _shelterB1NorthMaintenanceWalkwayRoomIdle(Task* task)
{
}

/// Room-controller states 0..2: initialize the receiver and variant-2 scenes, idle, teardown.
///
/// The controller copies all three callbacks before unchecked state dispatch.
/// No terminator; keep the room overlay loaded while the controller is live.
static const TaskFuncTable3 _gShelterB1NorthMaintenanceWalkwayStateHandlers = {
    { _shelterB1NorthMaintenanceWalkwayInitRoomTask, _shelterB1NorthMaintenanceWalkwayRoomIdle, taskKill },
};

void shelterB1NorthMaintenanceWalkwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterB1NorthMaintenanceWalkwayStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Restores view 3's scene sprite visibility from the B2 north walkway scene flag.
///
/// Requires this room's loaded sprite directory. Flag 0 hides the single-sprite
/// batch, 1 reveals it, and all other byte values preserve its current visibility.
static void _shelterB1NorthMaintenanceWalkwaySetSceneSpriteVisibility(u8 sceneSeen)
{
    enum { SCENE_NOT_SEEN    = 0,
           SCENE_SEEN        = 1,
           SCENE_VIEW_INDEX  = 2,
           SCENE_BATCH_INDEX = 2 };
    const GameLocationKey* location = &gGameSession->location;
    SpriteView*            views;
    SpriteBatch*           batches;

    views = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    if (sceneSeen == SCENE_NOT_SEEN) {
        batches                           = views[SCENE_VIEW_INDEX].batches;
        batches[SCENE_BATCH_INDEX].hidden = 1;
    } else if (sceneSeen == SCENE_SEEN) {
        batches                           = views[SCENE_VIEW_INDEX].batches;
        batches[SCENE_BATCH_INDEX].hidden = 0;
    }
}

void shelterB1NorthMaintenanceWalkwayDrawGlowsTask(Task* task)
{
    enum { GLOW_TASK_INITIALIZE   = 0,
           GLOW_TASK_DRAW         = 1,
           LAMP_GLOW_RADIUS_SCALE = 0x200 };

    if (task->state == GLOW_TASK_INITIALIZE) {
        _shelterB1NorthMaintenanceWalkwayBindEffectTasks();
        task->state = GLOW_TASK_DRAW;
    }

    // Each view selects an interior base in the complete world-point table.
    switch (gGameSession->location.view) {
        case 2: {
            const SVECTOR* glowPoints;
            glowPoints = &_gShelterB1NorthMaintenanceWalkwayGlowPoints[12];
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 3: {
            const SVECTOR* glowPoints;
            glowPoints = &_gShelterB1NorthMaintenanceWalkwayGlowPoints[10];
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[2], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 4:
        case 6: {
            const SVECTOR* glowPoints;
            glowPoints = &_gShelterB1NorthMaintenanceWalkwayGlowPoints[18];
            glowDrawRedDisc(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE);
            glowDrawDimGreyCapsule(&glowPoints[-18], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-16], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-14], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[-12], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-10], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-8], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            glowDrawDimGreyCapsule(&glowPoints[-6], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            break;
        }
        case 5: {
            const SVECTOR* glowPoints;
            glowPoints = _gShelterB1NorthMaintenanceWalkwayGlowPoints;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[6], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
    }
}
