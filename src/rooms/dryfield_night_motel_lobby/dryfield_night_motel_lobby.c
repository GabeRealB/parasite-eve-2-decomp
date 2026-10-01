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
#include "../../shared/action_prompt.h"
#include "../../shared/room_cutscene.h"
#include "../../shared/room_variants.h"

void func_dryfield_night_motel_lobby_8017FD10(Task* task);

extern UiObjectDesc D_800611E4;

/// Saved `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` (area id), restored when the cutscene ends.

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `gMcSaveData` address with case 0.

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
extern TaskDesc gRoomCutsceneTaskDescs[];

/// The room's message table, which `func_dryfield_night_motel_lobby_8017FD9C`
/// installs on its task.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, DirectionActionRequest*);
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

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 func_dryfield_night_motel_lobby_8017FB00(void);
s32 func_dryfield_night_motel_lobby_8017FB7C(s32, s32, s32);
s32 func_dryfield_night_motel_lobby_8017FC6C(Task*, s32, DirectionActionRequest* request);
s32 func_dryfield_night_motel_lobby_8017FCDC(s32, s32, s32);

DryfieldNightMotelLobbyMessageEntry D_dryfield_night_motel_lobby_801827CC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, { .call2 = roomVariantParkingLotMsg } },
    { 5105, { .call0 = func_dryfield_night_motel_lobby_8017FB00 } },
    { DIRECTION_MESSAGE_ROOM_ACTION, { .call1 = func_dryfield_night_motel_lobby_8017FC6C } },
    { 5104, { .call3 = func_dryfield_night_motel_lobby_8017FB7C } },
    { 5106, { .call3 = func_dryfield_night_motel_lobby_8017FCDC } },
    { 2147483647, { .call0 = NULL } },
};

TaskDesc D_dryfield_night_motel_lobby_801827FC[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_night_motel_lobby_8017FD10, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static void func_dryfield_night_motel_lobby_8017FD9C(Task* task);
static void func_dryfield_night_motel_lobby_8017FDE8(Task* task);

#include "../../shared/telephone.inc.c"

void func_dryfield_night_motel_lobby_8017EAE0(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_cutscene_sound_task.inc.c"

s32 func_dryfield_night_motel_lobby_8017FB00(void)
{
    return 0;
}

#include "../../shared/room_variants_parking_lot.inc.c"

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
        Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, 4, &D_dryfield_night_motel_lobby_801844E0);
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FC6C(Task* task, s32 msgId, DirectionActionRequest* request)
{
    if (request->actionId == 1) {
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
        if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
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
                            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 7;
                            task->state                                                = 6;
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

#include "../../shared/action_prompt_outline_rect.inc.c"
