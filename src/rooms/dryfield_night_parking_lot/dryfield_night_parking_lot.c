#include "rooms/dryfield_night_parking_lot.h"

#include "types.h"

#include "dryfield_night_parking_lot_private.h"

#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
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

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/parking_lot.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

AreaApplyRec gParkingLotAreaRecs[2] = {
    { 3, 24, 2, 0 },
    { 255, 0, 0, 0 },
};

RoomEventMsg gRoomEventMsg = { 0 };

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
RoomEventActiveBytes gRoomEventActive = { 0, { 63, 252, 16 } };

RoomEventReq gRoomEventReq;

static void func_dryfield_night_parking_lot_8017DBB0(Task* task);
static void func_dryfield_night_parking_lot_8017DC28(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/parking_lot_event_msg.inc.c"

#include "../../shared/parking_lot_sound_msg.inc.c"

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_night_parking_lot_8017DB04(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for message 0x13F0 in the room's message table: point 4 runs CAP
/// command 4. Always returns 0.
s32 func_dryfield_night_parking_lot_8017DB0C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 4) {
        capRunCommandWithTransition(4);
    }
    return 0;
}

/// Handler for message 0x13EF in the room's message table. When the record's
/// `actionId` is 1 on the visit whose `place` is 3, it latches nibble 0x79 once,
/// sends the player-weapon message and passes
/// `D_dryfield_night_parking_lot_8017ECB4` to `evsStartScript`. Always returns 0.
s32 func_dryfield_night_parking_lot_8017DB34(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if ((request->actionId == 1) && (gGameSession->location.loc.variant == 3) && (gameFlagGetNibble(GAME_FLAG_NIGHT_PARKING_LOT_EVENT_SEEN) == 0)) {
        gameFlagSetNibble(GAME_FLAG_NIGHT_PARKING_LOT_EVENT_SEEN, 1);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        evsStartScript(D_dryfield_night_parking_lot_8017ECB4, EVENT_SCRIPT_HUD_KEEP);
    }
    return 0;
}

/// Script callback the room's script table names: stores its argument in
/// `gSceneCombatState.actor01600Wave`.
void func_dryfield_night_parking_lot_8017DBA4(s32 arg0)
{
    gSceneCombatState.actor01600Wave = arg0;
}

/// Room entry task state 0: parks the room's message table in `Task::msgTable`
/// and publishes the task in pointer slot 7. On the visit whose `place` is 3,
/// once nibble 0x79 is set - the nibble the 0x13EF handler
/// `func_dryfield_night_parking_lot_8017DB34` latches - it also sets
/// `gSceneCombatState.actor01600Wave` to 2. The state then advances.
static void func_dryfield_night_parking_lot_8017DBB0(Task* task)
{
    task->msgTable = D_dryfield_night_parking_lot_8017EC60;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == 3) && (gameFlagGetNibble(GAME_FLAG_NIGHT_PARKING_LOT_EVENT_SEEN) != 0)) {
        gSceneCombatState.actor01600Wave = 2;
    }
    task->state = (s32)(task->state + 1);
}

/// Room entry task state 1: does nothing, and nothing here advances the state.
static void func_dryfield_night_parking_lot_8017DC28(Task* task)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_parking_lot_8017D5DC = {
    { func_dryfield_night_parking_lot_8017DBB0, func_dryfield_night_parking_lot_8017DC28, taskKill },
};

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_night_parking_lot_8017DC30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_parking_lot_8017D5DC;
    sp.funcs[task->state](task);
}
