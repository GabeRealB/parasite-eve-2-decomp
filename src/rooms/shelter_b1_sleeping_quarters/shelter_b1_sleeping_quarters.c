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

void func_shelter_b1_sleeping_quarters_8017D778(Task* task);

extern TaskDesc D_shelter_b1_sleeping_quarters_80180540;

/// The room's message table, which its cap scripts index.
extern TaskMessageEntry D_shelter_b1_sleeping_quarters_80180518[];

static s32 _shelterB1SleepingQuartersRejectKeyItemUse(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
s32        func_shelter_b1_sleeping_quarters_8017D670(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_shelter_b1_sleeping_quarters_8017D6FC(Task*, s32, s32, s32);
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
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_sleeping_quarters_8017D670 },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB1SleepingQuartersRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB1SleepingQuartersIgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_sleeping_quarters_8017D6FC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b1_sleeping_quarters_80180540 = { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_sleeping_quarters_8017D778, { .value = 0 } };

static void func_shelter_b1_sleeping_quarters_8017D83C(Task* task);
static void _shelterB1SleepingQuartersIdleRoomState(Task* task);

/// Hides the task's model while the 2-bit game flag its spawn argument names
/// reads 2, and shows it otherwise.
void func_shelter_b1_sleeping_quarters_8017D608(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    if (areaGetCurrentObjectState((u8)((Enemy*)task->spawnArg2.pointer)->placeKey) == 2) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
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

s32 func_shelter_b1_sleeping_quarters_8017D670(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    if (in->areaId != GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY) {
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

s32 func_shelter_b1_sleeping_quarters_8017D6FC(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 8) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        taskSpawnFromTable(&D_shelter_b1_sleeping_quarters_80180540, 0, 8, 0);
    }
    if (arg2 == 3) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_SLEEPING_QUARTERS_16F) == 0 ? 3 : 0xF, CAP_EVENT_NO_FLAGS);
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

void func_shelter_b1_sleeping_quarters_8017D778(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            capSelectLoadedFile(1);
            capSetTexturePage(0x2C0, 0);
            capRunCommand(task->spawnArg1.value, CAP_PLAYBACK_DISPLAY_TRANSITION);
            task->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            capReset();
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

/// Leaves the initialized room receiver waiting for messages in state one.
static void _shelterB1SleepingQuartersIdleRoomState(Task* task)
{
}

/// The room task's three states, run from a stack copy by
/// `func_shelter_b1_sleeping_quarters_8017D888`: the entry tick, the idle
/// state, then `taskKill`.
static const TaskFuncTable3 D_shelter_b1_sleeping_quarters_8017D5C4 = {
    { func_shelter_b1_sleeping_quarters_8017D83C, _shelterB1SleepingQuartersIdleRoomState, taskKill },
};

/// Runs the room task's current state from a stack copy of its state table.
void func_shelter_b1_sleeping_quarters_8017D888(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_sleeping_quarters_8017D5C4;
    sp.funcs[task->state](task);
}
