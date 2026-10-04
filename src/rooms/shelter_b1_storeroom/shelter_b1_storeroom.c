#include "rooms/shelter_b1_storeroom.h"

#include "types.h"

#include "gameplay/captions.h"
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
static void func_shelter_b1_storeroom_8017D78C(Task* task);

/// The room task's three states, dispatched by
/// `func_shelter_b1_storeroom_8017D794`: install the message table, idle, end.
static const TaskFuncTable3 D_shelter_b1_storeroom_8017D5C4 = {
    { func_shelter_b1_storeroom_8017D740, func_shelter_b1_storeroom_8017D78C, taskKill }
};

s32 func_shelter_b1_storeroom_8017D5FC(Task*, s32, s32, s32);
s32 func_shelter_b1_storeroom_8017D604(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_storeroom_8017D6E0(Task*, s32, s32, s32);
s32 func_shelter_b1_storeroom_8017D6E8(Task*, s32, s32, s32);
s32 func_shelter_b1_storeroom_8017D6F0(Task*, s32, s32, s32);

TaskMessageEntry D_shelter_b1_storeroom_80184968[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_storeroom_8017D604 },
    { 5105, func_shelter_b1_storeroom_8017D5FC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_storeroom_8017D6E8 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_storeroom_8017D6E0 },
    { ROOM_MESSAGE_SOUND, func_shelter_b1_storeroom_8017D6F0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 func_shelter_b1_storeroom_8017D5FC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D604(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B1_ARMORY && gameFlagGetNibble(GAME_FLAG_B1_ARMORY_STOREROOM_DOOR_UNLOCKED) == 0) {
        if (in->queryOnly != ROOM_EVENT_EXECUTE) {
            return 0;
        }
        Gp_SetNibbleIf(in->flagId, 2);
        Gp_RunCapCmd1(1);
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
    Gp_RunCapCmd1(0xE);
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6E0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6E8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_storeroom_8017D6F0(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 8:
            SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, 8), 0, 0);
            break;
        case 0x6A:
            SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, 9), 0, 0);
            break;
    }
    return 0;
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

static void func_shelter_b1_storeroom_8017D78C(Task* task)
{
}

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b1_storeroom_8017D794(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_storeroom_8017D5C4;
    sp.funcs[task->state](task);
}
