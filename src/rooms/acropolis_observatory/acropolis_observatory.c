#include "rooms/acropolis_observatory.h"

#include "types.h"

#include "acropolis_observatory_private.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room's message table, published at `Task::msgTable` by the room task.
extern TaskMessageEntry D_acropolis_observatory_8017E7B8[];

/// Set once the room task has spawned the streamed scene for this visit.
extern s32 D_acropolis_observatory_8017E7D8;

static s32 _acropolisObservatoryResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisObservatoryRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _acropolisObservatoryHandleRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

TaskMessageEntry D_acropolis_observatory_8017E7B8[4] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisObservatoryResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisObservatoryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisObservatoryHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_acropolis_observatory_8017E7D8;

static void _acropolisObservatoryInitializeRoomTask(Task* task);
static void func_acropolis_observatory_8017D8AC(Task* task);

/// Resolves the observatory's exits from route progress and movie availability.
///
/// Borrows complete request/reply records, which may alias. Queries only copy
/// the request. Execution records the forked-road exit's first use and advances
/// promenade route progress from 3 to 4. Missing movie data, debug bypass or
/// an already-used route selects alternate arrival 5 (forked road) or 1
/// (promenade). Destination rooms follow the security-lock and bridge flags.
/// Always returns 1 to permit the transition; receiver and message ID are unused.
static s32 _acropolisObservatoryResolveRoomTransition(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_OBSERVATORY_FORKED_ROAD_ALTERNATE_ARRIVAL = 5,
        ACROPOLIS_OBSERVATORY_PROMENADE_ALTERNATE_ARRIVAL   = 1,
        ACROPOLIS_OBSERVATORY_EXIT_UNUSED                   = 0,
        ACROPOLIS_OBSERVATORY_EXIT_USED                     = 1,
        ACROPOLIS_OBSERVATORY_ROUTE_ARRIVED                 = 3,
        ACROPOLIS_OBSERVATORY_ROUTE_DEPARTED                = 4,
        ACROPOLIS_OBSERVATORY_SECURITY_EXIT_ROOM_BIT        = 1,
        ACROPOLIS_OBSERVATORY_DESTINATION_ROOM_INITIAL      = 1,
        ACROPOLIS_OBSERVATORY_DESTINATION_ROOM_CHANGED      = 2,
        ACROPOLIS_OBSERVATORY_TRANSITION_ALLOWED            = 1
    };
    s32 destinationRoom;

    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_FORKED_ROAD && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0) {
            reply->warp = ACROPOLIS_OBSERVATORY_FORKED_ROAD_ALTERNATE_ARRIVAL;
        }
        if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_EXIT_USED) == ACROPOLIS_OBSERVATORY_EXIT_UNUSED) {
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_EXIT_USED, ACROPOLIS_OBSERVATORY_EXIT_USED);
        } else {
            reply->warp = ACROPOLIS_OBSERVATORY_FORKED_ROAD_ALTERNATE_ARRIVAL;
        }
        if (request->areaId == GAME_AREA_ACROPOLIS_FORKED_ROAD) {
            if (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & ACROPOLIS_OBSERVATORY_SECURITY_EXIT_ROOM_BIT) {
                reply->room = ACROPOLIS_OBSERVATORY_DESTINATION_ROOM_CHANGED;
            }
        }
    }
    if (request->areaId == GAME_AREA_ACROPOLIS_PROMENADE) {
        if (gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                reply->warp = ACROPOLIS_OBSERVATORY_PROMENADE_ALTERNATE_ARRIVAL;
            }
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) == ACROPOLIS_OBSERVATORY_ROUTE_ARRIVED) {
                gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, ACROPOLIS_OBSERVATORY_ROUTE_DEPARTED);
            } else {
                reply->warp = ACROPOLIS_OBSERVATORY_PROMENADE_ALTERNATE_ARRIVAL;
            }
        }
        if (request->areaId == GAME_AREA_ACROPOLIS_PROMENADE && request->queryOnly == ROOM_EVENT_EXECUTE) {
            destinationRoom = gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS);
            if (destinationRoom == 0) {
                destinationRoom = ACROPOLIS_OBSERVATORY_DESTINATION_ROOM_INITIAL;
            } else {
                destinationRoom = ACROPOLIS_OBSERVATORY_DESTINATION_ROOM_CHANGED;
            }
            reply->room = destinationRoom;
        }
    }
    return ACROPOLIS_OBSERVATORY_TRANSITION_ALLOWED;
}

/// Refuses key-item use without consuming the item or starting a room event.
///
/// All arguments are ignored. The reply selects the inventory's "No use now" notice.
static s32 _acropolisObservatoryRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Starts the observatory's unseen event when room 2 receives action 1.
///
/// Borrows a four-byte `DirectionActionRequest` for synchronous dispatch;
/// latches the event before spawning it. Other actions, rooms and later visits
/// leave progress intact. Ignores the receiver, message ID and zero second
/// payload word, retains no request pointer and always returns zero.
static s32 _acropolisObservatoryHandleRoomAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_OBSERVATORY_ACTION_START_EVENT = 1,
        ACROPOLIS_OBSERVATORY_EVENT_ROOM         = 2,
        ACROPOLIS_OBSERVATORY_EVENT_UNSEEN       = 0,
        ACROPOLIS_OBSERVATORY_EVENT_SEEN         = 1
    };

    if ((request->actionId == ACROPOLIS_OBSERVATORY_ACTION_START_EVENT) &&
        (gGameSession->location.loc.room == ACROPOLIS_OBSERVATORY_EVENT_ROOM) &&
        (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN) == ACROPOLIS_OBSERVATORY_EVENT_UNSEEN)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN, ACROPOLIS_OBSERVATORY_EVENT_SEEN);
        taskSpawnFromTable(&D_acropolis_observatory_8017FE6C, 0, 0, 0);
    }
    return 0;
}

/// Registers the observatory's room-message receiver and activates its unseen encounter.
///
/// Requires a live task in state 0 and loaded room resources. Publishes the task
/// in `GAME_TASK_SLOT_ROOM`, borrowing the message table. Room 2 activates the
/// enemy wave controller while the observatory event is unseen; enters state 1.
static void _acropolisObservatoryInitializeRoomTask(Task* task)
{
    enum {
        ACROPOLIS_OBSERVATORY_ENCOUNTER_ROOM  = 2,
        ACROPOLIS_OBSERVATORY_EVENT_UNSEEN    = 0,
        ACROPOLIS_OBSERVATORY_WAVES_ACTIVATED = 1
    };

    task->msgTable = D_acropolis_observatory_8017E7B8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.room == ACROPOLIS_OBSERVATORY_ENCOUNTER_ROOM) &&
        (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN) == ACROPOLIS_OBSERVATORY_EVENT_UNSEEN)) {
        gSceneCombatState.actor03700Wave = ACROPOLIS_OBSERVATORY_WAVES_ACTIVATED;
    }
    task->state = task->state + 1;
}

/// Second state of the room task: on a visit that arrived by warp 3 or 4 it
/// spawns the matching streamed-scene ride from the room's task table (entry 1
/// or 0), once per visit.
static void func_acropolis_observatory_8017D8AC(Task* task)
{
    if ((D_acropolis_observatory_8017E7D8 == 0) && (gGameSession->location.loc.warp == 3)) {
        D_acropolis_observatory_8017E7D8 = 1;
        taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 1, 0, 0);
    }
    if ((D_acropolis_observatory_8017E7D8 == 0) && (gGameSession->location.loc.warp == 4)) {
        D_acropolis_observatory_8017E7D8 = 1;
        taskSpawnFromTable(D_acropolis_observatory_8017E7DC, 0, 0, 0);
    }
}

/// The room task's three states.
static const TaskFuncTable3 D_acropolis_observatory_8017D5C4 = {
    { _acropolisObservatoryInitializeRoomTask, func_acropolis_observatory_8017D8AC, taskKill },
};

void acropolisObservatoryRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_acropolis_observatory_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
