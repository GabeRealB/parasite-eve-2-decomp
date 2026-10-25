#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "factory_private.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sound.h"

#include "main/display.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_factory.h"

/// Selects the gate's TU-local room-transition task descriptor.
///
/// Addressable `TaskDesc` lvalue; see `room_events.h` for the spawn contract.
/// Bind before that header and keep through `room_event.inc.c`.
#define ROOM_EVENT_TASK_DESC (_gRoomEventTaskDesc)
#include "../../../../shared/room/room_events.h"
#include "../../../../shared/room/room_variants.h"

static void _factoryRoomInit(Task* task);
static void _factoryEntryIdle(Task* task);

/// The pending event request the gate latched, the flag saying
/// one was latched, and the descriptor of the task the gate spawns to play it.
extern u8           gRoomEventActive;
extern RoomEventReq gRoomEventReq;

/// Bodyless transition-task recipe shared by this overlay's gate and event task.
///
/// Runs at priority `TASK_PRIORITY_SCENE` with zero spawn payloads and unused metadata. The gate
/// reads the recipe synchronously after latching the request and destination;
/// keep those snapshots and the room callback code live through session reload.
static TaskDesc _gRoomEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventTask, { .value = 0 } };

/// Spawn recipe for the operator-panel session's polling owner, followed by TASK_DESC_END.
///
/// Entry 0 creates a bodyless priority-32 task which spawns the stage-selected
/// panel and polls its exit. It borrows the room-owned panel-task slot; the
/// terminal entry is retained but is never spawned by the command handler.
static TaskDesc _gFactoryPanelSessionDesc[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, factoryPanelSpawn, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Factory room messages: departures, key-item refusal, CAP commands, sound and actions.
///
/// Five handlers followed by `TASK_MESSAGE_TABLE_END`. Resolve borrows an
/// eight-byte request/reply pair; command and sound take integer first words;
/// the direction action borrows its four-byte request. All entries use native
/// payload signatures. The room task borrows the table and code while loaded.
static TaskMessageEntry _gFactoryMsgTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantFactoryMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, factoryIgnoreMessage },
    { ROOM_MESSAGE_COMMAND, factoryCommand },
    { ROOM_MESSAGE_SOUND, factorySoundCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, factoryRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../../../shared/room/room_event.inc.c"

/// Registers the factory room task and spawns its lift and barrier collision.
///
/// Owns one zeroed `Task*` slot at `Task::work`, also published as
/// `gFactoryPanelSlot`; room teardown releases it. The lift and panel poller
/// borrow that slot, which must outlive both. Runtime stage selects the day
/// spawn/panel tables for daytime Dryfield and night tables otherwise, then
/// power progress controls view 9's sprite. Allocation/spawning is unchecked.
/// Starts at task state 0 and advances to the idle room-message receiver.
static void _factoryRoomInit(Task* task)
{
    Task** slot;

    task->msgTable = _gFactoryMsgTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    slot       = (gFactoryPanelSlot = memCalloc(sizeof(*slot), 0));
    task->work = slot;
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        gFactorySpawnTable = gFactoryDaySpawnTable;
    } else {
        gFactorySpawnTable = gDryfieldNightFactoryTaskDescs;
    }
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        gFactoryPanelDesc = &gFactoryDayPanelDesc;
    } else {
        gFactoryPanelDesc = &gDryfieldNightFactoryPanelTaskDesc;
    }
    taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_LIFT, 0, gFactoryPanelSlot);
    taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_BARRIER_COLLISION, 0, 0);
    if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
        dryfieldFactorySetView9SpriteVisible(gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) & 0xFF);
    } else {
        dryfieldNightFactorySetView9SpriteVisible(gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) & 0xFF);
    }
    task->state++;
}

s32 roomVariantFactoryMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    RoomEventReq eventRequest;
    u8           stageId;

    // Preserve the complete request before execution-only destination selection.
    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_DRIVEWAY) {
        stageId = gGameSession->location.stage;
        if (stageId == GAME_STAGE_DRYFIELD) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) >= FACTORY_DRIVEWAY_PROGRESS_OPEN) {
                    reply->roomState = stageId;
                } else {
                    reply->roomState = FACTORY_DRIVEWAY_DEFAULT_ROOM;
                }
            }
        } else if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->roomState = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_GARAGE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) < FACTORY_GARAGE_FINAL_CHAPTER) {
                reply->roomState = FACTORY_GARAGE_EARLY_ROOM;
            } else {
                reply->roomState = FACTORY_GARAGE_LATE_ROOM;
            }
        }
        if (request->areaId == GAME_AREA_DRYFIELD_GARAGE) {
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS) != FACTORY_LAMP_PROGRESS_SECOND) {
                if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                    return ROOM_VARIANT_TRANSITION_REFUSED;
                }
                capStartSequenceSlot(FACTORY_GARAGE_REFUSAL_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                gameFlagSetNibbleIfPresent(request->flagId, FACTORY_DEPARTURE_REFUSAL_MAP_MARK);
                return ROOM_VARIANT_TRANSITION_REFUSED;
            }
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_BREEZEWAY) {
        if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FACTORY_DOOR_UNLOCKED) == 0) {
            if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                return ROOM_VARIANT_TRANSITION_REFUSED;
            }
            gameFlagSetNibbleIfPresent(request->flagId, FACTORY_DEPARTURE_REFUSAL_MAP_MARK);
            capRunCommandWithTransition(FACTORY_BREEZEWAY_REFUSAL_CAP_COMMAND);
            return ROOM_VARIANT_TRANSITION_REFUSED;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_DRIVEWAY) {
        eventRequest.capCmd        = FACTORY_DRIVEWAY_CAP_COMMAND;
        eventRequest.missingCapCmd = FACTORY_DRIVEWAY_CAP_COMMAND;
        eventRequest.firstSnd      = FACTORY_DAY_SOUND(FACTORY_SOUND_DRIVEWAY_EVENT_FIRST);
        eventRequest.secondSnd     = FACTORY_DAY_SOUND(FACTORY_SOUND_DRIVEWAY_EVENT_SECOND);
        eventRequest.flagId        = -GAME_FLAG_030;
        eventRequest.collectedBit  = ROOM_EVENT_GATE_NO_COLLECTION_REQUIRED;
        return _roomEventGate(&eventRequest, request);
    }
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

void factoryPanelSpawn(Task* task)
{
    enum { FACTORY_PANEL_SPAWN_INIT = 0,
           FACTORY_PANEL_SPAWN_WAIT = 1 };
    s32 scriptResult;

    switch (task->state) {
        case FACTORY_PANEL_SPAWN_INIT:
            *gFactoryPanelSlot = taskSpawnFromTable(gFactoryPanelDesc, 0, 0, 0);
            task->state++;
            return;
        case FACTORY_PANEL_SPAWN_WAIT:
            if (taskPollKill(*gFactoryPanelSlot, &scriptResult) != 0) {
                taskKill(task);
            }
            return;
    }
}

/// Factory room-entry lifecycle handlers, indexed by `Task::state`.
///
/// Setup registers the room and owns the panel-task slot; run retains it for
/// room messages; teardown releases the task and its work. Dispatch copies
/// all three callbacks and requires a `FACTORY_TASK_STATE_` selector.
static const TaskFuncTable3 _gFactoryEntryStates = {
    { [FACTORY_TASK_STATE_SETUP]    = _factoryRoomInit,
      [FACTORY_TASK_STATE_RUN]      = _factoryEntryIdle,
      [FACTORY_TASK_STATE_TEARDOWN] = taskKill },
};

s32 factoryIgnoreMessage(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 factoryCommand(Task* task, s32 messageId, s32 command, s32 unusedArgument)
{
    switch (command) {
        case FACTORY_COMMAND_CAP_SCENE:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_CAP_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_BARRIER_TRANSITION:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_BARRIER_TRANSITION_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_LAMP_SCENE:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_LAMP_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_POWER_SCENE:
            taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_POWER_SCENE, command, 0);
            break;
        case FACTORY_COMMAND_PANEL_SESSION:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            taskSpawnFromTable(_gFactoryPanelSessionDesc, 0, 0, 0);
            break;
        case FACTORY_COMMAND_HATCH_SCENE:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) == FACTORY_LIFT_POSITION_TURNED) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(gFactorySpawnTable, FACTORY_SPAWN_HATCH_SCENE, command, 0);
            }
            break;
    }
    return 0;
}

s32 factorySoundCommand(Task* task, s32 messageId, s32 soundCue, s32 secondArg)
{
    switch (soundCue) {
        case FACTORY_SOUND_SCRIPT_CUE_7:
            sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_SCRIPT_CUE_7), 0, 0);
            break;
        case FACTORY_SOUND_LAMP_SECOND_POSITION:
            sndEvtRequestStageScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_LAMP_SECOND_POSITION), 0, 0);
            gameFlagSetNibble(GAME_FLAG_FACTORY_LAMP_PROGRESS, FACTORY_LAMP_PROGRESS_SECOND);
            break;
    }
    return 0;
}

s32 factoryRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if ((request->actionId == FACTORY_ACTION_START_SCENE) && (gameFlagGetNibble(GAME_FLAG_02C) == 0)) {
        capSpawnEventIfIdle(FACTORY_START_SCENE_CAP, CAP_EVENT_PAUSE_ACTORS);
        gameFlagSetNibble(GAME_FLAG_02C, 1);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, FACTORY_START_SCENE_OBJECTIVE);
        sndEvtRequestScriptStart(FACTORY_DAY_SOUND(FACTORY_SOUND_START_SCENE), 0, 0);
    }
    return 0;
}

/// Keeps the factory room's entry task alive to receive room messages.
///
/// The initialized task retains its message table and panel-task slot until
/// teardown; this per-frame callback leaves them and the state unchanged.
static void _factoryEntryIdle(Task* task)
{
}

void FACTORY_ROOM_INSTANCE_ENTRY_TASK(Task* task)
{
    TaskFuncTable3 stateHandlers = _gFactoryEntryStates;

    stateHandlers.funcs[task->state](task);
}
