#include "rooms/shelter_1f_tent.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "shelter_1f_tent_private.h"

#include "actors/actor_460200.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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

#include "mapui/map_neo_ark.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_cutscene.h"

static void _roomCutsceneSoundTask(Task* task);

extern UiObjectDesc D_800611E4;

extern EvsCommand D_actor_460200_801362B8[];
extern EvsCommand D_actor_460200_80137890[];

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row captions of the play-data panel, one per row.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
static u8 Telephone_Data_80181A70[];

/// Suffix appended after a percentage.
static u8 Telephone_Data_80181A78[];

/// Help strings handed to the UI holder while the cursor rests on a row of
/// the play-data panel, one per row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Lists of the play-data panel, the usage panel and the telephone menu, and
/// the descriptors of the panels they spawn.
static UiList       Telephone_Data_80181C44;
static UiList       Telephone_Data_80181C6C;
static UiObjectDesc Telephone_Data_80181C90;
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;
static UiList       Telephone_Data_80181CF4;

/// Task table the cutscene and its sound task are spawned from.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// Message table of the room's message task.
extern TaskMessageEntry D_shelter_1f_tent_80181CDC[];

#define TELEPHONE_TITLE_BYTES "Telephone\0\x0C-"
#include "../../shared/telephone.h"

static s32 _shelter1fTentRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32 _shelter1fTentResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelter1fTentHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32 _shelter1fTentHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, _roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_shelter_1f_tent_80181CDC[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fTentResolveRoomEvent },
    { 5105, _shelter1fTentRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fTentHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fTentHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _shelter1fTentInitializeRoom(Task* task);
static void _shelter1fTentRoomIdleState(Task* task);

#include "../../shared/telephone.inc.c"

void shelter1fTentTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// Installs the tent receiver and applies first-arrival story, map and actor setup.
///
/// Called in state 0 with the room, actor-460200 and session resources loaded.
/// Variant 1 creates the soldiers. First arrival restores HP/MP, applies saved
/// area updates and starts the soldier event script; later visits start the two
/// ambience scripts. Publishes the borrowed room task and advances to idle state 1.
static void _shelter1fTentInitializeRoom(Task* task)
{
    enum {
        SOLDIER_ROOM_VARIANT            = 1,
        FIRST_ARRIVAL_RECORDED          = 1,
        MAP_MARK_SHOWN                  = 2,
        ARRIVAL_OBJECTIVE               = 0x36,
        ARRIVAL_DIALOGUE                = 10,
        ARRIVAL_FOLLOWUP_DIALOGUE       = 11,
        COMPANION_SCHEDULE_AFTER_BURNER = 8
    };
    task->msgTable = D_shelter_1f_tent_80181CDC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.loc.variant == SOLDIER_ROOM_VARIANT) {
        actor460200SetupTentSoldiers();
    }
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_1F_TENT_ARRIVED) == 0) {
        // Seed the arrival's saved progress before starting its actor script.
        gameFlagSetNibble(GAME_FLAG_SHELTER_1F_TENT_ARRIVED, FIRST_ARRIVAL_RECORDED);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1B3, MAP_MARK_SHOWN);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1B0, MAP_MARK_SHOWN);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1AE, MAP_MARK_SHOWN);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1CD, MAP_MARK_SHOWN);
        gameFlagSetNibble(GAME_FLAG_0FC, 0);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_POD, 0);
        gameFlagSetNibble(GAME_FLAG_SHELTER_ELEVATOR_ENABLED, 0);
        gameFlagSetNibble(GAME_FLAG_SHELTER_1F_TENT_1BA, 2);
        gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1BB, MAP_MARK_SHOWN);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ARRIVAL_OBJECTIVE);
        playerStateRestoreFullHpMp();
        areaApplySavedUpdates(D_shelter_1f_tent_801842D4);
        evsStartScriptWithSkip(D_actor_460200_801362B8, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_460200_80137890);
        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) != 0) {
            areaApplySavedUpdates(D_shelter_1f_tent_801843B0);
            areaApplySavedUpdates(D_shelter_1f_tent_801843B8);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ARRIVAL_FOLLOWUP_DIALOGUE);
        } else {
            areaApplySavedUpdates(D_shelter_1f_tent_801843A8);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ARRIVAL_DIALOGUE);
        }
        if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) != 0) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_SCHEDULE_AFTER_BURNER);
        }
    } else {
        sndEvtRequestScriptStart(SOUND_SHELTER_1F_TENT_AMBIENCE_1, 0, 0);
        sndEvtRequestScriptStart(SOUND_SHELTER_1F_TENT_AMBIENCE_2, 0, 0);
    }
    task->state = task->state + 1;
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses every key-item use in the tent with the item menu cannot-use reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`. `itemId` is the selected collected-item
/// ID. All arguments are ignored; no inventory state changes.
static s32 _shelter1fTentRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition after resolving its Neo Ark destination variant.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Borrows a complete eight-byte request
/// and writable reply during synchronous dispatch; they may alias. Copies the
/// request before resolving the reply room. Query mode preserves the selectors.
/// Neither pointer is retained; the map overlay must remain loaded. Always
/// returns 1 (passage allowed); the receiving task and message ID are unused.
static s32 _shelter1fTentResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_1F_TENT_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return SHELTER_1F_TENT_TRANSITION_ALLOWED;
}

/// Configures and starts the tent's replayable room cutscene.
///
/// `capSlot` is 1. The singleton record is borrowed by the spawned runner and
/// must remain unchanged until it finishes; both room and CAP resources stay loaded.
static inline void _shelter1fTentStartReplayCutscene(s32 capSlot)
{
    enum {
        CUTSCENE_RUNNER_TASK       = 0,
        CUTSCENE_VIEW              = 5,
        CUTSCENE_PLAY              = 0,
        CUTSCENE_FOLLOWUP_COMMAND  = 12,
        CUTSCENE_START_SOUND       = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 3),
        CUTSCENE_END_SOUND         = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 6),
        CUTSCENE_SCENE_SOUND       = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 4),
        CUTSCENE_AFTER_SCENE_SOUND = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_TENT, 5)
    };
    D_shelter_1f_tent_801843C4.view            = CUTSCENE_VIEW;
    D_shelter_1f_tent_801843C4.capSlot         = capSlot;
    D_shelter_1f_tent_801843C4.capFile         = capSlot;
    D_shelter_1f_tent_801843C4.skipScene       = CUTSCENE_PLAY;
    D_shelter_1f_tent_801843C4.startSound      = CUTSCENE_START_SOUND;
    D_shelter_1f_tent_801843C4.endSound        = CUTSCENE_END_SOUND;
    D_shelter_1f_tent_801843C4.sceneSound      = CUTSCENE_SCENE_SOUND;
    D_shelter_1f_tent_801843C4.afterSceneSound = CUTSCENE_AFTER_SCENE_SOUND;
    taskSpawnFromTable(gRoomCutsceneTaskDescs, CUTSCENE_RUNNER_TASK, CUTSCENE_FOLLOWUP_COMMAND, &D_shelter_1f_tent_801843C4);
}

/// Handles the tent's scene command, choosing the first scene or its replay.
///
/// `ROOM_MESSAGE_COMMAND` supplies integer command 1. Its first call marks the
/// first scene seen and starts CAP command 24; later calls start CAP slot/file 1
/// in view 5 with follow-up command 12. Other commands do nothing. The receiver,
/// message ID and second word are unused; every path returns zero.
static s32 _shelter1fTentHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg)
{
    enum { COMMAND_SCENE    = 1,
           FIRST_SCENE_SEEN = 1,
           CAP_FIRST_SCENE  = 24 };
    if (commandId == COMMAND_SCENE) {
        if (gameFlagGetNibble(GAME_FLAG_TENT_FIRST_SCENE) == 0) {
            gameFlagSetNibble(GAME_FLAG_TENT_FIRST_SCENE, FIRST_SCENE_SEEN);
            capRunCommandWithTransition(CAP_FIRST_SCENE);
            return 0;
        }
        _shelter1fTentStartReplayCutscene(commandId);
    }
    return 0;
}

/// Routes tent room actions to the two soldiers' conversation handlers.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request synchronously:
/// action 1 talks to soldier C and action 2 to soldier A. Other actions do
/// nothing. The argument byte and zero second word are unused, no pointer is
/// retained, and every action returns zero. Requires the soldiers' resources live.
static s32 _shelter1fTentHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { ACTION_TALK_SOLDIER_C = 1,
           ACTION_TALK_SOLDIER_A = 2 };
    if (request->actionId == ACTION_TALK_SOLDIER_C) {
        actor460200TalkToSoldierC();
    }
    if (request->actionId == ACTION_TALK_SOLDIER_A) {
        actor460200TalkToSoldierA();
    }
    return 0;
}

/// Keeps the tent room task available for messages in state 1.
///
/// Leaves its state and message table intact; room teardown is owned by the caller.
static void _shelter1fTentRoomIdleState(Task* task)
{
    // Retain the target idle callback's otherwise unused stack frame.
    char reservedStack[0x10];
}

/// States of the room's message task, run by
/// `shelter1fTentRoomTask`: install the message table and apply the
/// room's first-visit setup, idle, die.
static const TaskFuncTable3 D_shelter_1f_tent_8017D6A4 = {
    {
        _shelter1fTentInitializeRoom,
        _shelter1fTentRoomIdleState,
        taskKill,
    },
};

void shelter1fTentRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_1f_tent_8017D6A4;
    stateHandlers.funcs[task->state](task);
}
