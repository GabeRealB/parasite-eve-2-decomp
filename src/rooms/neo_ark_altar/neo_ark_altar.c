#include "rooms/neo_ark_altar.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "neo_ark_altar_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

/// The room's own `TaskMessageEntry[]` - the message table this task publishes.
extern TaskMessageEntry D_neo_ark_altar_8017EF98[];

/// Single-entry spawn table for the altar's cutscene-driver task
/// (`_neoArkAltarSwitchSceneTask`).
extern TaskDesc D_neo_ark_altar_8017EF8C;

static void _neoArkAltarInitializeRoomTask(Task* task);
static void _neoArkAltarRoomIdleState(Task* unusedTask);

/// State table of the room's message task: set-up
/// (`_neoArkAltarInitializeRoomTask`), an empty per-frame state and `taskKill`.
/// Its bytes open the room's rodata, ahead of the cutscene driver's jump table.
static const TaskFuncTable3 D_neo_ark_altar_8017D5C4 = {
    _neoArkAltarInitializeRoomTask,
    _neoArkAltarRoomIdleState,
    taskKill,
};

static void _neoArkAltarSwitchSceneTask(Task* task);
static s32  _neoArkAltarRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _neoArkAltarResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkAltarIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg);
static s32  _neoArkAltarHandleActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

TaskDesc D_neo_ark_altar_8017EF8C = { { { TASK_BODY_NONE, 32 } }, _neoArkAltarSwitchSceneTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_altar_8017EF98[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkAltarResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkAltarRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkAltarHandleActionMessage },
    { ROOM_MESSAGE_COMMAND, _neoArkAltarIgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Restores player/scene presentation after an altar switch choice and releases it.
///
/// Requires a live bodyless controller and saved/session state. The movie task
/// owns movie-resource restoration; this handoff restores game view/control.
static inline void _neoArkAltarFinishSwitchScene(Task* task)
{
    enum { NEO_ARK_ALTAR_SWITCH_RETURN_VIEW = 2 };

    SetDispMask(1);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = NEO_ARK_ALTAR_SWITCH_RETURN_VIEW;
    gGameSession->hideHud                                      = 0;
    gGameSession->eventState                                   = 0;
    gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_RUNNING;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    taskKill(task);
}

/// Runs the altar switch choice, sprite animation and selected movie.
///
/// Starts bodyless in state 0 with loaded altar/CAP/movie resources. Holds and
/// hides the player and scene actors while command 2 selects clear (key 11),
/// set (key 21) or cancellation (key 12). Accepted choices animate the switch
/// for 30 callback frames and launch movie variant 0 for set or 1 for clear.
/// Completion restores presentation/control, saves view 2 and kills the task.
/// An unrecognized choice keeps waiting; movie-spawn failure is not inspected.
static void _neoArkAltarSwitchSceneTask(Task* task)
{
    enum {
        NEO_ARK_ALTAR_SWITCH_PREPARE          = 0,
        NEO_ARK_ALTAR_SWITCH_SELECTION_DELAY  = 1,
        NEO_ARK_ALTAR_SWITCH_START_CAP        = 2,
        NEO_ARK_ALTAR_SWITCH_WAIT_CAP         = 3,
        NEO_ARK_ALTAR_SWITCH_RESOLVE_CHOICE   = 4,
        NEO_ARK_ALTAR_SWITCH_COMMIT_CHOICE    = 5,
        NEO_ARK_ALTAR_SWITCH_ANIMATE          = 6,
        NEO_ARK_ALTAR_SWITCH_START_MOVIE      = 7,
        NEO_ARK_ALTAR_SWITCH_FINISH_MOVIE     = 8,
        NEO_ARK_ALTAR_SWITCH_RESTORE          = 10,
        NEO_ARK_ALTAR_SWITCH_CAP_COMMAND      = 2,
        NEO_ARK_ALTAR_SWITCH_KEY_CLEAR        = 11,
        NEO_ARK_ALTAR_SWITCH_KEY_SET          = 21,
        NEO_ARK_ALTAR_SWITCH_KEY_CANCEL       = 12,
        NEO_ARK_ALTAR_SWITCH_ANIMATION_FRAMES = 30,
        NEO_ARK_ALTAR_SWITCH_SCENE_VIEW       = 5,
        NEO_ARK_ALTAR_SWITCH_MOVIE_SET        = 0,
        NEO_ARK_ALTAR_SWITCH_MOVIE_CLEAR      = 1,
    };

    switch (task->state) {
        case NEO_ARK_ALTAR_SWITCH_PREPARE:
            // Keep the room hidden and player held until the choice/movie finishes.
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = NEO_ARK_ALTAR_SWITCH_SCENE_VIEW;
            gGameSession->hideHud                                      = 1;
            gGameSession->eventState                                   = 1;
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            task->state++;
            break;
        case NEO_ARK_ALTAR_SWITCH_SELECTION_DELAY:
            task->state++;
            break;
        case NEO_ARK_ALTAR_SWITCH_START_CAP:
            capRunCommand(NEO_ARK_ALTAR_SWITCH_CAP_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case NEO_ARK_ALTAR_SWITCH_WAIT_CAP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case NEO_ARK_ALTAR_SWITCH_RESOLVE_CHOICE:
            switch (capGetVariantKey()) {
                case NEO_ARK_ALTAR_SWITCH_KEY_CLEAR:
                    gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE, 0);
                    task->state++;
                    break;
                case NEO_ARK_ALTAR_SWITCH_KEY_SET:
                    gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE, 1);
                    task->state++;
                    break;
                case NEO_ARK_ALTAR_SWITCH_KEY_CANCEL:
                    task->state = NEO_ARK_ALTAR_SWITCH_RESTORE;
                    break;
            }
            break;
        case NEO_ARK_ALTAR_SWITCH_COMMIT_CHOICE:
            if ((gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_0F9) == 0) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0)) {
                gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_0F9, 1);
                areaApplySavedUpdates(D_neo_ark_altar_801800A0);
            }
            sndEvtRequestScriptStart(SOUND_NEO_ARK_ALTAR_SWITCH_TOGGLE, 0, 0);
            task->killCountdown = NEO_ARK_ALTAR_SWITCH_ANIMATION_FRAMES;
            task->state++;
            break;
        case NEO_ARK_ALTAR_SWITCH_ANIMATE:
            neoArkAltarStepSwitchSprites(gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) & 0xFF);
            task->killCountdown--;
            if (task->killCountdown <= 0) {
                task->state++;
            }
            break;
        case NEO_ARK_ALTAR_SWITCH_START_MOVIE:
            if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) != 0) {
                taskSpawnFromTable(D_neo_ark_altar_8017EFC0, 0, NEO_ARK_ALTAR_SWITCH_MOVIE_SET, 0);
            } else {
                taskSpawnFromTable(D_neo_ark_altar_8017EFC0, 0, NEO_ARK_ALTAR_SWITCH_MOVIE_CLEAR, 0);
            }
            task->state++;
            break;
        case NEO_ARK_ALTAR_SWITCH_FINISH_MOVIE:
            task->state = NEO_ARK_ALTAR_SWITCH_RESTORE;
            break;
        case NEO_ARK_ALTAR_SWITCH_RESTORE:
            _neoArkAltarFinishSwitchScene(task);
            break;
    }
}

/// Refuses key-item use at the altar without consuming the item or starting an event.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED` for the item menu's unavailable-use notice.
static s32 _neoArkAltarRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a Neo Ark destination room for an altar transition request.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`. Copies the complete eight-byte request
/// into writable reply storage before resolving its room from game progress.
/// Query mode preserves the copied record. The pointers may alias and are
/// borrowed only for this call; the Neo Ark map overlay must be loaded.
/// Always returns 1 to allow the transition; task and message ID are unused.
static s32 _neoArkAltarResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_ALTAR_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_ALTAR_TRANSITION_ALLOWED;
}

/// Ignores CAP room commands at the altar and returns zero.
///
/// Handles `ROOM_MESSAGE_COMMAND`; neither integer payload is read and the
/// room and receiver remain unchanged.
static s32 _neoArkAltarIgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedCommandArg)
{
    return 0;
}

/// Handles the altar switch action according to the current room selector.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte read-only request for
/// this call; no pointer is retained. Action 1 runs CAP command 3 in room 1
/// and starts the switch scene in other rooms. Other actions are ignored.
/// Control/argument bytes, receiver, ID and zero second word are unused.
/// Returns zero; requires the altar's loaded CAP and task resources.
static s32 _neoArkAltarHandleActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { NEO_ARK_ALTAR_ACTION_SWITCH      = 1,
           NEO_ARK_ALTAR_ROOM_1_CAP_COMMAND = 3 };

    if (request->actionId == NEO_ARK_ALTAR_ACTION_SWITCH) {
        if (gGameSession->location.loc.room == request->actionId) {
            capRunCommandWithTransition(NEO_ARK_ALTAR_ROOM_1_CAP_COMMAND);
        } else {
            taskSpawnFromTable(&D_neo_ark_altar_8017EF8C, 0, 0, 0);
        }
    }
    return 0;
}

/// Publishes the altar's room-message interface and starts its tile puzzle.
///
/// Requires a live state-0 room task and loaded altar sprites and descriptors.
/// Advances the switch sprites toward the saved choice before starting the
/// tile controller, then enters idle state 1. Spawn failure is not inspected;
/// the registered room-task pointer remains borrowed through normal teardown.
static void _neoArkAltarInitializeRoomTask(Task* task)
{
    task->msgTable = D_neo_ark_altar_8017EF98;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    neoArkAltarStepSwitchSprites(gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE) & 0xFF);
    taskSpawnFromTable(D_neo_ark_altar_8017F088, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// Keeps the altar room task available for messages after initialization.
///
/// State 1 performs no per-frame work and leaves `unusedTask` unchanged.
static void _neoArkAltarRoomIdleState(Task* unusedTask)
{
}

void neoArkAltarRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_neo_ark_altar_8017D5C4;
    handlers.funcs[task->state](task);
}
