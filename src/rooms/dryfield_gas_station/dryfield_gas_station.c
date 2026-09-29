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
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

extern UiObjectDesc D_800611E4;

/// Saved `Mc_SaveData[0].state.at4.loc.view` (area id), restored when the cutscene ends.

/// `Mc_SaveData[0].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `Mc_SaveData` address with case 0.

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

extern TaskDesc D_dryfield_gas_station_80181E18[];
extern TaskDesc D_dryfield_gas_station_80181E3C[];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(s32, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call2)(s32, s32, s32);
    } handler;
} DryfieldGasStationMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldGasStationMessageEntry, 8);

extern DryfieldGasStationMessageEntry D_dryfield_gas_station_80181E54[5];

#define TELEPHONE_TITLE_BYTES "Telephone\0\0\x12"
#include "../../shared/telephone.h"

void func_dryfield_gas_station_8017F4B4(Task*);

void func_dryfield_gas_station_8017F4B4(Task*);
void func_dryfield_gas_station_8017FCBC(Task*);

void func_dryfield_gas_station_8017FE20(Task*);

#include "../../shared/telephone_data.inc.c"

void func_dryfield_gas_station_8017F4B4(Task*);
void func_dryfield_gas_station_8017FCBC(Task*);

TaskDesc D_dryfield_gas_station_80181E18[3] = {
    { 0, 32, func_dryfield_gas_station_8017F4B4, { .model = NULL } },
    { 0, 32, func_dryfield_gas_station_8017FCBC, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

TaskDesc D_dryfield_gas_station_80181E3C[2] = {
    { 0, 32, func_dryfield_gas_station_8017FE20, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 func_dryfield_gas_station_8017FA20(s32, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_gas_station_8017FB94(s32, s32, s32);
s32 func_dryfield_gas_station_8017FD4C(void);
s32 func_dryfield_gas_station_8017FD54(s32, s32, s32);

DryfieldGasStationMessageEntry D_dryfield_gas_station_80181E54[5] = {
    { 5102, { .call1 = func_dryfield_gas_station_8017FA20 } },
    { 5105, { .call0 = func_dryfield_gas_station_8017FD4C } },
    { 5104, { .call2 = func_dryfield_gas_station_8017FD54 } },
    { 5106, { .call2 = func_dryfield_gas_station_8017FB94 } },
    { 2147483647, { .call0 = NULL } },
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

/// Task body of the room's cutscene: `spawnArg2` is its
/// `RoomCutsceneRec`. It hides the HUD and the weapons, forces the
/// script's area, loads and plays the cap file while a child task from
/// `D_dryfield_gas_station_80181E18` runs (confirm or cancel skips it), then
/// runs the cap command for the story's progress, restores the area and the
/// HUD, and kills itself; states 20-23 are the branch that runs follow-up
/// commands 0x20 / 0x21 by the cap's event key.
void func_dryfield_gas_station_8017F4B4(Task* task)
{
    s32              poll;
    s32              cmd;
    s32              a0;
    s32              a1;
    s32              flag;
    RoomCutsceneRec* script;
    McSaveData*      save;

    script = task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            D_dryfield_gas_station_80184BD0 = NULL;
            Gp_MsgPlayerWeapon(0);
            save = &Mc_SaveData[0];
            if (save->state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (script->field_0 > 0) {
                D_80115694               = save->state.at4.loc.view;
                save->state.at4.loc.view = (u8)script->field_0;
            } else {
                D_80115694 = -script->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (script->field_4 != 0) {
                SndEvt_EnqueueType6(script->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (script->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(script->field_3);
                a0 = script->field_14;
                a1 = 0;
                if (a0 == 0) {
                    a0 = 0x3C0;
                } else {
                    a1 = script->field_16;
                }
                func_800E6D4C(a0, a1);
            }
            if (script->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_dryfield_gas_station_80184BD0 =
                Task_SpawnFromTable(D_dryfield_gas_station_80181E18, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_dryfield_gas_station_80184BD0);
                task->state++;
            } else if (Task_PollKill(D_dryfield_gas_station_80184BD0, &poll) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (script->field_2 == 0) {
                SndEvt_EnqueueType6(script->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (script->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(script->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GP_LOC_WORD(Mc_SaveData[0].state.at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData[0].state.at4.loc.view = (u8)D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(script->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (script->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// Answers the room message `in`, copying it to `out` first. For message 2 it
/// reports in `out->field_3` how far nibble 0x61 has advanced (3 once nibble
/// 0x7A reaches 4). Message 3 returns 2 when the session sits at stage 3,
/// place 1 with `Gp_StateF0` agreeing, and 0 while nibble 0x3B is clear;
/// message 2 returns 0 while nibble 0x45 reads 1. The cap commands and nibble
/// write that go with those answers run only when `in->field_5` is clear.
/// Every other case returns 1.
s32 func_dryfield_gas_station_8017FA20(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 n;
    s32 val;

    *out = *in;
    if (in->prefix.packed == 2 && in->field_5 == 0) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->field_3 = val;
    }
    if (in->prefix.packed == 3) {
        if ((gGameSession->at4.loc.stage == in->prefix.packed) && (gGameSession->at4.loc.place == 1) &&
            (Gp_StateF0.prefix.bytes.field_0 == gGameSession->at4.loc.place)) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(0x15);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x3B) == 0) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(7);
                Gp_SetNibbleIf(in->field_6, 2);
            }
            return 0;
        }
    }
    if (in->prefix.packed == 2) {
        if (GameFlag_GetNibble(0x45) == 1) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 0;
        }
    }
    return 1;
}

/// Maps a cap (cutscene) script event key to the stage sound it should play in
/// the gas station, then enqueues it as a type-6 sound event. Event key 0x83
/// only plays if a cap script is still reporting an event key. Keys with no
/// sound are ignored. Always returns 0.
s32 func_dryfield_gas_station_8017FB94(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 5:
            id = 0x52010005;
            goto play;
        case 7:
            id = 0x52010007;
            goto play;
        case 0xA:
            id = 0x5201000A;
            goto play;
        case 0xD:
            id = 0x5201000D;
            goto play;
        case 0x11:
            id = 0x52010011;
            goto play;
        case 0x13:
            id = 0x52010013;
            goto play;
        case 0x6D:
        case 0x82:
            id = 0x5201000B;
            goto play;
        case 0x73:
            id = 0x5201000E;
            goto play;
        case 0x83:
            if (Gp_GetCapEventKey() == 0) {
                break;
            }
            id = 0x52010012;
        play:
            Gp_EnqueueStageSnd6(id, 0, 0);
            break;
    }
    return 0;
}

/// Task body that enqueues the type-6 sound event held in `spawnArg2` on its
/// first tick and again at tick 0x50, and kills itself at tick 0x78; `state`
/// counts the ticks.
void func_dryfield_gas_station_8017FCBC(Task* task)
{
    switch (task->state) {
        case 0x50:
        case 0x0:
            SndEvt_EnqueueType6(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}

/// Always returns 0.
s32 func_dryfield_gas_station_8017FD4C(void)
{
    return 0;
}

/// Cutscene trigger for the gas station. On request 1, if the `0x16B` flag is
/// clear it raises it and asks the cap system to run command 0xB; otherwise it
/// fills in the room's cap script (area 8, this request as the slot and file)
/// and spawns `D_dryfield_gas_station_80181E18`. Returns 1 when the request is
/// not 1, otherwise the spawned task.
s32 func_dryfield_gas_station_8017FD54(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x16B) == 0) {
            GameFlag_SetNibble(0x16B, 1);
            Gp_RunCapCmd1(0xB);
            return 0;
        }
        if (Mc_SaveData[0].state.at4.loc.warp == arg2) {
            Mc_SaveData[0].state.at4.loc.warp = 2;
        }
        D_dryfield_gas_station_80184BD8.field_0  = 8;
        D_dryfield_gas_station_80184BD8.field_1  = arg2;
        D_dryfield_gas_station_80184BD8.field_3  = arg2;
        D_dryfield_gas_station_80184BD8.field_2  = 0;
        D_dryfield_gas_station_80184BD8.field_4  = 0x52010005;
        D_dryfield_gas_station_80184BD8.field_8  = 0x52010007;
        D_dryfield_gas_station_80184BD8.field_10 = 0x52010008;
        D_dryfield_gas_station_80184BD8.field_C  = 0x52010010;
        return (s32)Task_SpawnFromTable(D_dryfield_gas_station_80181E18, 0, 2, &D_dryfield_gas_station_80184BD8);
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
/// (`Mc_SaveData[0].state.at4.loc.warp == 1`) it spawns the room's event task and clears the three
/// progression flags; otherwise it just asks the stage for area 1. Either way
/// it advances to state 1 and raises the `D_80115598` flag.
static void func_dryfield_gas_station_8017FEDC(Task* arg0)
{
    arg0->msgTable = D_dryfield_gas_station_80181E54;
    Game_SetPtrSlot(arg0, 7);
    if (Mc_SaveData[0].state.at4.loc.warp == 1) {
        Task_SpawnFromTable(D_dryfield_gas_station_80181E3C, 0, 0, 0);
        GameFlag_SetNibble(0x7A, 2);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 0);
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
