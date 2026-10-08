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
#include "../../shared/room_variants.h"
#include "../../shared/water_tower.h"

/// The event the room's gate `_roomEventGate` latched:
/// the incoming message and the request, kept for the event task it spawns
/// from `gRoomEventTaskDesc`, and the flag the gate sets once it
/// has done so.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;
extern u8           gRoomEventActive;

static void _dryfieldWaterTowerInitializeRoomTask(Task* task);
static void _dryfieldWaterTowerIdleRoomTask(Task* task);

static void _dryfieldWaterTowerRestoreMechanismSpriteVisibility(void);

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
    { _dryfieldWaterTowerInitializeRoomTask, _dryfieldWaterTowerIdleRoomTask, taskKill },
};

/// Restores ordinary presentation after declining the tower mechanism prompt.
static inline void _dryfieldWaterTowerCancelMechanismPrompt(void)
{
    enum { DRYFIELD_WATER_TOWER_EVENT_IDLE = 0 };

    gGameSession->eventState                                   = DRYFIELD_WATER_TOWER_EVENT_IDLE;
    gGameSession->hideHud                                      = 0;
    gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_dryfield_water_tower_8018768C.view;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
}

void dryfieldWaterTowerMechanismPromptTask(Task* task)
{
    enum {
        DRYFIELD_WATER_TOWER_PROMPT_START          = 0,
        DRYFIELD_WATER_TOWER_PROMPT_WAIT           = 1,
        DRYFIELD_WATER_TOWER_PROMPT_REPLY          = 2,
        DRYFIELD_WATER_TOWER_CAP_MECHANISM_COMMAND = 7,
        DRYFIELD_WATER_TOWER_PROMPT_ACCEPTED_KEY   = 10,
        DRYFIELD_WATER_TOWER_EVENT_ACTIVE          = 1,
    };

    switch (task->state) {
        case DRYFIELD_WATER_TOWER_PROMPT_START:
            if (gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE) < GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED) {
                // Hold presentation until the prompt chooses a run or cancellation.
                _dryfieldWaterTowerRestoreMechanismSpriteVisibility();
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                capRunCommand(DRYFIELD_WATER_TOWER_CAP_MECHANISM_COMMAND, CAP_PLAYBACK_IN_PLACE);
                gGameSession->eventState = DRYFIELD_WATER_TOWER_EVENT_ACTIVE;
                {
                    u32 savedView                        = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
                    s32 promptState                      = task->state;
                    D_dryfield_water_tower_8018768C.view = savedView;
                    task->state                          = promptState + 1;
                }
                return;
            }
            capRunCommandWithTransition(DRYFIELD_WATER_TOWER_CAP_MECHANISM_COMMAND);
            break;
        case DRYFIELD_WATER_TOWER_PROMPT_WAIT:
            if (capIsBusy() == 0) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
                /* keeps the `lw state` behind the `sb` instead of filling its load delay */
                task->state = task->state + 1;
            }
            return;
        case DRYFIELD_WATER_TOWER_PROMPT_REPLY:
            if (capGetVariantKey() == DRYFIELD_WATER_TOWER_PROMPT_ACCEPTED_KEY) {
                // Hand the accepted run to the persistent prop-scene driver.
                gameFlagSetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE, GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED);
                _dryfieldWaterTowerRestoreMechanismSpriteVisibility();
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskMessageDispatch(D_dryfield_water_tower_801876A0, DRYFIELD_WATER_TOWER_MESSAGE_REQUEST_RUN, 0, 0);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_WATER_TOWER, 9), 0, 0);
            } else {
                // Cancellation restores the saved view and ordinary player control.
                _dryfieldWaterTowerCancelMechanismPrompt();
            }
            break;
        default:
            return;
    }
    taskKill(task);
}

#include "../../shared/water_tower_event_msg.inc.c"

#include "../../shared/water_tower_sound_msg.inc.c"

/// Restores the tower mechanism's background sprite from its saved operating state.
///
/// Initial/restored states show batch 1 of view 19; tower/tank-operated states
/// hide it. Other values leave it unchanged. Requires the daytime water-tower
/// sprite table to be loaded; the setter leaves other stages untouched.
static void _dryfieldWaterTowerRestoreMechanismSpriteVisibility(void)
{
    s32 mechanismState = gameFlagGetNibble(GAME_FLAG_WATER_TOWER_MECHANISM_STATE);

    if (mechanismState < GAME_FLAG_WATER_TOWER_MECHANISM_INITIAL) {
        return;
    }
    if (mechanismState < GAME_FLAG_WATER_TOWER_MECHANISM_TOWER_OPERATED) {
        dryfieldWaterTowerSetMechanismSpriteVisible(1);
    } else if (mechanismState < GAME_FLAG_WATER_TOWER_MECHANISM_TANK_OPERATED + 1) {
        dryfieldWaterTowerSetMechanismSpriteVisible(0);
    }
}

s32 dryfieldWaterTowerUseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 dryfieldWaterTowerCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_WATER_TOWER_COMMAND_MECHANISM           = 7,
        DRYFIELD_WATER_TOWER_MECHANISM_PROMPT_DESCRIPTOR = 0,
    };

    if (commandId == DRYFIELD_WATER_TOWER_COMMAND_MECHANISM) {
        taskSpawnFromTable(D_dryfield_water_tower_801803D8, DRYFIELD_WATER_TOWER_MECHANISM_PROMPT_DESCRIPTOR, 0, 0);
    }
    return 0;
}

s32 dryfieldWaterTowerRoomActionMsg(Task* task, s32 messageId, const DirectionActionRequest* request, s32 secondArg)
{
    return 0;
}

s32 dryfieldWaterTowerActorEventMsg(Task* task, s32 messageId, s32 eventId, s32 eventArg)
{
    return taskMessageDispatch(D_dryfield_water_tower_801876A0, messageId, eventId, eventArg);
}

/// Registers the water-tower message receiver and starts its prop-scene driver.
///
/// Enter in state 0 with the room resources loaded. Publishes the room task
/// and stores the spawned driver, including NULL on failure, before idling
/// in state 1. The room and driver borrow the overlay throughout their lives.
static void _dryfieldWaterTowerInitializeRoomTask(Task* task)
{
    Task* propSceneTask;

    task->msgTable = D_dryfield_water_tower_801803A0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    propSceneTask                   = taskSpawnFromTable(D_dryfield_water_tower_80182384, 0, 0, 0);
    task->state                     = task->state + 1;
    D_dryfield_water_tower_801876A0 = propSceneTask;
}

/// Keeps the installed room task available for messages between entry and teardown.
static void _dryfieldWaterTowerIdleRoomTask(Task* task)
{
}

void dryfieldWaterTowerRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_water_tower_8017D5DC;
    stateHandlers.funcs[task->state](task);
}
