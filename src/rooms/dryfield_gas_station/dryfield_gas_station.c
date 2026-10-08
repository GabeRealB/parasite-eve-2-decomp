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
#include "gameplay/object_task.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
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

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"
#include "../../shared/room_variants.h"
#include "../../shared/gas_station_sounds.h"

static void _roomCutsceneSoundTask(Task* task);

extern UiObjectDesc D_800611E4;

/// Saved `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` (area id), restored when the cutscene ends.

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `gMcSaveData` address with case 0.

/// Prompt texts: "Save", "Play Data", "Weapon Data" and "PE Data".
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row labels of the "Play Data" statistics list, one per row index.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// The suffix appended to the statistics rows that count events.
static u8 Telephone_Data_80181A70[];

/// The "%" suffix appended to a formatted percentage.
static u8 Telephone_Data_80181A78[];

/// Help lines shown for the selected statistics row, one per row index.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// The "Play Data" statistics list.
static UiList Telephone_Data_80181C44;

/// The usage list shown by `func_dryfield_gas_station_8017E8DC`.
static UiList Telephone_Data_80181C6C;

/// UI descriptor of the help-line box the "Play Data" and usage panels open
/// beside their lists.
static UiObjectDesc Telephone_Data_80181C90;

/// UI descriptors the "Play Data" and usage prompts open.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// The list shown by `dryfieldGasStationTelephoneMenuTask`.
static UiList Telephone_Data_80181CF4;

extern TaskDesc         gRoomCutsceneTaskDescs[];
extern TaskDesc         D_dryfield_gas_station_80181E3C[];
extern TaskMessageEntry D_dryfield_gas_station_80181E54[5];

#define TELEPHONE_TITLE_BYTES "Telephone\0\0\x12"
#include "../../shared/telephone.h"

static void _dryfieldGasStationArrivalSupervisorTask(Task* task);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc D_dryfield_gas_station_80181E3C[2] = {
    { { { TASK_BODY_NONE, 32 } }, _dryfieldGasStationArrivalSupervisorTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static s32 _dryfieldGasStationRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldGasStationHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);

TaskMessageEntry D_dryfield_gas_station_80181E54[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantGasStationMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldGasStationRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _dryfieldGasStationHandleRoomCommand },
    { ROOM_MESSAGE_SOUND, _gasStationCueSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

static void func_dryfield_gas_station_8017FEDC(Task* arg0);
static void _dryfieldGasStationIdleRoomTask(Task* unusedTask);

#include "../../shared/telephone.inc.c"

void dryfieldGasStationTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_variants_gas_station.inc.c"

#include "../../shared/gas_station_sounds_cue.inc.c"

#include "../../shared/room_cutscene_sound_task.inc.c"

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
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == command) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = DRYFIELD_GAS_STATION_RETURN_WARP;
        }
        D_dryfield_gas_station_80184BD8.view            = DRYFIELD_GAS_STATION_CUTSCENE_VIEW;
        D_dryfield_gas_station_80184BD8.capSlot         = command;
        D_dryfield_gas_station_80184BD8.capFile         = command;
        D_dryfield_gas_station_80184BD8.skipScene       = 0;
        D_dryfield_gas_station_80184BD8.startSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x05);
        D_dryfield_gas_station_80184BD8.endSound        = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x07);
        D_dryfield_gas_station_80184BD8.sceneSound      = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x08);
        D_dryfield_gas_station_80184BD8.afterSceneSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x10);
        return (s32)taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, DRYFIELD_GAS_STATION_CUTSCENE_FOLLOW_UP_COMMAND, &D_dryfield_gas_station_80184BD8);
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
            D_dryfield_gas_station_80184BCC = taskSpawnFromTable(D_dryfield_gas_station_80181E7C, 0, 0, 0);
            task->state++;
            break;
        case DRYFIELD_GAS_STATION_ARRIVAL_WAIT:
            if (taskPollKill(D_dryfield_gas_station_80184BCC, &arrivalResult) != 0) {
                task->state++;
            }
            break;
        case DRYFIELD_GAS_STATION_ARRIVAL_EXIT:
            taskKill(task);
            break;
    }
}

/// State 0 of the gas-station cutscene task. On the first visit
/// (`gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == 1`) it spawns the room's event task and clears the three
/// progression flags; otherwise it just asks the stage for area 1. Either way
/// it advances to state 1 and raises the `D_80115598` flag.
static void func_dryfield_gas_station_8017FEDC(Task* arg0)
{
    arg0->msgTable = D_dryfield_gas_station_80181E54;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == 1) {
        taskSpawnFromTable(D_dryfield_gas_station_80181E3C, 0, 0, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, 2);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0);
    } else {
        stageMusicRequestAreaStart(1);
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// Keeps the initialized gas-station room task idle in state 1.
///
/// Ignores its argument; messages continue through the installed room table.
static void _dryfieldGasStationIdleRoomTask(Task* unusedTask)
{
}

/// The three states of the room's main task, run by
/// `dryfieldGasStationRoomTask`: set-up, the per-frame handler and the
/// kill.
static const TaskFuncTable3 D_dryfield_gas_station_8017D6A4 = {
    { func_dryfield_gas_station_8017FEDC, _dryfieldGasStationIdleRoomTask, taskKill },
};

void dryfieldGasStationRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_gas_station_8017D6A4;
    stateHandlers.funcs[task->state](task);
}
