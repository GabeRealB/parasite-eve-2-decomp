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

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_cutscene.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

#include "../../shared/room/telephone.h"

static s32 _shelter1fTentRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 secondArg);
static s32 _shelter1fTentResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelter1fTentHandleRoomCommand(Task* task, s32 messageId, s32 commandId, s32 unusedSecondArg);
static s32 _shelter1fTentHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);

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

/// Tent handlers for exits, key-item refusal, soldier conversations and scene commands.
///
/// Four handlers and a `TASK_MESSAGE_TABLE_END` record; the room task borrows
/// this read-only table and its callbacks while the overlay remains loaded.
static const TaskMessageEntry _gShelter1fTentRoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelter1fTentResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelter1fTentRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelter1fTentHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelter1fTentHandleRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _shelter1fTentInitializeRoom(Task* task);
static void _shelter1fTentRoomIdleState(Task* task);

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x0C\x2D"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void shelter1fTentTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

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
    task->msgTable = _gShelter1fTentRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.variant == SOLDIER_ROOM_VARIANT) {
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
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, ARRIVAL_OBJECTIVE);
        playerStateRestoreFullHpMp();
        areaApplySavedUpdates(gShelter1fTentFirstArrivalAreaUpdates);
        evsStartScriptWithSkip(gActor460200TentArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor460200TentArrivalSkipScript);
        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) != 0) {
            areaApplySavedUpdates(gShelter1fTentHeliportAfterItemFollowupAreaUpdates);
            areaApplySavedUpdates(gShelter1fTentGolemFreezer1AfterItemFollowupAreaUpdates);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, ARRIVAL_FOLLOWUP_DIALOGUE);
        } else {
            areaApplySavedUpdates(gShelter1fTentHeliportBeforeItemFollowupAreaUpdates);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, ARRIVAL_DIALOGUE);
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

#include "../../shared/room/room_cutscene_sound_task.inc.c"

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
    gShelter1fTentReplayCutscene.view            = CUTSCENE_VIEW;
    gShelter1fTentReplayCutscene.capSlot         = capSlot;
    gShelter1fTentReplayCutscene.capFile         = capSlot;
    gShelter1fTentReplayCutscene.skipScene       = CUTSCENE_PLAY;
    gShelter1fTentReplayCutscene.startSound      = CUTSCENE_START_SOUND;
    gShelter1fTentReplayCutscene.endSound        = CUTSCENE_END_SOUND;
    gShelter1fTentReplayCutscene.sceneSound      = CUTSCENE_SCENE_SOUND;
    gShelter1fTentReplayCutscene.afterSceneSound = CUTSCENE_AFTER_SCENE_SOUND;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, CUTSCENE_RUNNER_TASK, CUTSCENE_FOLLOWUP_COMMAND, &gShelter1fTentReplayCutscene);
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

/// Tent room states: 0 initialize messages/first-arrival setup, 1 idle, 2 release.
///
/// The dispatcher copies all three callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check; keep the room loaded through dispatch.
static const TaskFuncTable3 _gShelter1fTentRoomStateHandlers = {
    {
        _shelter1fTentInitializeRoom,
        _shelter1fTentRoomIdleState,
        taskKill,
    },
};

void shelter1fTentRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelter1fTentRoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
