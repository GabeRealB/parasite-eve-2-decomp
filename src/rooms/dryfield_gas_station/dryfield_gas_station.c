#include "rooms/dryfield_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_gas_station_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_cutscene.h"
#include "../../shared/room/room_variants.h"
#include "../../shared/gas_station_sounds.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// Saved `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view` (area id), restored when the cutscene ends.

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `gMcSaveData` address with case 0.

#include "../../shared/room/telephone.h"

static void _dryfieldGasStationArrivalSupervisorTask(Task* task);

#include "../../shared/room/telephone_data.inc.c"

/// This carrier's bodyless scene runner and timed sound task, followed by END.
///
/// Entry SCENE borrows a RoomCutsceneRec in spawnArg2 and takes the follow-up
/// CAP command in spawnArg1; SOUND takes a packed sound request in spawnArg2.
/// Both run at priority 32. Keep callback code and the scene record loaded
/// until completion; spawning copies the recipe and retains no table pointer.
static TaskDesc _gRoomCutsceneTaskDescs[ROOM_CUTSCENE_TASK_DESCRIPTOR_COUNT] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Bodyless arrival-supervisor recipe at slot 0, followed by TASK_DESC_END.
///
/// Spawned with zero payloads on saved warp 1. Spawning consumes the recipe;
/// callback code must remain loaded until it collects the arrival task's result.
static TaskDesc _gDryfieldGasStationArrivalSupervisorTaskDescs[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _dryfieldGasStationArrivalSupervisorTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static s32 _dryfieldGasStationRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldGasStationHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);

/// Gas-station transition, key-item, command and sound-cue message dispatch.
///
/// The room task borrows five records, including TASK_MESSAGE_TABLE_END,
/// while the overlay is loaded. Transition messages borrow request/reply
/// records; the other handlers consume integer arguments synchronously.
static TaskMessageEntry _gDryfieldGasStationRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantGasStationMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldGasStationRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _dryfieldGasStationHandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _gasStationCueSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _dryfieldGasStationInitRoomTask(Task* task);
static void _dryfieldGasStationIdleRoomTask(Task* unusedTask);

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x00\x12"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void dryfieldGasStationTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

#include "../../shared/room/room_variants_gas_station.inc.c"

#include "../../shared/gas_station_sounds_cue.inc.c"

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Refuses every request to use a collected key item at the gas station.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`. Ignores both integer payload words
/// and the receiver; returns `ROOM_KEY_ITEM_USE_REFUSED` without consuming an item.
static s32 _dryfieldGasStationRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Handles room command 1 by selecting the first scene or a later cutscene.
///
/// The first request latches the first-scene flag, starts CAP command 11 and
/// returns zero. Later requests stage view 8, CAP slot/file 1 and their sounds,
/// change saved warp 1 to 2, and return the spawned cutscene handle as an s32
/// message word. The shared parameters must outlive the child; another request
/// replaces them. Other command IDs return 1.
static s32 _dryfieldGasStationHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        DRYFIELD_GAS_STATION_COMMAND_CUTSCENE           = 1,
        DRYFIELD_GAS_STATION_CAP_FIRST_SCENE            = 0xB,
        DRYFIELD_GAS_STATION_RETURN_WARP                = 2,
        DRYFIELD_GAS_STATION_CUTSCENE_VIEW              = 8,
        DRYFIELD_GAS_STATION_CUTSCENE_FOLLOW_UP_COMMAND = 2,
    };
    if (command == DRYFIELD_GAS_STATION_COMMAND_CUTSCENE) {
        if (gameFlagGetNibble(GAME_FLAG_GAS_STATION_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_GAS_STATION_FIRST_SCENE, 1);
            capRunCommandWithTransition(DRYFIELD_GAS_STATION_CAP_FIRST_SCENE);
            return 0;
        }
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == command) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = DRYFIELD_GAS_STATION_RETURN_WARP;
        }
        gDryfieldGasStationRoomCutscene.view            = DRYFIELD_GAS_STATION_CUTSCENE_VIEW;
        gDryfieldGasStationRoomCutscene.capSlot         = command;
        gDryfieldGasStationRoomCutscene.capFile         = command;
        gDryfieldGasStationRoomCutscene.skipScene       = 0;
        gDryfieldGasStationRoomCutscene.startSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x05);
        gDryfieldGasStationRoomCutscene.endSound        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x07);
        gDryfieldGasStationRoomCutscene.sceneSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x08);
        gDryfieldGasStationRoomCutscene.afterSceneSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x10);
        return (s32)taskSpawnFromTable(_gRoomCutsceneTaskDescs, ROOM_CUTSCENE_TASK_SCENE, DRYFIELD_GAS_STATION_CUTSCENE_FOLLOW_UP_COMMAND, &gDryfieldGasStationRoomCutscene);
    }
    return 1;
}

/// Supervises the movie-and-cutscene arrival sequence started at warp 1.
///
/// Stores the child handle, polls its requested exit, and ends on the next frame.
/// Requires a successful spawn and the room/arrival resources to remain loaded.
/// The child's result is collected but unused; the stored handle is not cleared.
static void _dryfieldGasStationArrivalSupervisorTask(Task* task)
{
    enum {
        DRYFIELD_GAS_STATION_ARRIVAL_SPAWN = 0,
        DRYFIELD_GAS_STATION_ARRIVAL_WAIT  = 1,
        DRYFIELD_GAS_STATION_ARRIVAL_EXIT  = 2,
    };
    s32 arrivalResult;

    switch (task->state) {
        case DRYFIELD_GAS_STATION_ARRIVAL_SPAWN:
            gDryfieldGasStationArrivalTask = taskSpawnFromTable(gDryfieldGasStationArrivalTaskDescs, 0, 0, 0);
            task->state++;
            break;
        case DRYFIELD_GAS_STATION_ARRIVAL_WAIT:
            if (taskPollKill(gDryfieldGasStationArrivalTask, &arrivalResult) != 0) {
                task->state++;
            }
            break;
        case DRYFIELD_GAS_STATION_ARRIVAL_EXIT:
            taskKill(task);
            break;
    }
}

/// Registers the gas-station room receiver and initializes its arrival or music.
///
/// Saved warp 1 starts the arrival supervisor and commits chapter 2 with the
/// dialogue/follow-up flags cleared, even if spawning fails. Other arrivals
/// request area music. Installs the message table and room task slot, advances
/// to idle and enables post-CAP notifications. Requires loaded room resources.
static void _dryfieldGasStationInitRoomTask(Task* task)
{
    enum { ARRIVAL_WARP           = 1,
           ARRIVAL_STORY_CHAPTER  = 2,
           AREA_MUSIC_START_TICKS = 1 };
    task->msgTable = _gDryfieldGasStationRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp == ARRIVAL_WARP) {
        taskSpawnFromTable(_gDryfieldGasStationArrivalSupervisorTaskDescs, 0, 0, 0);
        gameFlagSetNibble(EVENT_FLAG_STORY_CHAPTER, ARRIVAL_STORY_CHAPTER);
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, 0);
    } else {
        stageMusicRequestAreaStart(AREA_MUSIC_START_TICKS);
    }
    task->state                     = task->state + 1;
    gCapRoomCompletionSoundsEnabled = CAP_ROOM_COMPLETION_SOUNDS_ENABLED;
}

/// Keeps the initialized gas-station room task idle in state 1.
///
/// Ignores its argument; messages continue through the installed room table.
static void _dryfieldGasStationIdleRoomTask(Task* unusedTask)
{
}

/// Room-task lifecycle: register the receiver and arrival/music, idle, release.
///
/// Copied by value for unchecked state 0..2 dispatch; callbacks require this overlay loaded.
static const TaskFuncTable3 _gDryfieldGasStationRoomTaskStates = {
    { _dryfieldGasStationInitRoomTask, _dryfieldGasStationIdleRoomTask, taskKill },
};

void dryfieldGasStationRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldGasStationRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
