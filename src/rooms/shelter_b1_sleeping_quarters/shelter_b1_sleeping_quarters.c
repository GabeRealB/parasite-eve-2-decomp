#include "rooms/shelter_b1_sleeping_quarters.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/captions.h"
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

void func_shelter_b1_sleeping_quarters_8017D778(Task* task);

extern TaskDesc D_shelter_b1_sleeping_quarters_80180540;

/// The room's message table, which its cap scripts index.
extern TaskMessageEntry D_shelter_b1_sleeping_quarters_80180518[];

s32 func_shelter_b1_sleeping_quarters_8017D668(Task*, s32, s32, s32);
s32 func_shelter_b1_sleeping_quarters_8017D670(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_sleeping_quarters_8017D6FC(Task*, s32, s32, s32);
s32 func_shelter_b1_sleeping_quarters_8017D770(Task*, s32, s32, s32);

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
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_sleeping_quarters_8017D670 },
    { 5105, func_shelter_b1_sleeping_quarters_8017D668 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_sleeping_quarters_8017D770 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_sleeping_quarters_8017D6FC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_sleeping_quarters_80180540 = { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sleeping_quarters_8017D778, { .value = 0 } };

static void func_shelter_b1_sleeping_quarters_8017D83C(Task* task);
static void func_shelter_b1_sleeping_quarters_8017D880(Task* task);

/// Hides the task's model while the 2-bit game flag its spawn argument names
/// reads 2, and shows it otherwise.
void func_shelter_b1_sleeping_quarters_8017D608(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    if (Gp_GetCurBit2Flag((u8)((Enemy*)task->spawnArg2.pointer)->placeKey) == 2) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

s32 func_shelter_b1_sleeping_quarters_8017D668(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b1_sleeping_quarters_8017D670(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY) {
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

s32 func_shelter_b1_sleeping_quarters_8017D6FC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 8) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_shelter_b1_sleeping_quarters_80180540, 0, 8, 0);
    }
    if (arg2 == 3) {
        Gp_SpawnIfCapIdle(gameFlagGetNibble(GAME_FLAG_SLEEPING_QUARTERS_16F) == 0 ? 3 : 0xF, 0);
    }
    return 0;
}

s32 func_shelter_b1_sleeping_quarters_8017D770(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

void func_shelter_b1_sleeping_quarters_8017D778(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0);
            Gp_RunCapCmd(task->spawnArg1.value, 1);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

/// First state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7 and advances to the next state.
static void func_shelter_b1_sleeping_quarters_8017D83C(Task* task)
{
    task->msgTable = D_shelter_b1_sleeping_quarters_80180518;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room task.
static void func_shelter_b1_sleeping_quarters_8017D880(Task* task)
{
}

/// The room task's three states, run from a stack copy by
/// `func_shelter_b1_sleeping_quarters_8017D888`: the entry tick, the idle
/// state, then `taskKill`.
static const TaskFuncTable3 D_shelter_b1_sleeping_quarters_8017D5C4 = {
    { func_shelter_b1_sleeping_quarters_8017D83C, func_shelter_b1_sleeping_quarters_8017D880, taskKill },
};

/// Runs the room task's current state from a stack copy of its state table.
void func_shelter_b1_sleeping_quarters_8017D888(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sleeping_quarters_8017D5C4;
    sp.funcs[task->state](task);
}
