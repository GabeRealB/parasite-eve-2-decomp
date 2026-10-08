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

static void func_shelter_b1_storeroom_8017D740(Task* task);
static void _shelterB1StoreroomIdleRoomState(Task* task);

/// The room task's three states, dispatched by
/// `func_shelter_b1_storeroom_8017D794`: install the message table, idle, end.
static const TaskFuncTable3 D_shelter_b1_storeroom_8017D5C4 = {
    { func_shelter_b1_storeroom_8017D740, _shelterB1StoreroomIdleRoomState, taskKill }
};

static s32 _shelterB1StoreroomRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
s32        func_shelter_b1_storeroom_8017D604(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32 _shelterB1StoreroomIgnoreRoomCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32 _shelterB1StoreroomIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32 _shelterB1StoreroomHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg);

TaskMessageEntry D_shelter_b1_storeroom_80184968[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_storeroom_8017D604 },
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

s32 func_shelter_b1_storeroom_8017D604(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B1_ARMORY && gameFlagGetNibble(GAME_FLAG_B1_ARMORY_STOREROOM_DOOR_UNLOCKED) == 0) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        gameFlagSetNibbleIfPresent(in->flagId, 2);
        capRunCommandWithTransition(1);
        return 0;
    }
    if (in->areaId != GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY && in->areaId != GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) < 6) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    capRunCommandWithTransition(0xE);
    return 0;
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

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7, sets `D_80115598` and advances to the idle state.
static void func_shelter_b1_storeroom_8017D740(Task* task)
{
    task->msgTable = D_shelter_b1_storeroom_80184968;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
    D_80115598  = 1;
}

/// Leaves the initialized storeroom receiver waiting for messages in state one.
static void _shelterB1StoreroomIdleRoomState(Task* task)
{
}

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b1_storeroom_8017D794(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_storeroom_8017D5C4;
    sp.funcs[task->state](task);
}
