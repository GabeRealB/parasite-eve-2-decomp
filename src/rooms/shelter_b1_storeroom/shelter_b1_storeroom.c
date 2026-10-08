#include "rooms/shelter_b1_storeroom.h"

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

/// Message table the room task installs on itself: ids 0x13EE-0x13F2 mapped
/// to the room's handlers, closed by id `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_shelter_b1_storeroom_80184968[];

static void _shelterB1StoreroomInitializeRoomState(Task* task);
static void _shelterB1StoreroomIdleRoomState(Task* task);

/// The room task's three states, dispatched by
/// `shelterB1StoreroomTask`: install the message table, idle, end.
static const TaskFuncTable3 D_shelter_b1_storeroom_8017D5C4 = {
    { _shelterB1StoreroomInitializeRoomState, _shelterB1StoreroomIdleRoomState, taskKill }
};

static s32 _shelterB1StoreroomRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
static s32 _shelterB1StoreroomResolveTransitionMessage(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB1StoreroomIgnoreRoomCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32 _shelterB1StoreroomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _shelterB1StoreroomHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg);

TaskMessageEntry D_shelter_b1_storeroom_80184968[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB1StoreroomResolveTransitionMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1StoreroomRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1StoreroomIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB1StoreroomIgnoreRoomCommand },
    { ROOM_MESSAGE_SOUND, _shelterB1StoreroomHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Refuses key-item use without consuming the item or changing room state.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` carries a collected-item ID and a zero second
/// payload. All parameters are unused; the reply selects the menu's refusal notice.
static s32 _shelterB1StoreroomRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves the destination and gates departure through the armory and maintenance walkways.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; task and messageId are unused. Borrows
/// complete eight-byte records for this call; reply is writable and may alias
/// request. Copies the request, then resolves its room variant. The locked
/// armory and either walkway from chapter 6 onward are refused. Queries have
/// no caption/flag effects; executing a locked-armory refusal writes 2 to the
/// optional request flag. Returns 1 to allow departure, 0 to refuse it.
static s32 _shelterB1StoreroomResolveTransitionMessage(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        TRANSITION_BLOCKED        = 0,
        TRANSITION_ALLOWED        = 1,
        REFUSAL_FLAG_VALUE        = 2,
        LOCKED_ARMORY_CAP_COMMAND = 1,
        LATE_WALKWAY_CAP_COMMAND  = 0xE,
        WALKWAY_CLOSED_CHAPTER    = 6,
    };
    // Preserve the full request, including when request and reply are the same record.
    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId == GAME_AREA_SHELTER_B1_ARMORY && gameFlagGetNibble(GAME_FLAG_B1_ARMORY_STOREROOM_DOOR_UNLOCKED) == 0) {
        if (request->queryOnly != ROOM_EVENT_EXECUTE) {
            return TRANSITION_BLOCKED;
        }
        gameFlagSetNibbleIfPresent(request->flagId, REFUSAL_FLAG_VALUE);
        capRunCommandWithTransition(LOCKED_ARMORY_CAP_COMMAND);
        return TRANSITION_BLOCKED;
    }
    if (request->areaId != GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY && request->areaId != GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY) {
        return TRANSITION_ALLOWED;
    }
    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < WALKWAY_CLOSED_CHAPTER) {
        return TRANSITION_ALLOWED;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return TRANSITION_BLOCKED;
    }
    capRunCommandWithTransition(LATE_WALKWAY_CAP_COMMAND);
    return TRANSITION_BLOCKED;
}

/// Ignores CAP room commands without changing room state.
///
/// `ROOM_MESSAGE_COMMAND` carries an integer selector and argument. All four
/// parameters are unused; returns zero, which the CAP sender discards.
static s32 _shelterB1StoreroomIgnoreRoomCommand(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    enum { SHELTER_B1_STOREROOM_COMMAND_IGNORED = 0 };

    return SHELTER_B1_STOREROOM_COMMAND_IGNORED;
}

/// Ignores direction-trigger actions without changing room state.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte request and supplies a
/// zero second payload. Neither is read or retained; the sender ignores the result.
static s32 _shelterB1StoreroomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    enum { SHELTER_B1_STOREROOM_ROOM_ACTION_IGNORED = 0 };

    return SHELTER_B1_STOREROOM_ROOM_ACTION_IGNORED;
}

/// Maps storeroom CAP sound cues to scripts in the Mine/Shelter storeroom bank.
///
/// `ROOM_MESSAGE_SOUND` cue 8 selects script 8; weapon cue 106 selects script 9.
/// Unknown cues do nothing. The sound bank must be loaded; the task, message ID
/// and second payload are unused. Returns zero regardless of sound admission.
static s32 _shelterB1StoreroomHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum {
        SHELTER_B1_STOREROOM_SOUND_CUE_8           = 8,
        SHELTER_B1_STOREROOM_SOUND_CUE_WEAPON_6    = 100 + 6,
        SHELTER_B1_STOREROOM_SOUND_SCRIPT_8        = 8,
        SHELTER_B1_STOREROOM_SOUND_SCRIPT_WEAPON_6 = 9,
        SHELTER_B1_STOREROOM_SOUND_RESULT          = 0,
    };

    switch (cueId) {
        case SHELTER_B1_STOREROOM_SOUND_CUE_8:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, SHELTER_B1_STOREROOM_SOUND_SCRIPT_8), 0, 0);
            break;
        case SHELTER_B1_STOREROOM_SOUND_CUE_WEAPON_6:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, SHELTER_B1_STOREROOM_SOUND_SCRIPT_WEAPON_6), 0, 0);
            break;
    }
    return SHELTER_B1_STOREROOM_SOUND_RESULT;
}

/// Installs and registers the room receiver, enabling CAP completion sound messages.
///
/// Starts at state 0 and advances to state 1. Spawn arguments are unused;
/// the borrowed message table and callbacks remain live with the room overlay.
static void _shelterB1StoreroomInitializeRoomState(Task* task)
{
    enum {
        CAP_COMPLETION_SOUNDS_ENABLED = 1,
    };
    task->msgTable = D_shelter_b1_storeroom_80184968;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state++;
    D_80115598 = CAP_COMPLETION_SOUNDS_ENABLED;
}

/// Leaves the initialized storeroom receiver waiting for messages in state one.
static void _shelterB1StoreroomIdleRoomState(Task* task)
{
}

void shelterB1StoreroomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b1_storeroom_8017D5C4;
    states.funcs[task->state](task);
}
