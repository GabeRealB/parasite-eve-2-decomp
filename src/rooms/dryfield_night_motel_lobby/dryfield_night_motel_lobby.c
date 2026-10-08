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

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

static void _dryfieldNightMotelLobbyCashRegisterOwnerTask(Task* task);

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
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

static s32 _dryfieldNightMotelLobbyRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightMotelLobbyCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldNightMotelLobbyCashRegisterActionMsg(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _dryfieldNightMotelLobbySoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

TaskMessageEntry D_dryfield_night_motel_lobby_801827CC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantParkingLotMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightMotelLobbyRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightMotelLobbyCashRegisterActionMsg },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightMotelLobbyCommandMsg },
    { ROOM_MESSAGE_SOUND, _dryfieldNightMotelLobbySoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_night_motel_lobby_801827FC[2] = {
    { { { TASK_BODY_NONE, 32 } }, _dryfieldNightMotelLobbyCashRegisterOwnerTask, { .value = 0 } },
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

/// Prepares the persistent lobby revisit cutscene for its chapter's CAP file.
///
/// Writes the borrowed playback record before the runner is spawned; the record
/// must remain live through playback. Texture-page X uses VRAM word coordinates.
static inline void _dryfieldNightMotelLobbyPrepareRevisitCutscene(void)
{
    enum { REVISIT_VIEW          = 5,
           REVISIT_CAP_SLOT      = 1,
           REVISIT_CHAPTER_SPLIT = 4,
           EARLY_CAP_TPAGE_X     = 896,
           LATE_CAP_TPAGE_X      = 960,
           EARLY_CAP_FILE        = 1,
           LATE_CAP_FILE         = 2 };
    D_dryfield_night_motel_lobby_801844E0.view    = REVISIT_VIEW;
    D_dryfield_night_motel_lobby_801844E0.capSlot = REVISIT_CAP_SLOT;
    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < REVISIT_CHAPTER_SPLIT) {
        D_dryfield_night_motel_lobby_801844E0.capTPageX = EARLY_CAP_TPAGE_X;
        D_dryfield_night_motel_lobby_801844E0.capFile   = EARLY_CAP_FILE;
    } else {
        D_dryfield_night_motel_lobby_801844E0.capTPageX = LATE_CAP_TPAGE_X;
        D_dryfield_night_motel_lobby_801844E0.capFile   = LATE_CAP_FILE;
    }
    D_dryfield_night_motel_lobby_801844E0.skipScene       = 0;
    D_dryfield_night_motel_lobby_801844E0.startSound      = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 3);
    D_dryfield_night_motel_lobby_801844E0.endSound        = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 4);
    D_dryfield_night_motel_lobby_801844E0.sceneSound      = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 5);
    D_dryfield_night_motel_lobby_801844E0.afterSceneSound = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY, 6);
}

/// Starts first-visit or chapter-dependent revisit playback for lobby command 3.
///
/// Handles `ROOM_MESSAGE_COMMAND`. The first visit latches its saved flag before
/// requesting CAP command 10. Later visits configure the overlay's persistent
/// cutscene record and spawn its runner, which borrows that record. Other commands
/// do nothing. The receiver, message ID and second payload are unused; returns zero.
static s32 _dryfieldNightMotelLobbyCommandMsg(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        VISIT_COMMAND           = 3,
        FIRST_VISIT_CAP_COMMAND = 10,
        CUTSCENE_PRIORITY       = 4,
    };
    if (commandId == VISIT_COMMAND) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOBBY_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_LOBBY_FIRST_SCENE, true);
            capRunCommandWithTransition(FIRST_VISIT_CAP_COMMAND);
            return 0;
        }
        // Revisit playback selects the CAP file for the current story chapter.
        _dryfieldNightMotelLobbyPrepareRevisitCutscene();
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, CUTSCENE_PRIORITY, &D_dryfield_night_motel_lobby_801844E0);
    }
    return 0;
}

/// Opens the cash-register interaction for lobby direction action 1.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a non-NULL borrowed request.
/// Before code acceptance, holds player control, hides the model and spawns the
/// cash-register owner task. Afterwards requests CAP command 8. Ignores the
/// receiver, message ID and second payload, retains no request and returns zero.
static s32 _dryfieldNightMotelLobbyCashRegisterActionMsg(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { CASH_REGISTER_ACTION           = 1,
           COMPLETED_REGISTER_CAP_COMMAND = 8 };

    if (request->actionId == CASH_REGISTER_ACTION) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_LOBBY_EVENT_SEEN) == 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            taskSpawnFromTable(D_dryfield_night_motel_lobby_801827FC, 0, 0, 0);
        } else {
            capRunCommandWithTransition(COMPLETED_REGISTER_CAP_COMMAND);
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

/// Owns the cash-register interaction task until its requested exit is dispatched.
///
/// State 0 spawns the controller and retains its task handle; state 1 polls that
/// live child, ignoring its result, then releases this bodyless owner. Requires
/// successful spawning and the lobby callbacks to stay loaded through polling.
/// The stored child handle is left unchanged after exit.
static void _dryfieldNightMotelLobbyCashRegisterOwnerTask(Task* task)
{
    enum { SPAWN_REGISTER = 0,
           WAIT_REGISTER  = 1 };
    s32 childResult;

    switch (task->state) {
        case SPAWN_REGISTER:
            D_dryfield_night_motel_lobby_801844CC = taskSpawnFromTable(&D_dryfield_night_motel_lobby_801828D4, 0, 0, 0);
            task->state++;
            return;
        case WAIT_REGISTER:
            if (taskPollKill(D_dryfield_night_motel_lobby_801844CC, &childResult) != 0) {
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
