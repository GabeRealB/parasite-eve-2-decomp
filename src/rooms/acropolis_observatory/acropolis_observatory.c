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
s32        func_acropolis_observatory_8017D7C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

TaskMessageEntry D_acropolis_observatory_8017E7B8[4] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisObservatoryResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisObservatoryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_observatory_8017D7C4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_acropolis_observatory_8017E7D8;

static void func_acropolis_observatory_8017D834(Task* task);
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

/// Message gate for the observatory hotspot: sub-id 1 arms the room's task the
/// first time it fires during session phase 2, latching nibble 0xCA so a later
/// visit does nothing. The outgoing record is never written - this handler only
/// consumes the message.
s32 func_acropolis_observatory_8017D7C4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if ((in->warp == 1) && (gGameSession->location.loc.room == 2) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN, 1);
        taskSpawnFromTable(&D_acropolis_observatory_8017FE6C, 0, 0, 0);
    }
    return 0;
}

/// Observatory task entry: parks the overlay's message table in the task and
/// registers it as the room's slot-7 pointer. On the phase-2 visit that has not
/// yet latched nibble 0xCA it also arms the shared field-actor byte, then steps
/// the task on to its next state.
static void func_acropolis_observatory_8017D834(Task* task)
{
    task->msgTable = D_acropolis_observatory_8017E7B8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.room == 2) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN) == 0)) {
        gSceneCombatState.actor03700Wave = 1;
    }
    task->state = (s32)(task->state + 1);
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
    { func_acropolis_observatory_8017D834, func_acropolis_observatory_8017D8AC, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_acropolis_observatory_8017D5C4`.
void func_acropolis_observatory_8017D950(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_observatory_8017D5C4;
    sp.funcs[task->state](task);
}
