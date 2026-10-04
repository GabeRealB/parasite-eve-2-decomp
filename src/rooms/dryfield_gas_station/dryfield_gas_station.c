#include "rooms/dryfield_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_gas_station_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
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

/// The list shown by `func_dryfield_gas_station_8017EA90`.
static UiList Telephone_Data_80181CF4;

extern TaskDesc         gRoomCutsceneTaskDescs[];
extern TaskDesc         D_dryfield_gas_station_80181E3C[];
extern TaskMessageEntry D_dryfield_gas_station_80181E54[5];

#define TELEPHONE_TITLE_BYTES "Telephone\0\0\x12"
#include "../../shared/telephone.h"

void func_dryfield_gas_station_8017FE20(Task*);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc D_dryfield_gas_station_80181E3C[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_gas_station_8017FE20, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 func_dryfield_gas_station_8017FD4C(Task*, s32, s32, s32);
s32 func_dryfield_gas_station_8017FD54(Task*, s32, s32, s32);

TaskMessageEntry D_dryfield_gas_station_80181E54[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantGasStationMsg },
    { 5105, func_dryfield_gas_station_8017FD4C },
    { ROOM_MESSAGE_COMMAND, func_dryfield_gas_station_8017FD54 },
    { ROOM_MESSAGE_SOUND, gasStationCueSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

static void func_dryfield_gas_station_8017FEDC(Task* arg0);
static void func_dryfield_gas_station_8017FF84(Task* task);

#include "../../shared/telephone.inc.c"

void func_dryfield_gas_station_8017EA90(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_variants_gas_station.inc.c"

#include "../../shared/gas_station_sounds_cue.inc.c"

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Always returns 0.
s32 func_dryfield_gas_station_8017FD4C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    return 0;
}

/// Cutscene trigger for the gas station. On request 1, if the `0x16B` flag is
/// clear it raises it and asks the cap system to run command 0xB; otherwise it
/// fills in the room's cap script (area 8, this request as the slot and file)
/// and spawns `gRoomCutsceneTaskDescs`. Returns 1 when the request is
/// not 1, otherwise the spawned task.
s32 func_dryfield_gas_station_8017FD54(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_GAS_STATION_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_GAS_STATION_FIRST_SCENE, 1);
            Gp_RunCapCmd1(0xB);
            return 0;
        }
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp == arg2) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 2;
        }
        D_dryfield_gas_station_80184BD8.view            = 8;
        D_dryfield_gas_station_80184BD8.capSlot         = arg2;
        D_dryfield_gas_station_80184BD8.capFile         = arg2;
        D_dryfield_gas_station_80184BD8.skipScene       = 0;
        D_dryfield_gas_station_80184BD8.startSound      = 0x52010005;
        D_dryfield_gas_station_80184BD8.endSound        = 0x52010007;
        D_dryfield_gas_station_80184BD8.sceneSound      = 0x52010008;
        D_dryfield_gas_station_80184BD8.afterSceneSound = 0x52010010;
        return (s32)Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, 2, &D_dryfield_gas_station_80184BD8);
    }
    return 1;
}

/// Spawns the room's event task and stores it in `D_dryfield_gas_station_80184BCC`,
/// waits for it to be killed, then kills this task.
void func_dryfield_gas_station_8017FE20(Task* arg0)
{
    s32 state = arg0->state;
    s32 out;

    switch (state) {
        case 0:
            D_dryfield_gas_station_80184BCC = Task_SpawnFromTable(D_dryfield_gas_station_80181E7C, 0, 0, 0);
            arg0->state++;
            break;
        case 1:
            if (Task_PollKill(D_dryfield_gas_station_80184BCC, &out) != 0) {
                arg0->state++;
            }
            break;
        case 2:
            taskKill(arg0);
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
        Task_SpawnFromTable(D_dryfield_gas_station_80181E3C, 0, 0, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_CHAPTER, 2);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 0);
    } else {
        Stage_RequestFromAreaTable(1);
    }
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// State 1 of the room's main task: does nothing.
static void func_dryfield_gas_station_8017FF84(Task* task)
{
}

/// The three states of the room's main task, run by
/// `func_dryfield_gas_station_8017FF8C`: set-up, the per-frame handler and the
/// kill.
static const TaskFuncTable3 D_dryfield_gas_station_8017D6A4 = {
    { func_dryfield_gas_station_8017FEDC, func_dryfield_gas_station_8017FF84, taskKill },
};

/// Dispatches the task through the room's three-state table, copied onto the
/// stack first.
void func_dryfield_gas_station_8017FF8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_gas_station_8017D6A4;
    sp.funcs[task->state](task);
}
