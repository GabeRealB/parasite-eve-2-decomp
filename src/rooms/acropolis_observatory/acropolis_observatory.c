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

s32 func_acropolis_observatory_8017D618(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_acropolis_observatory_8017D7BC(Task*, s32, s32, s32);
s32 func_acropolis_observatory_8017D7C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

TaskMessageEntry D_acropolis_observatory_8017E7B8[4] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_observatory_8017D618 },
    { 5105, func_acropolis_observatory_8017D7BC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_observatory_8017D7C4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_acropolis_observatory_8017E7D8;

static void func_acropolis_observatory_8017D834(Task* task);
static void func_acropolis_observatory_8017D8AC(Task* task);

/// Message gate for the observatory's two hotspots: copies the incoming record
/// to the outgoing one, then edits the copy according to the message id and the
/// game's progress nibbles.
///
/// Transitions to the forked road (area 9) and the promenade (area 0xB) answer with a
/// `warp` refusal code — 5 and 1 respectively — while the disc has no `.STR`
/// movie file (`gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0`) or the message's
/// nibble is not in the state that lets it run once. The first pass through
/// each also advances that nibble, so the refusal only shows on later visits.
/// `queryOnly` non-zero means "report only", which suppresses both the nibble
/// writes and the refusals.
s32 func_acropolis_observatory_8017D618(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 answer;

    *out = *in;
    if (in->areaId == GAME_AREA_ACROPOLIS_FORKED_ROAD && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0) {
            out->warp = 5;
        }
        if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_EXIT_USED) == 0) {
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_EXIT_USED, 1);
        } else {
            out->warp = 5;
        }
        if (in->areaId == GAME_AREA_ACROPOLIS_FORKED_ROAD) {
            if (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 1) {
                out->room = 2;
            }
        }
    }
    if (in->areaId == GAME_AREA_ACROPOLIS_PROMENADE) {
        if (gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                out->warp = 1;
            }
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) == 3) {
                gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, 4);
            } else {
                out->warp = 1;
            }
        }
        if (in->areaId == GAME_AREA_ACROPOLIS_PROMENADE && in->queryOnly == ROOM_EVENT_EXECUTE) {
            answer = gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS);
            if (answer == 0) {
                answer = 1;
            } else {
                answer = 2;
            }
            out->room = answer;
        }
    }
    return 1;
}

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_acropolis_observatory_8017D7BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Message gate for the observatory hotspot: sub-id 1 arms the room's task the
/// first time it fires during session phase 2, latching nibble 0xCA so a later
/// visit does nothing. The outgoing record is never written - this handler only
/// consumes the message.
s32 func_acropolis_observatory_8017D7C4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if ((in->warp == 1) && (gGameSession->location.loc.room == 2) && (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN, 1);
        Task_SpawnFromTable(&D_acropolis_observatory_8017FE6C, 0, 0, 0);
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
        Task_SpawnFromTable(D_acropolis_observatory_8017E7DC, 1, 0, 0);
    }
    if ((D_acropolis_observatory_8017E7D8 == 0) && (gGameSession->location.loc.warp == 4)) {
        D_acropolis_observatory_8017E7D8 = 1;
        Task_SpawnFromTable(D_acropolis_observatory_8017E7DC, 0, 0, 0);
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
