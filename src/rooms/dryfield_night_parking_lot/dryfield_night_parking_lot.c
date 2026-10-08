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

static void _dryfieldNightParkingLotInitializeRoom(Task* task);
static void _dryfieldNightParkingLotIdleState(Task* unusedTask);

/// Room variant carrying the scavengers' scripted entrance and subsequent battle.
enum { DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_VARIANT = 3 };

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/parking_lot_event_msg.inc.c"

#define PARKING_LOT_SOUND_MSG parkingLotSoundMsg
#include "../../shared/parking_lot_sound_msg.inc.c"

s32 dryfieldNightParkingLotRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 dryfieldNightParkingLotCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_NIGHT_PARKING_LOT_COMMAND_CAPTION = 4,
    };

    if (commandId == DRYFIELD_NIGHT_PARKING_LOT_COMMAND_CAPTION) {
        capRunCommandWithTransition(DRYFIELD_NIGHT_PARKING_LOT_COMMAND_CAPTION);
    }
    return 0;
}

s32 dryfieldNightParkingLotStartScavengerEncounterMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { DRYFIELD_NIGHT_PARKING_LOT_ACTION_SCAVENGER_ENTRANCE = 1 };

    if ((request->actionId == DRYFIELD_NIGHT_PARKING_LOT_ACTION_SCAVENGER_ENTRANCE) &&
        (gGameSession->location.loc.variant == DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_VARIANT) && (gameFlagGetNibble(GAME_FLAG_NIGHT_PARKING_LOT_EVENT_SEEN) == 0)) {
        // Latch before the script starts so another trigger cannot repeat the entrance.
        gameFlagSetNibble(GAME_FLAG_NIGHT_PARKING_LOT_EVENT_SEEN, 1);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        evsStartScript(D_dryfield_night_parking_lot_8017ECB4, EVENT_SCRIPT_HUD_KEEP);
    }
    return 0;
}

void dryfieldNightParkingLotSetScavengerWave(s32 wave)
{
    gSceneCombatState.actor01600Wave = wave;
}

/// Registers the night parking lot's room receiver and resumes its scavenger battle.
///
/// Requires the live room task in state 0. Variant 3 with the entrance event
/// already latched selects wave 2, activating the scavengers at their battle
/// positions instead of replaying their entrance. Advances to idle state 1.
static void _dryfieldNightParkingLotInitializeRoom(Task* task)
{
    enum { DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_BATTLE_WAVE = 2 };

    task->msgTable = D_dryfield_night_parking_lot_8017EC60;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.loc.variant == DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_VARIANT) && (gameFlagGetNibble(GAME_FLAG_NIGHT_PARKING_LOT_EVENT_SEEN) != 0)) {
        gSceneCombatState.actor01600Wave = DRYFIELD_NIGHT_PARKING_LOT_SCAVENGER_BATTLE_WAVE;
    }
    task->state++;
}

/// Keeps the initialized parking-lot room task alive to receive messages in state 1.
static void _dryfieldNightParkingLotIdleState(Task* unusedTask)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_parking_lot_8017D5DC = {
    { _dryfieldNightParkingLotInitializeRoom, _dryfieldNightParkingLotIdleState, taskKill },
};

void dryfieldNightParkingLotRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_parking_lot_8017D5DC;
    stateHandlers.funcs[task->state](task);
}
