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
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"

extern s8 D_shelter_b1_north_maintenance_walkway_80185B7C;

extern TaskMessageEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc         D_shelter_b1_north_maintenance_walkway_80184AAC[];

extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184AB8[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B08[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B18[];
extern SVECTOR D_shelter_b1_north_maintenance_walkway_80184B48[];

static void _shelterB1NorthMaintenanceWalkwaySetSceneSpriteVisibility(u8 sceneSeen);

extern TaskDesc D_shelter_b1_north_maintenance_walkway_80184A78;

static s32  _shelterB1NorthMaintenanceWalkwayResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static void _shelterB1NorthMaintenanceWalkwayPostBattleSceneTask(Task* task);
static void _shelterB1NorthMaintenanceWalkwayInitRoomTask(Task* task);
static s32  _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);
static s32  _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Requests use of the key item in the first payload word.
enum { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskDesc D_shelter_b1_north_maintenance_walkway_80184A78 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b1_north_maintenance_walkway_80184A84[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1NorthMaintenanceWalkwayResolveRoomTransition },
    { SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_MESSAGE_USE_KEY_ITEM, _shelterB1NorthMaintenanceWalkwayRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1NorthMaintenanceWalkwayIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1NorthMaintenanceWalkwayIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_north_maintenance_walkway_80184AAC[1] = {
    { { { TASK_BODY_NONE, 32 } }, _shelterB1NorthMaintenanceWalkwayPostBattleSceneTask, { .value = 0 } },
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184AB8[10] = {
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
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184B08[2] = {
    { 3106, -197, 2648, 0 },
    { 3106, -197, 2036, 0 },
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184B18[6] = {
    { 653, -197, 2896, 0 },
    { -31, -197, 2896, 0 },
    { -42, -197, 5113, 0 },
    { 597, -197, 5113, 0 },
    { -1562, -197, 5113, 0 },
    { -2493, -197, 5113, 0 },
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184B48[1] = {
    { 749, -1283, 2310, 0 },
};

static void _shelterB1NorthMaintenanceWalkwayRoomIdle(Task* task);

/// Binds the shared enemy-effect selectors to this room's implementations.
///
/// Stores packed bank-6 task IDs for later `effectSpawn` calls. Call after
/// room-effect initialization clears the selectors and before enemies use them.
/// This overlay must remain loaded while the selected IDs are used and while
/// their spawned tasks are live.
static __inline__ void _shelterB1NorthMaintenanceWalkwayBindEffectTasks(void)
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
static __inline__ s32 _shelterB1NorthMaintenanceWalkwayStartEvent(const RoomEventMsg* message, const RoomLatchedEvent* event)
{
    enum { ROOM_EVENT_FLAG_NONE         = 0,
           ROOM_EVENT_FLAG_CLEAR        = 0,
           ROOM_EVENT_FLAG_LATCHED      = 1,
           ROOM_EVENT_DEPARTURE_DIRECT  = 1,
           ROOM_EVENT_DEPARTURE_HANDLED = 2 };

    D_shelter_b1_north_maintenance_walkway_80185B7C = false;
    if (gameFlagGetNibble(event->flagId) == ROOM_EVENT_FLAG_CLEAR || event->flagId == ROOM_EVENT_FLAG_NONE) {
        if (message->queryOnly == ROOM_EVENT_EXECUTE) {
            gRoomEventStagedMsg = *message;
            gRoomEventLatched   = *event;
            if (event->flagId != ROOM_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, ROOM_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&D_shelter_b1_north_maintenance_walkway_80184A78, 0, 0, 0);
            D_shelter_b1_north_maintenance_walkway_80185B7C = true;
        }
        return ROOM_EVENT_DEPARTURE_HANDLED;
    }
    return ROOM_EVENT_DEPARTURE_DIRECT;
}

#include "../../shared/room_event_staged_task.inc.c"

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
    s32              capCommand;
    s32              transitSound;
    s16              sceneFlag;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId != GAME_AREA_SHELTER_B1_STOREROOM) {
        goto sleepingQuarters;
    }
    transitSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 1);
    capCommand     = CAP_COMMAND_DEPART_STOREROOM;
    event.stageSnd = transitSound;
    sceneFlag      = FLAG_DEPART_STOREROOM;
startEvent:
    event.capCmd = capCommand;
    event.flagId = sceneFlag;
    event.fade   = 0;
    return _shelterB1NorthMaintenanceWalkwayStartEvent(reply, &event);
sleepingQuarters:
    if (request->areaId == GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS) {
        transitSound   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY, 3);
        capCommand     = CAP_COMMAND_DEPART_QUARTERS;
        event.stageSnd = transitSound;
        sceneFlag      = FLAG_DEPART_QUARTERS;
        goto startEvent;
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

    task->msgTable = D_shelter_b1_north_maintenance_walkway_80184A84;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == SCENE_ROOM_VARIANT) {
        taskSpawnFromTable(D_shelter_b1_north_maintenance_walkway_80184AAC, 0, 0, 0);
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

/// The room task's three states: set-up, idle and exit.
static const TaskFuncTable3 D_shelter_b1_north_maintenance_walkway_8017D5D8 = {
    { _shelterB1NorthMaintenanceWalkwayInitRoomTask, _shelterB1NorthMaintenanceWalkwayRoomIdle, taskKill },
};

void shelterB1NorthMaintenanceWalkwayRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b1_north_maintenance_walkway_8017D5D8;
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
    const GameLocationKey* location = &gGameSession->location.loc;
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

    // The point arrays' boundaries are unresolved; views 3, 4 and 6 cross them.
    switch (gGameSession->location.loc.view) {
        case 2: {
            const SVECTOR* glowPoints;
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184B18;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 3: {
            const SVECTOR* glowPoints;
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184B08;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[2], LAMP_GLOW_RADIUS_SCALE, GLOW_HALF_TURN);
            glowDrawDimGreyCapsule(&glowPoints[4], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
        case 4:
        case 6: {
            const SVECTOR* glowPoints;
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184B48;
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
            glowPoints = D_shelter_b1_north_maintenance_walkway_80184AB8;
            glowDrawDimGreyCapsule(&glowPoints[0], LAMP_GLOW_RADIUS_SCALE, 0);
            glowDrawDimGreyCapsule(&glowPoints[6], LAMP_GLOW_RADIUS_SCALE, -GLOW_QUARTER_TURN);
            break;
        }
    }
}
