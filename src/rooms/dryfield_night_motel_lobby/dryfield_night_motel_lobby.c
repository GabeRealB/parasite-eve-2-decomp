#include "rooms/dryfield_night_motel_lobby.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_night_motel_lobby_private.h"

#include "gameplay/action_prompt.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "overlay.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

void func_dryfield_night_motel_lobby_8017FD10(Task* task);

extern UiObjectDesc D_800611E4;

/// Saved `Mc_SaveData[0].state.at4.loc.view` (area id), restored when the cutscene ends.

/// `Mc_SaveData[0].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `Mc_SaveData` address with case 0.

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_night_motel_lobby_8017D748` draws.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// The " times" suffix appended to that panel's count rows.
static u8 Telephone_Data_80181A70[];

/// Help texts of the panel's nine rows, handed to the UI holder for the
/// selected row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// The usage panel's row list.
static UiList Telephone_Data_80181C6C;

static const char Telephone_Data_8017D638[];

/// The telephone menu's entry list.
static UiList Telephone_Data_80181CF4;

/// The "Play Data" panel's row list.
static UiList Telephone_Data_80181C44;

/// Labels the four menu-entry handlers `func_dryfield_night_motel_lobby_8017F18C`
/// to `func_dryfield_night_motel_lobby_8017F400` draw: "Save", "Play Data",
/// "Weapon Data" and "PE Data".
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// UI descriptors the "Play Data" entry and the two usage entries open.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// The room's task descriptor table: entry 0 is the cap (cutscene) task,
/// entry 1 the sound-event task it runs alongside.
extern TaskDesc D_dryfield_night_motel_lobby_801827A8[];

/// The room's message table, which `func_dryfield_night_motel_lobby_8017FD9C`
/// installs on its task.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, GpMsg13EF*);
        s32 (*call2)(s32, s32, RoomEventMsg*, RoomEventMsg*);
        s32 (*call3)(s32, s32, s32);
    } handler;
} DryfieldNightMotelLobbyMessageEntry;
STATIC_ASSERT_SIZEOF(DryfieldNightMotelLobbyMessageEntry, 8);

extern DryfieldNightMotelLobbyMessageEntry D_dryfield_night_motel_lobby_801827CC[6];

extern TaskDesc D_dryfield_night_motel_lobby_801827FC[];

/// The "%" suffix appended to the percentages the play-data panels print.
static u8 Telephone_Data_80181A78[];

/// UI descriptor the "Play Data" panel and the usage panel spawn when they
/// first open.
static UiObjectDesc Telephone_Data_80181C90;

#define TELEPHONE_TITLE_BYTES "Telephone\0\1\x0E"
#include "../../shared/telephone.h"

void func_dryfield_night_motel_lobby_8017F504(Task*);

void func_dryfield_night_motel_lobby_8017F504(Task*);
void func_dryfield_night_motel_lobby_8017FA70(Task*);

#include "../../shared/telephone_data.inc.c"

void func_dryfield_night_motel_lobby_8017F504(Task*);
void func_dryfield_night_motel_lobby_8017FA70(Task*);

TaskDesc D_dryfield_night_motel_lobby_801827A8[3] = {
    { 0, 32, func_dryfield_night_motel_lobby_8017F504, { .model = NULL } },
    { 0, 32, func_dryfield_night_motel_lobby_8017FA70, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 func_dryfield_night_motel_lobby_8017FB00(void);
s32 func_dryfield_night_motel_lobby_8017FB08(s32, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_motel_lobby_8017FB7C(s32, s32, s32);
s32 func_dryfield_night_motel_lobby_8017FC6C(Task*, s32, GpMsg13EF*);
s32 func_dryfield_night_motel_lobby_8017FCDC(s32, s32, s32);

DryfieldNightMotelLobbyMessageEntry D_dryfield_night_motel_lobby_801827CC[6] = {
    { 5102, { .call2 = func_dryfield_night_motel_lobby_8017FB08 } },
    { 5105, { .call0 = func_dryfield_night_motel_lobby_8017FB00 } },
    { 5103, { .call1 = func_dryfield_night_motel_lobby_8017FC6C } },
    { 5104, { .call3 = func_dryfield_night_motel_lobby_8017FB7C } },
    { 5106, { .call3 = func_dryfield_night_motel_lobby_8017FCDC } },
    { 2147483647, { .call0 = NULL } },
};

TaskDesc D_dryfield_night_motel_lobby_801827FC[2] = {
    { 0, 32, func_dryfield_night_motel_lobby_8017FD10, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

static void func_dryfield_night_motel_lobby_8017FD9C(Task* task);
static void func_dryfield_night_motel_lobby_8017FDE8(Task* task);
static void func_dryfield_night_motel_lobby_80180064(RoomRect* rect, u8 r, u8 g, u8 b);

#include "../../shared/telephone.inc.c"

void func_dryfield_night_motel_lobby_8017EAE0(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

void func_dryfield_night_motel_lobby_8017F504(Task* task)
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
            D_dryfield_night_motel_lobby_801844D0 = NULL;
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
            D_dryfield_night_motel_lobby_801844D0 =
                Task_SpawnFromTable(D_dryfield_night_motel_lobby_801827A8, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_dryfield_night_motel_lobby_801844D0);
                task->state++;
            } else if (Task_PollKill(D_dryfield_night_motel_lobby_801844D0, &poll) != 0) {
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

void func_dryfield_night_motel_lobby_8017FA70(Task* task)
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

s32 func_dryfield_night_motel_lobby_8017FB00(void)
{
    return 0;
}

/// Message handler: copies the incoming message onto the outgoing one and, for
/// message 0xF with `field_5` clear, answers in `field_3` with game-flag nibble
/// 0x61 plus one. Always returns 1.
s32 func_dryfield_night_motel_lobby_8017FB08(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->prefix.packed == 0xF && in->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    return 1;
}

/// Message handler for the lobby's `arg2 == 3` event: on the first visit it
/// latches the visit flag and starts the scene, otherwise it fills in the cap
/// script and spawns the cutscene task.
s32 func_dryfield_night_motel_lobby_8017FB7C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        if (GameFlag_GetNibble(0x16D) == 0) {
            GameFlag_SetNibble(0x16D, 1);
            Gp_RunCapCmd1(0xA);
            return 0;
        }
        D_dryfield_night_motel_lobby_801844E0.field_0 = 5;
        D_dryfield_night_motel_lobby_801844E0.field_1 = 1;
        if (GameFlag_GetNibble(0x7A) < 4) {
            D_dryfield_night_motel_lobby_801844E0.field_14 = 0x380;
            D_dryfield_night_motel_lobby_801844E0.field_3  = 1;
        } else {
            D_dryfield_night_motel_lobby_801844E0.field_14 = 0x3C0;
            D_dryfield_night_motel_lobby_801844E0.field_3  = 2;
        }
        D_dryfield_night_motel_lobby_801844E0.field_2  = 0;
        D_dryfield_night_motel_lobby_801844E0.field_4  = 0x53110003;
        D_dryfield_night_motel_lobby_801844E0.field_8  = 0x53110004;
        D_dryfield_night_motel_lobby_801844E0.field_10 = 0x53110005;
        D_dryfield_night_motel_lobby_801844E0.field_C  = 0x53110006;
        Task_SpawnFromTable(D_dryfield_night_motel_lobby_801827A8, 0, 4, &D_dryfield_night_motel_lobby_801844E0);
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FC6C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x74) == 0) {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Task_SpawnFromTable(D_dryfield_night_motel_lobby_801827FC, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(8);
        }
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FCDC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        Gp_EnqueueStageSnd6(0x5311000A, 0, 0);
    }
    return 0;
}

void func_dryfield_night_motel_lobby_8017FD10(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_dryfield_night_motel_lobby_801844CC = Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801828D4, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_night_motel_lobby_801844CC, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

static void func_dryfield_night_motel_lobby_8017FD9C(Task* task)
{
    task->msgTable = D_dryfield_night_motel_lobby_801827CC;
    Game_SetPtrSlot(task, 7);
    D_dryfield_night_motel_lobby_801844D4 = 1;
    task->state                           = (s32)(task->state + 1);
}

static void func_dryfield_night_motel_lobby_8017FDE8(Task* task)
{
    s32 temp_v0;

    temp_v0 = Gp_HasCollectedBit(0x113);
    if ((temp_v0 != 0) && (D_dryfield_night_motel_lobby_801844D4 == 0)) {
        func_800E3FAC(0xA2, 0x14);
    }
    D_dryfield_night_motel_lobby_801844D4 = temp_v0;
}

/// The three states of the task `func_dryfield_night_motel_lobby_8017FE38` runs:
/// set-up, the per-frame check, and the kill.
static const TaskFuncTable3 D_dryfield_night_motel_lobby_8017D6A4 = {
    {
        func_dryfield_night_motel_lobby_8017FD9C,
        func_dryfield_night_motel_lobby_8017FDE8,
        taskKill,
    },
};

void func_dryfield_night_motel_lobby_8017FE38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_lobby_8017D6A4;
    sp.funcs[task->state](task);
}

/// Runs one frame of the lobby's examine prompt. A busy cap suspends the whole
/// scan for that frame; otherwise the cursor is hit-tested against the room's
/// hotspot table and a confirm press on a hit hotspot is handed to the keypad
/// (`func_dryfield_night_motel_lobby_80180440`) or, while the prompt is idle,
/// latches the hotspot for the next prompt state. Hotspot id 0xB is the panel
/// the keypad is read from, and a code that checks out ends the sequence in
/// state 6. A cancel press ends it in state 5.
///
/// The two paths that leave early call the cursor draw themselves and return
/// rather than jumping to a shared label: the three identical call-and-epilogue
/// tails are what GCC's cross jumping folds into one, and that fold is what
/// leaves the argument setup standing before the merged call with the branches
/// landing past it. Writing a `goto` there compiles to a different tail.
void func_dryfield_night_motel_lobby_8017FE90(Task* task)
{
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->work;
    OverlayHotspot*   hs     = D_dryfield_night_motel_lobby_80182820;
    RoomActionPrompt* prompt = D_80114D28;

    work->field_7            = 0;
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
    } else {
        prompt->targetId = 0x80;
        if (func_dryfield_night_motel_lobby_80180DE4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = 2;
            if (prompt->buttons.slots[0].state == 2) {
                while (hs->id != -1) {
                    if (hs->hit != 0) {
                        if (work->promptBusy == 0) {
                            prompt->mode     = 0;
                            prompt->targetId = 0;
                            work->field_0    = hs->id;
                            work->promptKind = hs->promptKind;
                            task->state      = 3;
                            func_dryfield_night_motel_lobby_801802A8(task);
                            return;
                        }
                        if (work->field_6 == 0) {
                            if (hs->id == 0xB) {
                                work->field_6 = 1;
                                work->field_7 = 1;
                                SndEvt_EnqueueType6(0x53110007, 0, 0);
                            }
                            break;
                        }
                        func_dryfield_night_motel_lobby_80180440(task, hs->id);
                        if (work->field_8 != 0) {
                            Mc_SaveData[0].state.at4.loc.view = 7;
                            task->state                       = 6;
                            func_dryfield_night_motel_lobby_801802A8(task);
                            return;
                        }
                        break;
                    }
                    hs++;
                }
            }
        } else {
            prompt->mode = 1;
        }
        if (prompt->buttons.slots[1].state == 2) {
            task->state = 5;
        }
    }
    func_dryfield_night_motel_lobby_801802A8(task);
}

/// Outlines `rect` in (`r`, `g`, `b`) with four flat `LINE_F2`s, one per edge
/// of the rectangle from (`x`, `y`) to (`x + w`, `y + h`), each linked into
/// `gGpuCurrentOt[1]`.
static void func_dryfield_night_motel_lobby_80180064(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}
