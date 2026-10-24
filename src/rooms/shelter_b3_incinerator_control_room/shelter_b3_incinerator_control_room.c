#include "rooms/shelter_b3_incinerator_control_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "shelter_b3_incinerator_control_room_private.h"

#include "actors/actor_142600.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
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

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_cutscene.h"

static void _roomCutsceneTask(Task* task);

static void _roomCutsceneSoundTask(Task* task);

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

#include "../../shared/room/telephone.h"

static s32 _shelterB3IncineratorControlRoomRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _shelterB3IncineratorControlRoomResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB3IncineratorControlRoomHandleCommand(Task* task, s32 messageId, s32 command, s32 unusedArg);
static s32 _shelterB3IncineratorControlRoomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);
static s32 _shelterB3IncineratorControlRoomHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused);

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

/// Synchronous message interface for the initialized control-room task.
///
/// Resolves room exits, refuses key items, ignores room actions, and handles
/// telephone commands and sound cues. The sixth entry ends lookup, so unsupported
/// IDs return zero. `Task::msgTable` borrows this table and its callbacks while
/// the task is live; pointer payloads are borrowed only during dispatch.
static TaskMessageEntry _gShelterB3IncineratorControlRoomMessageTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB3IncineratorControlRoomResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB3IncineratorControlRoomRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB3IncineratorControlRoomIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB3IncineratorControlRoomHandleCommand },
    { ROOM_MESSAGE_SOUND, _shelterB3IncineratorControlRoomHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _shelterB3IncineratorControlRoomInitializeRoom(Task* task);
static void _shelterB3IncineratorControlRoomIdleRoom(Task* task);

/// Retained bytes following this room's telephone menu title terminator.
///
/// Two-byte string-literal binding for `telephone.inc.c`; unused by label drawing.
#define TELEPHONE_MENU_TITLE_TRAILING_BYTES "\x14\xCF"
#include "../../shared/room/telephone.inc.c"
#undef TELEPHONE_MENU_TITLE_TRAILING_BYTES

void shelterB3IncineratorControlRoomTelephoneMenuTask(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/room/telephone_panels.inc.c"

#include "../../shared/room/room_cutscene_task.inc.c"

#include "../../shared/room/room_cutscene_sound_task.inc.c"

/// Refuses key-item use with the inventory menu's refused reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored and the
/// room state remains unchanged.
static s32 _shelterB3IncineratorControlRoomRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Blocks the elevator-hall exit until the control-room door is unlocked.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with borrowed eight-byte request/reply
/// records, which may alias. Copies and resolves the destination first. Returns
/// 1 for another destination or an unlocked door, otherwise 0. Only execution
/// writes the optional flag to 2 and starts the blocked-door CAP command.
static s32 _shelterB3IncineratorControlRoomResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_EXIT_BLOCKED             = 0,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_EXIT_ALLOWED             = 1,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_BLOCKED_DOOR_FLAG_VALUE  = 2,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_BLOCKED_DOOR_CAP_COMMAND = 3,
    };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId != GAME_AREA_SHELTER_B3_ELEVATOR_HALL) {
        return SHELTER_B3_INCINERATOR_CONTROL_ROOM_EXIT_ALLOWED;
    }
    if (gameFlagGetNibble(GAME_FLAG_B3_INCINERATOR_CONTROL_DOOR_UNLOCKED) != 0) {
        return SHELTER_B3_INCINERATOR_CONTROL_ROOM_EXIT_ALLOWED;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return SHELTER_B3_INCINERATOR_CONTROL_ROOM_EXIT_BLOCKED;
    }
    gameFlagSetNibbleIfPresent(request->flagId, SHELTER_B3_INCINERATOR_CONTROL_ROOM_BLOCKED_DOOR_FLAG_VALUE);
    capRunCommandWithTransition(SHELTER_B3_INCINERATOR_CONTROL_ROOM_BLOCKED_DOOR_CAP_COMMAND);
    return SHELTER_B3_INCINERATOR_CONTROL_ROOM_EXIT_BLOCKED;
}

/// Configures and starts the repeat-use control-panel scene in view 8.
///
/// capSlot selects the CAP slot, loaded file and saved arrival warp; the sole
/// caller supplies 1. Slot/file values narrow to signed bytes and warp to an
/// unsigned byte. The room-owned record and resources must remain loaded and
/// unchanged through playback. Completion runs CAP command 7.
static inline void _shelterB3IncineratorControlRoomStartPanelScene(s32 capSlot)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_TASK              = 0,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_PLAY_SCENE        = 0,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_VIEW              = 8,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_FOLLOW_UP_COMMAND = 7,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_START_SOUND       = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 1),
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_END_SOUND         = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 4),
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_SCENE_SOUND       = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 2),
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_AFTER_SCENE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, 3),
    };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp            = capSlot;
    gShelterB3IncineratorControlRoomTelephoneCutscene.view            = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_VIEW;
    gShelterB3IncineratorControlRoomTelephoneCutscene.capSlot         = capSlot;
    gShelterB3IncineratorControlRoomTelephoneCutscene.capFile         = capSlot;
    gShelterB3IncineratorControlRoomTelephoneCutscene.skipScene       = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_PLAY_SCENE;
    gShelterB3IncineratorControlRoomTelephoneCutscene.startSound      = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_START_SOUND;
    gShelterB3IncineratorControlRoomTelephoneCutscene.endSound        = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_END_SOUND;
    gShelterB3IncineratorControlRoomTelephoneCutscene.sceneSound      = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_SCENE_SOUND;
    gShelterB3IncineratorControlRoomTelephoneCutscene.afterSceneSound = SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_AFTER_SCENE_SOUND;
    taskSpawnFromTable(_gRoomCutsceneTaskDescs, SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_TASK, SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_FOLLOW_UP_COMMAND, &gShelterB3IncineratorControlRoomTelephoneCutscene);
}

/// Handles control-panel command 1, separating first-use dialogue from repeat-use scenes.
///
/// `ROOM_MESSAGE_COMMAND` supplies an integer command and unused second word.
/// The first use sets its progress flag and runs CAP command 6. Later uses
/// start the view-8 cutscene with CAP slot/file 1; its room-owned record must
/// remain live through playback. Other commands do nothing; returns zero.
static s32 _shelterB3IncineratorControlRoomHandleCommand(Task* task, s32 messageId, s32 command, s32 unusedArg)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_FIRST_USE_CAP_COMMAND = 6,
    };

    if (command == SHELTER_B3_INCINERATOR_CONTROL_ROOM_PANEL_COMMAND) {
        if (gameFlagGetNibble(GAME_FLAG_INCINERATOR_CONTROL_FIRST_USE) != 0) {
            _shelterB3IncineratorControlRoomStartPanelScene(command);
        } else {
            gameFlagSetNibble(GAME_FLAG_INCINERATOR_CONTROL_FIRST_USE, 1);
            capSpawnEventIfIdle(SHELTER_B3_INCINERATOR_CONTROL_ROOM_FIRST_USE_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
        }
    }
    return 0;
}

/// Ignores direction-triggered room actions and returns zero.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; the borrowed four-byte request
/// and unused second word are neither read nor retained.
static s32 _shelterB3IncineratorControlRoomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    return 0;
}

/// Maps room sound cue 99 to control-room sound-bank entry 9.
///
/// Handles `ROOM_MESSAGE_SOUND` with an integer cue and unused second word.
/// Requires the room sound bank loaded. Other cues do nothing; returns zero.
static s32 _shelterB3IncineratorControlRoomHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_CUE_99  = 99,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_ENTRY_9 = 9
    };
    if (cueId == SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_CUE_99) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM, SHELTER_B3_INCINERATOR_CONTROL_ROOM_SOUND_ENTRY_9), 0, 0);
    }
    return 0;
}

/// Installs the room message interface and applies the incinerator arrival scene.
///
/// State 0 registers the room slot and advances to idle. Arrival warp 4 updates
/// the objective and saved areas, restores companion HP and starts the skippable
/// actor-142600 scene. Other arrivals only install the message interface.
static void _shelterB3IncineratorControlRoomInitializeRoom(Task* task)
{
    enum {
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_INCINERATOR_ARRIVAL = 4,
        SHELTER_B3_INCINERATOR_CONTROL_ROOM_ARRIVAL_OBJECTIVE   = 35,
    };

    task->msgTable = _gShelterB3IncineratorControlRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
    if (gGameSession->location.warp == SHELTER_B3_INCINERATOR_CONTROL_ROOM_INCINERATOR_ARRIVAL) {
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B3_INCINERATOR_CONTROL_ROOM_ARRIVAL_OBJECTIVE);
        areaApplySavedUpdates(gShelterB3IncineratorControlRoomArrivalSavedUpdates);
        companionRestoreFullHp();
        evsStartScriptWithSkip(gActor142600IncineratorArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor142600IncineratorArrivalSkipScript);
    }
}

/// Leaves the initialized room task idle while its message table remains active.
///
/// The room controller's state 1 performs no per-frame work or state change.
static void _shelterB3IncineratorControlRoomIdleRoom(Task* task)
{
    // Retained unused storage preserves the original 16-byte stack frame.
    char unusedStackBytes[0x10];
}

/// State handlers for room setup, message-service idle and task release.
///
/// Indexed by task state 0..2 without a bounds check or terminator. Dispatch
/// copies all three callback pointers to the stack before selecting one;
/// keep the overlay loaded while the room task is live.
static const TaskFuncTable3 _gShelterB3IncineratorControlRoomStateHandlers = {
    {
        _shelterB3IncineratorControlRoomInitializeRoom,
        _shelterB3IncineratorControlRoomIdleRoom,
        taskKill,
    },
};

void shelterB3IncineratorControlRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gShelterB3IncineratorControlRoomStateHandlers;
    handlers.funcs[task->state](task);
}
