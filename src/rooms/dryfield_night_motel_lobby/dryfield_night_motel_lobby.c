#include "rooms/dryfield_night_motel_lobby.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "dryfield_night_motel_lobby_private.h"

#include "gameplay/action_prompt.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
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

static void _roomCutsceneSoundTask(Task* task);

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

/// The room's message table, which `_dryfieldNightMotelLobbyInitializeRoomTask`
/// installs on its task.
extern TaskMessageEntry D_dryfield_night_motel_lobby_801827CC[6];

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
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static s32 _dryfieldNightMotelLobbyRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
s32        func_dryfield_night_motel_lobby_8017FB7C(Task*, s32, s32, s32);
s32        func_dryfield_night_motel_lobby_8017FC6C(Task*, s32, const void*, s32);
static s32 _dryfieldNightMotelLobbySoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

TaskMessageEntry D_dryfield_night_motel_lobby_801827CC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantParkingLotMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelLobbyRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_motel_lobby_8017FC6C },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_motel_lobby_8017FB7C },
    { ROOM_MESSAGE_SOUND, _dryfieldNightMotelLobbySoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_motel_lobby_801827FC[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_night_motel_lobby_8017FD10, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static void _dryfieldNightMotelLobbyInitializeRoomTask(Task* task);
static void _dryfieldNightMotelLobbyUpdateMasterkeyObjective(Task* unusedTask);

#include "../../shared/telephone.inc.c"

void dryfieldNightMotelLobbyTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses key-item use in the night motel lobby without changing inventory.
///
/// The item menu supplies the collected-item ID and a zero second payload.
/// All arguments are ignored; the refused reply requests the cannot-use notice.
static s32 _dryfieldNightMotelLobbyRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

#include "../../shared/room_variants_parking_lot.inc.c"

/// Message handler for the lobby's `arg2 == 3` event: on the first visit it
/// latches the visit flag and starts the scene, otherwise it fills in the cap
/// script and spawns the cutscene task.
s32 func_dryfield_night_motel_lobby_8017FB7C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOBBY_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_LOBBY_FIRST_SCENE, 1);
            capRunCommandWithTransition(0xA);
            return 0;
        }
        D_dryfield_night_motel_lobby_801844E0.view    = 5;
        D_dryfield_night_motel_lobby_801844E0.capSlot = 1;
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < 4) {
            D_dryfield_night_motel_lobby_801844E0.capTPageX = 0x380;
            D_dryfield_night_motel_lobby_801844E0.capFile   = 1;
        } else {
            D_dryfield_night_motel_lobby_801844E0.capTPageX = 0x3C0;
            D_dryfield_night_motel_lobby_801844E0.capFile   = 2;
        }
        D_dryfield_night_motel_lobby_801844E0.skipScene       = 0;
        D_dryfield_night_motel_lobby_801844E0.startSound      = 0x53110003;
        D_dryfield_night_motel_lobby_801844E0.endSound        = 0x53110004;
        D_dryfield_night_motel_lobby_801844E0.sceneSound      = 0x53110005;
        D_dryfield_night_motel_lobby_801844E0.afterSceneSound = 0x53110006;
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 4, &D_dryfield_night_motel_lobby_801844E0);
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FC6C(Task* task, s32 msgId, const void* firstArg, s32 secondArg)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 1) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOBBY_EVENT_SEEN) == 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            taskSpawnFromTable(D_dryfield_night_motel_lobby_801827FC, 0, 0, 0);
        } else {
            capRunCommandWithTransition(8);
        }
    }
    return 0;
}

/// Queues the night lobby's area sound for room sound cue 99.
///
/// Cue 99 selects script 10 of the night lobby sound bank, with zero pan and
/// depth offsets. Other cues have no effect. Always returns zero.
static s32 _dryfieldNightMotelLobbySoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg)
{
    enum { LOBBY_SOUND_CUE    = 0x63,
           LOBBY_SOUND_SCRIPT = 0x0A };

    if (cueKey == LOBBY_SOUND_CUE) {
        sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, LOBBY_SOUND_SCRIPT), 0, 0);
    }
    return 0;
}

void func_dryfield_night_motel_lobby_8017FD10(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_dryfield_night_motel_lobby_801844CC = taskSpawnFromTable(&D_dryfield_night_motel_lobby_801828D4, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (taskPollKill(D_dryfield_night_motel_lobby_801844CC, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

/// Registers the lobby's room receiver and initializes its masterkey monitor.
///
/// Requires the live room task in state 0. Seeds the previous collection result
/// to one so a key collected before entry does not advance the objective, then
/// enters monitoring state 1. The borrowed message table lives with the overlay.
static void _dryfieldNightMotelLobbyInitializeRoomTask(Task* task)
{
    enum { DRYFIELD_NIGHT_MOTEL_LOBBY_MASTERKEY_COLLECTED = 1 };

    task->msgTable = D_dryfield_night_motel_lobby_801827CC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    D_dryfield_night_motel_lobby_801844D4 = DRYFIELD_NIGHT_MOTEL_LOBBY_MASTERKEY_COLLECTED;
    task->state                           = task->state + 1;
}

/// Updates the objective when the Bronco masterkey collection bit turns on.
///
/// Tests the persistent collection bit, rather than current item quantity.
/// The room's initial cached value of one suppresses an already-collected key;
/// each tick replaces that cache with the current zero-or-one collection result.
static void _dryfieldNightMotelLobbyUpdateMasterkeyObjective(Task* unusedTask)
{
    enum { MASTERKEY_COLLECTED_OBJECTIVE = 0x14 };
    s32 masterkeyCollected;

    masterkeyCollected = inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY);
    if ((masterkeyCollected != 0) && (D_dryfield_night_motel_lobby_801844D4 == 0)) {
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, MASTERKEY_COLLECTED_OBJECTIVE);
    }
    D_dryfield_night_motel_lobby_801844D4 = masterkeyCollected;
}

/// The three states of the task `dryfieldNightMotelLobbyRoomTask` runs:
/// set-up, the per-frame check, and the kill.
static const TaskFuncTable3 D_dryfield_night_motel_lobby_8017D6A4 = {
    {
        _dryfieldNightMotelLobbyInitializeRoomTask,
        _dryfieldNightMotelLobbyUpdateMasterkeyObjective,
        taskKill,
    },
};

void dryfieldNightMotelLobbyRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_motel_lobby_8017D6A4;
    stateHandlers.funcs[task->state](task);
}

/// Hides the cursor and records a confirmed key for the cash-register prompt.
///
/// Borrows the cash-register task, its work, port 0's cursor and a confirmed
/// hotspot. The hotspot ID is a key in 0..13; prompt kind selects the first
/// menu row (0 Examine, 1 Push). Copies those selectors without retaining the
/// hotspot and selects state 3, which opens the menu. Leaves input latches intact.
static inline void _dryfieldNightMotelLobbyCashRegisterLatchHotspot(Task* task, DryfieldNightMotelLobbyCashRegisterWork* work, ActionPrompt* prompt, const ActionPromptHotspot* hotspot)
{
    enum { CASH_REGISTER_STATE_OPEN_PROMPT = 3 };

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    work->hotspotId     = hotspot->id;
    work->promptKind    = hotspot->promptKind;
    task->state         = CASH_REGISTER_STATE_OPEN_PROMPT;
}

void dryfieldNightMotelLobbyCashRegisterScanTask(Task* task)
{
    enum { CASH_REGISTER_STATE_CANCEL      = 5,
           CASH_REGISTER_STATE_ACCEPT_CODE = 6,
           CASH_REGISTER_ACCEPTED_VIEW     = 7,
           CASH_REGISTER_CONFIRM_SLOT      = 0,
           CASH_REGISTER_CANCEL_SLOT       = 1 };
    DryfieldNightMotelLobbyCashRegisterWork* work    = task->work;
    ActionPromptHotspot*                     hotspot = D_dryfield_night_motel_lobby_80182820;
    ActionPrompt*                            prompt  = D_80114D28;

    // A key clear belongs to one drawn frame; reset its latch before scanning.
    work->entryCleared       = 0;
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    } else {
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
        if (actionPromptHitTest(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
            if (prompt->buttons.slots[CASH_REGISTER_CONFIRM_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
                while (hotspot->id != ACTION_PROMPT_HOTSPOT_END) {
                    if (hotspot->hit != 0) {
                        if (work->examined == 0) {
                            _dryfieldNightMotelLobbyCashRegisterLatchHotspot(task, work, prompt, hotspot);
                            dryfieldNightMotelLobbyDrawCashRegisterDisplay(task);
                            return;
                        }
                        if (work->entryOpen == 0) {
                            if (hotspot->id == DRYFIELD_NIGHT_MOTEL_LOBBY_CASH_REGISTER_KEY_HASH) {
                                work->entryOpen    = 1;
                                work->entryCleared = 1;
                                sndEvtRequestScriptStart(SOUND_NIGHT_MOTEL_LOBBY_KEYPAD_PRESS, 0, 0);
                            }
                            break;
                        }
                        dryfieldNightMotelLobbyPressCashRegisterKey(task, hotspot->id);
                        if (work->codeAccepted != 0) {
                            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = CASH_REGISTER_ACCEPTED_VIEW;
                            task->state                                                = CASH_REGISTER_STATE_ACCEPT_CODE;
                            dryfieldNightMotelLobbyDrawCashRegisterDisplay(task);
                            return;
                        }
                        break;
                    }
                    hotspot++;
                }
            }
        } else {
            prompt->mode = ACTION_PROMPT_MODE_IDLE;
        }
        if (prompt->buttons.slots[CASH_REGISTER_CANCEL_SLOT].state == ACTION_PROMPT_BUTTON_PRESSED) {
            task->state = CASH_REGISTER_STATE_CANCEL;
        }
    }
    dryfieldNightMotelLobbyDrawCashRegisterDisplay(task);
}

#include "../../shared/action_prompt_outline_rect.inc.c"
