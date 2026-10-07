#include "types.h"

#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// The cap script task the entry task spawns, the target of the scene task's
/// message `DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN` and of the room's message 0x13F4.
Task* D_dryfield_water_tower_801876A0;

Task* D_dryfield_water_tower_801876A4;

#include "rooms/dryfield_water_tower.h"

#include "dryfield_water_tower_private.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
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

#include "rooms/room_common.h"
#include "../../shared/room_events.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */

/// The event the room's gate `_roomEventGate` latched:
/// the incoming message and the request, kept for the event task it spawns
/// from `gRoomEventTaskDesc`, and the flag the gate sets once it
/// has done so.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;
extern u8           gRoomEventActive;

static void func_dryfield_water_tower_8017DD6C(Task* arg0);
static void func_dryfield_water_tower_8017DDD0(Task* task);

static void func_dryfield_water_tower_8017DCB4(void);

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

u16 D_dryfield_water_tower_801876A8;

u16 D_dryfield_water_tower_801876AA;

Task* D_dryfield_water_tower_801876AC;

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room entry task's three states: install the room and spawn the cap
/// script, idle, and `taskKill`.
static const TaskFuncTable3 D_dryfield_water_tower_8017D5DC = {
    { func_dryfield_water_tower_8017DD6C, func_dryfield_water_tower_8017DDD0, taskKill },
};

/// The room's scene task, spawned on script event 7. Unless nibble 0x55 has
/// reached 2 it hides the player's weapon, runs CAP command 7 and waits for it,
/// saving the view byte; a key answer of 0xA then sets nibble 0x55 to 2, sends
/// `DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN` to the cap script and plays 0x52140009, and any other answer restores
/// the session and the view byte. With nibble 0x55 already at 2 it only runs
/// CAP command 7. Every finished path kills the task.
void func_dryfield_water_tower_8017D948(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE) < 2) {
                func_dryfield_water_tower_8017DCB4();
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                capRunCommand(7, CAP_PLAYBACK_IN_PLACE);
                gGameSession->eventState = 1;
                {
                    u32 view                             = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
                    s32 state                            = arg0->state;
                    D_dryfield_water_tower_8018768C.view = view;
                    arg0->state                          = state + 1;
                }
                return;
            }
            capRunCommandWithTransition(7);
            break;
        case 1:
            if (capIsBusy() == 0) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
                /* keeps the `lw state` behind the `sb` instead of filling its load delay */
                arg0->state = arg0->state + 1;
            }
            return;
        case 2:
            if (capGetVariantKey() == 0xA) {
                gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, 2);
                func_dryfield_water_tower_8017DCB4();
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskMessageDispatch(D_dryfield_water_tower_801876A0, DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN, 0, 0);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 9), 0, 0);
            } else {
                gGameSession->eventState                                   = 0;
                gGameSession->hideHud                                      = 0;
                gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_dryfield_water_tower_8018768C.view;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            }
            break;
        default:
            return;
    }
    taskKill(arg0);
}

#include "../../shared/water_tower_event_msg.inc.c"

#include "../../shared/water_tower_sound_msg.inc.c"

/// Shows or hides the view's sprites from nibble 0x55: modes 0 and 1 draw
/// them, 2 and 3 skip them, and any other value leaves them alone.
static void func_dryfield_water_tower_8017DCB4(void)
{
    s32 mode = gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE);

    if (mode < 0) {
        return;
    }
    if (mode < 2) {
        dryfieldWaterTowerSetMechanismSpriteVisible(1);
    } else if (mode < 4) {
        dryfieldWaterTowerSetMechanismSpriteVisible(0);
    }
}

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_water_tower_8017DCFC(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// The room's handler for message 0x13F0: script event 7 spawns the scene
/// task; every event answers 0.
s32 func_dryfield_water_tower_8017DD04(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        taskSpawnFromTable(D_dryfield_water_tower_801803D8, 0, 0, 0);
    }
    return 0;
}

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_water_tower_8017DD3C(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return 0;
}

/// The room's handler for message 0x13F4: passes the message on to the cap
/// script task with the arguments it arrived with.
s32 func_dryfield_water_tower_8017DD44(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return taskMessageDispatch(D_dryfield_water_tower_801876A0, msgId, arg2, arg3);
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and spawns the cap script.
static void func_dryfield_water_tower_8017DD6C(Task* arg0)
{
    Task* temp_v0;

    arg0->msgTable = D_dryfield_water_tower_801803A0;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    temp_v0                         = taskSpawnFromTable(D_dryfield_water_tower_80182384, 0, 0, 0);
    arg0->state                     = (s32)(arg0->state + 1);
    D_dryfield_water_tower_801876A0 = temp_v0;
}

/// State 1 of the room entry task: idles.
static void func_dryfield_water_tower_8017DDD0(Task* task)
{
}

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_water_tower_8017DDD8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_tower_8017D5DC;
    sp.funcs[task->state](task);
}
