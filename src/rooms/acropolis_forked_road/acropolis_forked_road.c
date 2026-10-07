#include "rooms/acropolis_forked_road.h"

#include "common.h"

#include "acropolis_forked_road_private.h"

#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room's message table, installed on its entry task.
extern TaskMessageEntry D_acropolis_forked_road_80180F14[];

/// The room task's once-per-visit latch for the return ride, and the word that
/// follows it.
///
/// The return ride is the streamed scene that carries the player back along
/// the road when the session arrives here through warp 2. The room task polls
/// for that arrival every frame, so `spawned` keeps it from starting the scene
/// again. Nothing clears it; it is zero in the room's image, so it starts over
/// each time that image is loaded. The following word is zero and has no
/// recovered access; its role, and whether it belongs to the latch at all, is
/// unproven.
typedef struct {
    s32 spawned;    // Return-ride scene started this visit (0 not yet, 1 started)
    u8  field_4[4]; // Zero bytes with no accesses; role unproven
} _AcropolisForkedRoadReturnRideLatch;
STATIC_ASSERT_SIZEOF(_AcropolisForkedRoadReturnRideLatch, 8);

extern _AcropolisForkedRoadReturnRideLatch D_acropolis_forked_road_80180F3C;

static void func_acropolis_forked_road_8017D92C(Task* task);
static void func_acropolis_forked_road_8017D970(Task* task);

/// State handlers of the room's own task.
static const TaskFuncTable3 D_acropolis_forked_road_8017D5C4 = {
    { func_acropolis_forked_road_8017D92C, func_acropolis_forked_road_8017D970, taskKill }
};

s32 func_acropolis_forked_road_8017D5EC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_acropolis_forked_road_8017D850(Task*, s32, s32, s32);
s32 func_acropolis_forked_road_8017D858(Task*, s32, s32, s32);
s32 func_acropolis_forked_road_8017D8A8(Task* task, s32 msgId, const void* firstArg, s32 arg3);

TaskMessageEntry D_acropolis_forked_road_80180F14[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_forked_road_8017D5EC },
    { 5105, func_acropolis_forked_road_8017D850 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_forked_road_8017D858 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_forked_road_8017D8A8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

_AcropolisForkedRoadReturnRideLatch D_acropolis_forked_road_80180F3C = { 0, { 0 } };

/// Message gate for the forked road's two hotspots: copies the incoming record
/// to the outgoing one, then answers according to the message id and the
/// game's progress nibbles. `queryOnly` non-zero means "report only", so every
/// side effect below is skipped while the answer stays the same.
///
/// Message 8 (the path back down) marks itself with `room = 2` once nibble 9
/// has bit 1 set, then either plays capture slot 0 while nibble 0 is still
/// under 3 or, past that, plays slot 3 once and records it in nibble 0x13.
///
/// Message 0xA (the path on) runs capture command 2 while nibble 1 is under 2.
/// Once it is at 2 the forked-road cutscene spawns from
/// `D_acropolis_forked_road_80180F44` and nibble 1 advances to 3, unless the
/// disc has no `.STR` movie file (`gDisplayState.debugMode < 0 || D_8006AC30.startSector == 0`), in which
/// case the message is refused with `warp = 2`.
s32 func_acropolis_forked_road_8017D5EC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId == GAME_AREA_ACROPOLIS_FOUNTAIN) {
        if ((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 2) && (in->queryOnly == ROOM_EVENT_EXECUTE)) {
            out->room = 2;
        }
        if (in->areaId == GAME_AREA_ACROPOLIS_FOUNTAIN) {
            if (gameFlagGetNibble(0) < 3) {
                if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                    Gp_SetNibbleIf(in->flagId, 2);
                    Gp_StartCapSlot(1, 1, 0);
                }
                return 0;
            }
            if (gameFlagGetNibble(0) >= 3) {
                if (gameFlagGetNibble(GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED) == 0) {
                    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                        Gp_StartCapSlot(1, 1, 3);
                        gameFlagSetNibble(GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED, 1);
                    }
                }
                return 1;
            }
        }
    }
    if (in->areaId == GAME_AREA_ACROPOLIS_OBSERVATORY) {
        if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) < 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(2);
            }
            return 0;
        }
        if ((gDisplayState.debugMode < 0) || (D_8006AC30.startSector == 0)) {
            if (in->queryOnly != ROOM_EVENT_EXECUTE) {
                return 1;
            }
            out->warp = 2;
        } else if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 7;
                Gp_MsgPlayerWeapon(0);
                taskSpawnFromTable(D_acropolis_forked_road_80180F44, 0, 0, 0);
                gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, 3);
            }
            return 0;
        } else {
            out->warp = 2;
        }
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 1;
        }
        if ((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 2) == 0) {
            return 1;
        }
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN) != 0) {
            return 1;
        }
        out->room = 2;
        return 1;
    }
    return 1;
}

/// Room script callback with nothing to do: always answers 0.
s32 func_acropolis_forked_road_8017D850(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_acropolis_forked_road_8017D858(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 cmd;

    if (arg2 == 3) {
        if ((areaGetCurrentObjectState(0x18) == 0) || (areaGetCurrentObjectState(0x18) == 1)) {
            cmd = 3;
        } else {
            cmd = 4;
        }
        Gp_RunCapCmd1(cmd);
    }
    return 0;
}

s32 func_acropolis_forked_road_8017D8A8(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    u8 temp;

    if (request->actionId == 1 && (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 2)) {
        temp = gGameSession->location.loc.variant;
        if (((temp == 4) || (temp == 8)) && (gameFlagGetNibble(GAME_FLAG_FORKED_ROAD_EVENT_SEEN) == 0)) {
            evsStartScript(D_acropolis_forked_road_801820B8, EVENT_SCRIPT_HUD_KEEP);
            gameFlagSetNibble(GAME_FLAG_FORKED_ROAD_EVENT_SEEN, 1);
        }
    }
    return 1;
}

/// First state of the room's own task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
static void func_acropolis_forked_road_8017D92C(Task* task)
{
    task->msgTable = D_acropolis_forked_road_80180F14;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Per-frame state of the room's own task: the first frame the session's warp
/// id is 2, spawns entry 2 of the room's task table, the return ride, latching
/// `D_acropolis_forked_road_80180F3C.spawned` so that happens only once.
static void func_acropolis_forked_road_8017D970(Task* task)
{
    if ((D_acropolis_forked_road_80180F3C.spawned == 0) && (gGameSession->location.loc.warp == 2)) {
        D_acropolis_forked_road_80180F3C.spawned = 1;
        taskSpawnFromTable(D_acropolis_forked_road_80180F44, 2, 0, 0);
    }
}

/// Runs the room task's current state out of its three-entry handler table:
/// the setup state, the per-frame warp check, then `taskKill`. The table is
/// copied onto the stack before the call.
void func_acropolis_forked_road_8017D9CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_forked_road_8017D5C4;
    sp.funcs[task->state](task);
}
