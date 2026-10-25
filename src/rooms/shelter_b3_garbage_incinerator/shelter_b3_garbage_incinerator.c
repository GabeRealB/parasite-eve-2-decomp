#include "types.h"

#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// Lift task returned by room setup and addressed by the Glutton's carried-move event.
///
/// This borrowed pointer must still denote a live lift when event 0 arrives.
/// Spawning is unchecked and the stored pointer is not cleared on lift teardown;
/// a non-NULL value alone does not establish lifetime.
static Task* _gShelterB3GarbageIncineratorSpawnedLiftTask;

/// Elapsed room-controller ticks since the last blocked exit-switch notice.
///
/// Starts at zero; each idle update increments it up to 61. A blocked switch
/// press at or above that threshold displays its notice and resets the counter.
/// The unsigned halfword and its first-declaration position are retained.
static u16 _gShelterB3GarbageIncineratorSwitchNoticeTicks;

#include "rooms/shelter_b3_garbage_incinerator.h"

#include "shelter_b3_garbage_incinerator_private.h"

#include "actors/actor_444000.h"
#include "actors/actor_342400.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"

#include "mapui/map_shelter.h"

static void _shelterB3GarbageIncineratorInitializeRoom(Task* task);
static void _shelterB3GarbageIncineratorAdvanceSwitchTimer(Task* task);

/// Minimum elapsed controller ticks before another blocked-switch notice.
enum { SHELTER_B3_GARBAGE_INCINERATOR_SWITCH_READY_TICKS = 61 };

/// Room-controller callbacks: 0 initialize, 1 advance the switch timer, 2 release.
///
/// Dispatch copies all three pointers; state must be 0..2. There is no
/// terminator or bounds check; keep the room's callback code loaded.
static const TaskFuncTable3 _gShelterB3GarbageIncineratorRoomTaskStates = { {
    _shelterB3GarbageIncineratorInitializeRoom,
    _shelterB3GarbageIncineratorAdvanceSwitchTimer,
    taskKill,
} };

static void _shelterB3GarbageIncineratorExitToControlRoomTask(Task* task);
static s32  _shelterB3GarbageIncineratorRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _shelterB3GarbageIncineratorResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _shelterB3GarbageIncineratorIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32  _shelterB3GarbageIncineratorHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB3GarbageIncineratorHandleActorEvent(Task* task, s32 messageId, s32 eventId, s32 unusedArg);
static s32  _shelterB3GarbageIncineratorHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused);

/// Room receiver routes for departures, key items, actions, commands, actor events and sounds.
///
/// Six live IDs followed by END/NULL; other IDs return zero. Transition and
/// action records are borrowed synchronously; remaining payloads are integer
/// words. The room receiver borrows this table and its loaded callback code.
static TaskMessageEntry _gShelterB3GarbageIncineratorRoomMessages[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB3GarbageIncineratorResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB3GarbageIncineratorRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB3GarbageIncineratorHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB3GarbageIncineratorIgnoreCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, _shelterB3GarbageIncineratorHandleActorEvent },
    { ROOM_MESSAGE_SOUND, _shelterB3GarbageIncineratorHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless priority-32 recipe for the deferred control-room departure choice.
///
/// Spawn only index 0 with unused zero payloads, after latching the destination.
/// The callback borrows that snapshot and loaded CAP/sound resources through
/// cancellation or reload; spawning retains no pointer to this recipe.
static TaskDesc _gShelterB3GarbageIncineratorControlRoomExitTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _shelterB3GarbageIncineratorExitToControlRoomTask, { .value = 0 } };

u16 gShelterB3GarbageIncineratorDoorEventSuspended;

/// Commits the deferred control-room destination and schedules a captured-frame reload.
///
/// Requires a live departure task and a prepared room-event destination.
/// Copies its area, warp and room into the live save, retaining stage and view.
/// Stops non-ambient scripts, selects sprite variant 1 and kills the departure
/// task after queuing reload; the reload consumes the saved selectors later.
static inline void _shelterB3GarbageIncineratorCommitControlRoomExit(Task* task)
{
    enum { SHELTER_B3_GARBAGE_INCINERATOR_DEFAULT_SPRITE_VARIANT = 1 };

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gDisplayState.spriteVariant                                 = SHELTER_B3_GARBAGE_INCINERATOR_DEFAULT_SPRITE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = gShelterB3GarbageIncineratorControlRoomExitDestination.areaId;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = gShelterB3GarbageIncineratorControlRoomExitDestination.warp;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = gShelterB3GarbageIncineratorControlRoomExitDestination.roomState;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Runs the control-room departure choice, transit sound and deferred reload.
///
/// Starts in state 0 with the destination already copied into room storage.
/// CAP command 18 selects cancellation at key 0; another key commits the exit
/// after the transit sound ends. Cancellation resumes the player and actors.
/// Requires the room's CAP/sound resources and destination storage to remain live.
static void _shelterB3GarbageIncineratorExitToControlRoomTask(Task* task)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_START,
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_WAIT_CAP,
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_CHOOSE,
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_WAIT_SOUND,
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_RELOAD,
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_CAP_COMMAND = 18,
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_CANCEL_KEY  = 0,
    };

    switch (task->state) {
        case SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_CHOOSE:
            if (capGetVariantKey() == SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_CANCEL_KEY) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            } else {
                sndEvtRequestStageScriptStart(SOUND_SHELTER_B3_INCINERATOR_EXIT_TRANSIT, 0, 0);
                task->state++;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_WAIT_SOUND:
            if (sndScriptHasActiveId(SOUND_SHELTER_B3_INCINERATOR_EXIT_TRANSIT) == 0) {
                task->state++;
            }
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_EXIT_RELOAD:
            _shelterB3GarbageIncineratorCommitControlRoomExit(task);
            break;
    }
}

/// Refuses key-item use with the inventory menu's refused reply.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored and the
/// room state remains unchanged.
static s32 _shelterB3GarbageIncineratorRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves exits, deferring a control-room departure behind its CAP choice.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with borrowed eight-byte request/reply
/// records, which may alias. Queries suppress departure effects. Returns 0
/// when blocked, 1 for an immediate exit or 2 for a deferred control-room exit.
/// A deferred exit copies the reply; neither argument pointer is retained.
static s32 _shelterB3GarbageIncineratorResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_EVENT_BLOCKED            = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_EVENT_IMMEDIATE          = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_EVENT_DEFERRED           = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_SECOND_ROOM_GROUP_START  = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_BLOCKED_CAP_COMMAND = 3,
        SHELTER_B3_GARBAGE_INCINERATOR_COMPANION_AFTER_BURNER   = 10,
        SHELTER_B3_GARBAGE_INCINERATOR_COMPANION_BEFORE_BURNER  = 5,
        SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_ROOM_ARRIVAL     = 4,
    };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM) {
        if (request->queryOnly != ROOM_EVENT_EXECUTE) {
            return SHELTER_B3_GARBAGE_INCINERATOR_EVENT_BLOCKED;
        }
        gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
        if (gGameSession->location.roomState < SHELTER_B3_GARBAGE_INCINERATOR_SECOND_ROOM_GROUP_START) {
            capSpawnEventIfIdle(SHELTER_B3_GARBAGE_INCINERATOR_EXIT_BLOCKED_CAP_COMMAND, CAP_EVENT_PAUSE_ACTORS);
            return SHELTER_B3_GARBAGE_INCINERATOR_EVENT_BLOCKED;
        }
        if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) != 0) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, SHELTER_B3_GARBAGE_INCINERATOR_COMPANION_AFTER_BURNER);
        } else {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, SHELTER_B3_GARBAGE_INCINERATOR_COMPANION_BEFORE_BURNER);
        }
        // Preserve the resolved destination until the choice task commits it.
        reply->warp                                            = SHELTER_B3_GARBAGE_INCINERATOR_CONTROL_ROOM_ARRIVAL;
        gShelterB3GarbageIncineratorControlRoomExitDestination = *reply;
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, 1);
        taskSpawnFromTable(&_gShelterB3GarbageIncineratorControlRoomExitTaskDesc, 0, 0, 0);
        return SHELTER_B3_GARBAGE_INCINERATOR_EVENT_DEFERRED;
    }
    if (request->areaId == GAME_AREA_SHELTER_B3_DUMPING_HOLE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        reply->roomState = gGameSession->incineratorRoomGroup + 1;
    }
    return SHELTER_B3_GARBAGE_INCINERATOR_EVENT_IMMEDIATE;
}

/// Ignores room commands and returns zero without changing room state.
///
/// Handles `ROOM_MESSAGE_COMMAND`; neither the command nor its argument is read.
static s32 _shelterB3GarbageIncineratorIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    return 0;
}

/// Accepts exit-switch action 2 or reports that the lift descent is unfinished.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION` with a borrowed four-byte request
/// and unused integer second word. Repeated blocked notices require 61 elapsed
/// controller ticks. A completed descent starts the exit encounter once.
/// Ignores other actions, retains no payload pointer and returns zero.
static s32 _shelterB3GarbageIncineratorHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_SWITCH_ACTION = 2,
    };

    if (request->actionId == SHELTER_B3_GARBAGE_INCINERATOR_EXIT_SWITCH_ACTION && gGameSession->incineratorExitPhase == GAME_SESSION_INCINERATOR_EXIT_NONE) {
        if (gGameSession->incineratorDescentPhase == GAME_SESSION_INCINERATOR_DESCENT_COMPLETE) {
            taskSpawnFromTable(&gShelterB3GarbageIncineratorExitEncounterTaskDesc, 0, 0, 0);
            gGameSession->incineratorExitPhase = GAME_SESSION_INCINERATOR_EXIT_WARP;
        } else if (_gShelterB3GarbageIncineratorSwitchNoticeTicks >= SHELTER_B3_GARBAGE_INCINERATOR_SWITCH_READY_TICKS) {
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_SWITCH_PRESS, 0, 0);
            shelterB3GarbageIncineratorShowTimedCaption(SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_EXIT_BLOCKED, SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_DEFAULT_KEY, SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_NOTICE_TICKS);
            _gShelterB3GarbageIncineratorSwitchNoticeTicks = 0;
        }
    }
    return 0;
}

/// Routes Glutton movement and replacement events to the lift and encounter tasks.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT`: event 0 starts the carried lift move;
/// 1 respawns the encounter, and 2 selects its later-phase spawn. Event 0
/// requires the lift task recorded at setup to remain live. The unused second
/// word is ignored; every event returns zero.
static s32 _shelterB3GarbageIncineratorHandleActorEvent(Task* task, s32 messageId, s32 eventId, s32 unusedArg)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_EVENT_START_CARRIED_MOVE   = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_EVENT_REPLACE_GLUTTON      = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_EVENT_REPLACE_LATE_GLUTTON = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_GLUTTON_NORMAL_SPAWN       = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_GLUTTON_LATE_SPAWN         = 1,
    };

    switch (eventId) {
        case SHELTER_B3_GARBAGE_INCINERATOR_EVENT_START_CARRIED_MOVE:
            taskMessageDispatch(_gShelterB3GarbageIncineratorSpawnedLiftTask, ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_EVENT_REPLACE_GLUTTON:
            gGameSession->skipEventIntro = 1;
            taskSpawnFromTable(&gActor444000IncineratorEventTaskDesc, 0, SHELTER_B3_GARBAGE_INCINERATOR_GLUTTON_NORMAL_SPAWN, 0);
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_EVENT_REPLACE_LATE_GLUTTON:
            gGameSession->skipEventIntro                   = 1;
            gShelterB3GarbageIncineratorDoorEventSuspended = SHELTER_B3_GARBAGE_INCINERATOR_DOOR_EVENT_SUSPENDED;
            taskSpawnFromTable(&gActor444000IncineratorEventTaskDesc, 0, SHELTER_B3_GARBAGE_INCINERATOR_GLUTTON_LATE_SPAWN, 0);
            break;
    }
    return 0;
}

/// Maps room sound cues 9 and 10 to the incinerator bank's corresponding scripts.
///
/// Handles `ROOM_MESSAGE_SOUND` with an integer cue and unused second word.
/// Requires the room sound bank loaded. Other cues do nothing; returns zero.
static s32 _shelterB3GarbageIncineratorHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unused)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_9  = 9,
        SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_10 = 10
    };
    switch (cueId) {
        case SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_9:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_9), 0, 0);
            break;
        case SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_10:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, SHELTER_B3_GARBAGE_INCINERATOR_SOUND_CUE_10), 0, 0);
            break;
    }
    return 0;
}

/// Installs room messages and captions, then starts the lift and variant encounters.
///
/// State 0 registers the room slot and advances to the switch-timer state.
/// Rooms 4 and above start the encounter controller; variant 2 adds a Mad Chaser.
static void _shelterB3GarbageIncineratorInitializeRoom(Task* task)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_TEXTURE_X    = 384,
        SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_FIRST_ROOM = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_MAD_CHASER_VARIANT   = 2,
    };

    task->msgTable = _gShelterB3GarbageIncineratorRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    shelterB3GarbageIncineratorSelectCaptionResource(SHELTER_B3_GARBAGE_INCINERATOR_CAPTION_TEXTURE_X, 0, 0);
    _gShelterB3GarbageIncineratorSpawnedLiftTask = taskSpawnFromTable(&gShelterB3GarbageIncineratorLiftTaskDesc, 0, 0, 0);
    if (gGameSession->location.roomState >= SHELTER_B3_GARBAGE_INCINERATOR_ENCOUNTER_FIRST_ROOM) {
        taskSpawnFromTable(gShelterB3GarbageIncineratorBlazeTaskDescs, SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CONTROLLER_TASK, SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_PLAY_ALERT, 0);
    }
    if (gGameSession->location.variant == SHELTER_B3_GARBAGE_INCINERATOR_MAD_CHASER_VARIANT) {
        taskSpawnFromTable(gActor342400EncounterTaskDescs, ACTOR_342400_ENCOUNTER_CONTROLLER_DESCRIPTOR, 0, 0);
    }
    task->state = task->state + 1;
}

/// Advances the switch's elapsed-frame counter up to its ready threshold of 61.
///
/// The room controller calls this in state 1. A switch press resets the
/// unsigned halfword counter to zero; calls below 61 increment it once, and
/// values at least 61 are preserved. `task` is unused.
static void _shelterB3GarbageIncineratorAdvanceSwitchTimer(Task* task)
{
    // Retained unused storage preserves the original 16-byte stack frame.
    char unusedStackBytes[0x10];

    if (_gShelterB3GarbageIncineratorSwitchNoticeTicks < SHELTER_B3_GARBAGE_INCINERATOR_SWITCH_READY_TICKS) {
        _gShelterB3GarbageIncineratorSwitchNoticeTicks++;
    }
}

void shelterB3GarbageIncineratorRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gShelterB3GarbageIncineratorRoomTaskStates;
    handlers.funcs[task->state](task);
}
