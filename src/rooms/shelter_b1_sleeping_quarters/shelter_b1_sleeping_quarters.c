#include "rooms/shelter_b1_sleeping_quarters.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

static void _shelterB1SleepingQuartersPlayAlternateCapTask(Task* task);

extern TaskDesc D_shelter_b1_sleeping_quarters_80180540;

/// The room's message table, which its cap scripts index.
extern TaskMessageEntry D_shelter_b1_sleeping_quarters_80180518[];

static s32 _shelterB1SleepingQuartersRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static s32 _shelterB1SleepingQuartersResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1SleepingQuartersHandleCapCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedArg);
static s32 _shelterB1SleepingQuartersIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

static u32     _gShelterB1SleepingQuartersModel02DFCPartVerts[1];
static SVECTOR _gShelterB1SleepingQuartersModel02DFCVerts[22];
static TmdBone _gShelterB1SleepingQuartersModel02DFCSkeleton[1];
static u32     _gShelterB1SleepingQuartersModel02DFCStream[78];

static TmdBone _gShelterB1SleepingQuartersModel02DFCSkeleton[1] = {
#include "assets/shelter_b1_sleeping_quarters_model_02DFC_skeleton.inc"
};

static u32 _gShelterB1SleepingQuartersModel02DFCPartVerts[1] = {
#include "assets/shelter_b1_sleeping_quarters_model_02DFC_partVerts.inc"
};

static SVECTOR _gShelterB1SleepingQuartersModel02DFCVerts[22] = {
#include "assets/shelter_b1_sleeping_quarters_model_02DFC_verts.inc"
};

static u32 _gShelterB1SleepingQuartersModel02DFCStream[78] = {
#include "assets/shelter_b1_sleeping_quarters_model_02DFC_stream.inc"

};

TmdSource gShelterB1SleepingQuartersModel02DFC = {
    0,
    552,
    0,
    1,
    _gShelterB1SleepingQuartersModel02DFCPartVerts,
    _gShelterB1SleepingQuartersModel02DFCVerts,
    &_gShelterB1SleepingQuartersModel02DFCVerts[22],
    _gShelterB1SleepingQuartersModel02DFCSkeleton,
    _gShelterB1SleepingQuartersModel02DFCStream,
};

TaskMessageEntry D_shelter_b1_sleeping_quarters_80180518[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1SleepingQuartersResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1SleepingQuartersRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1SleepingQuartersIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1SleepingQuartersHandleCapCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_sleeping_quarters_80180540 = { { { TASK_BODY_NONE, 192 } }, _shelterB1SleepingQuartersPlayAlternateCapTask, { .value = 0 } };

static void _shelterB1SleepingQuartersInitializeRoom(Task* task);
static void _shelterB1SleepingQuartersIdleRoomState(Task* task);

void shelterB1SleepingQuartersAreaObjectTask(Task* task)
{
    enum { SHELTER_B1_SLEEPING_QUARTERS_OBJECT_HIDDEN = 2 };

    TmdObject* model        = task->extra.tmd;
    Enemy*     placedObject = task->spawnArg2.pointer;

    if (areaGetCurrentObjectState((u8)placedObject->placeKey) == SHELTER_B1_SLEEPING_QUARTERS_OBJECT_HIDDEN) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Refuses key-item use without consuming the item or changing room state.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` carries the collected-item ID and a zero second
/// payload. All parameters are unused; the reply selects the menu's refusal notice.
static s32 _shelterB1SleepingQuartersRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves departures and intercepts walkway exits from chapter 6 onward with CAP playback.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; borrows a readable eight-byte request
/// and writable reply, which may alias. Copies and resolves the Shelter variant
/// first. Returns 1 for ordinary departures or before chapter 6; otherwise
/// returns 0. An executing walkway request then starts CAP command 14; queries
/// have no playback effects. Requires loaded room/map overlays and CAP data.
/// Neither payload pointer is retained; task and message ID are unused.
static s32 _shelterB1SleepingQuartersResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B1_SLEEPING_QUARTERS_DEPARTURE_ALLOWED     = 1,
        SHELTER_B1_SLEEPING_QUARTERS_DEPARTURE_INTERCEPTED = 0,
        SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_CAP_CHAPTER   = 6,
        SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_EXIT_CAP      = 14,
    };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId != GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY) {
        return SHELTER_B1_SLEEPING_QUARTERS_DEPARTURE_ALLOWED;
    }
    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_CAP_CHAPTER) {
        return SHELTER_B1_SLEEPING_QUARTERS_DEPARTURE_ALLOWED;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return SHELTER_B1_SLEEPING_QUARTERS_DEPARTURE_INTERCEPTED;
    }
    capRunCommandWithTransition(SHELTER_B1_SLEEPING_QUARTERS_WALKWAY_EXIT_CAP);
    return SHELTER_B1_SLEEPING_QUARTERS_DEPARTURE_INTERCEPTED;
}

/// Starts alternate-file CAP playback or selects the room's flag-dependent CAP event.
///
/// `ROOM_MESSAGE_COMMAND` carries selector 8 to hold player control and spawn
/// alternate-file command 8; selector 3 requests event 3 while flag 0x16F is zero,
/// event 15 otherwise. The event request is ignored while another CAP event is
/// active. Other selectors do nothing; all other parameters are unused and the
/// reply is always zero. Requires loaded room CAP data. The spawned task restores
/// player control on completion; a failed spawn leaves the requested hold intact.
static s32 _shelterB1SleepingQuartersHandleCapCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedArg)
{
    enum {
        SHELTER_B1_SLEEPING_QUARTERS_ALTERNATE_FILE_CAP   = 8,
        SHELTER_B1_SLEEPING_QUARTERS_SELECT_CAP_EVENT     = 3,
        SHELTER_B1_SLEEPING_QUARTERS_CAP_EVENT_FLAG_CLEAR = 3,
        SHELTER_B1_SLEEPING_QUARTERS_CAP_EVENT_FLAG_SET   = 15,
    };

    if (commandIndex == SHELTER_B1_SLEEPING_QUARTERS_ALTERNATE_FILE_CAP) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(&D_shelter_b1_sleeping_quarters_80180540, 0, SHELTER_B1_SLEEPING_QUARTERS_ALTERNATE_FILE_CAP, 0);
    }
    if (commandIndex == SHELTER_B1_SLEEPING_QUARTERS_SELECT_CAP_EVENT) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_SLEEPING_QUARTERS_16F) == 0 ? SHELTER_B1_SLEEPING_QUARTERS_CAP_EVENT_FLAG_CLEAR : SHELTER_B1_SLEEPING_QUARTERS_CAP_EVENT_FLAG_SET, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

/// Ignores direction-trigger actions without changing room state.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request and supplies a
/// zero second payload. Neither is read or retained; the sender ignores the result.
static s32 _shelterB1SleepingQuartersIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum { SHELTER_B1_SLEEPING_QUARTERS_ROOM_ACTION_IGNORED = 0 };

    return SHELTER_B1_SLEEPING_QUARTERS_ROOM_ACTION_IGNORED;
}

/// Selects the sleeping quarters' second loaded CAP resource and its texture page.
///
/// Requires idle playback and the resource and textures already loaded. The
/// resource remains borrowed until playback finishes and `capReset` selects
/// the default again. Texture origin is (704, 0) in 16-bit VRAM pixels.
static inline void _shelterB1SleepingQuartersSelectAlternateCap(void)
{
    enum {
        SHELTER_B1_SLEEPING_QUARTERS_ALTERNATE_CAP_RESOURCE  = 1,
        SHELTER_B1_SLEEPING_QUARTERS_ALTERNATE_CAP_TEXTURE_X = 704,
    };

    Gp_CapFile = NULL;
    capSelectLoadedFile(SHELTER_B1_SLEEPING_QUARTERS_ALTERNATE_CAP_RESOURCE);
    capSetTexturePage(SHELTER_B1_SLEEPING_QUARTERS_ALTERNATE_CAP_TEXTURE_X, 0);
}

/// Plays a command from the alternate CAP resource, then restores player control.
///
/// A fresh bodyless task starts in state 0 with `spawnArg1.value` as its valid
/// command index (the room sends 8). Requires idle CAP playback and the second
/// CAP resource and its textures loaded until completion. State 1 waits for
/// playback to become idle; state 2 resumes control, resets CAP to resource 0
/// and kills the task. No work or payload pointer is owned or retained.
static void _shelterB1SleepingQuartersPlayAlternateCapTask(Task* task)
{
    enum {
        SHELTER_B1_SLEEPING_QUARTERS_CAP_PREPARE = 0,
        SHELTER_B1_SLEEPING_QUARTERS_CAP_WAIT    = 1,
        SHELTER_B1_SLEEPING_QUARTERS_CAP_FINISH  = 2,
    };

    switch (task->state) {
        case SHELTER_B1_SLEEPING_QUARTERS_CAP_PREPARE:
            _shelterB1SleepingQuartersSelectAlternateCap();
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_DISPLAY_TRANSITION);
            task->state++;
            break;
        case SHELTER_B1_SLEEPING_QUARTERS_CAP_WAIT:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case SHELTER_B1_SLEEPING_QUARTERS_CAP_FINISH:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
            taskKill(task);
            break;
    }
}

/// Publishes the sleeping-quarters receiver for synchronous room messages.
///
/// Requires state 0 and the loaded room message table. Registers the live task
/// in `GAME_TASK_SLOT_ROOM` and advances to idle state 1 without allocating work.
static void _shelterB1SleepingQuartersInitializeRoom(Task* task)
{
    task->msgTable = D_shelter_b1_sleeping_quarters_80180518;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
}

/// Leaves the initialized room receiver waiting for messages in state one.
static void _shelterB1SleepingQuartersIdleRoomState(Task* task)
{
}

/// The room task's three states, run from a stack copy by
/// `shelterB1SleepingQuartersRoomTask`: the entry tick, the idle
/// state, then `taskKill`.
static const TaskFuncTable3 D_shelter_b1_sleeping_quarters_8017D5C4 = {
    { _shelterB1SleepingQuartersInitializeRoom, _shelterB1SleepingQuartersIdleRoomState, taskKill },
};

void shelterB1SleepingQuartersRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_shelter_b1_sleeping_quarters_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
