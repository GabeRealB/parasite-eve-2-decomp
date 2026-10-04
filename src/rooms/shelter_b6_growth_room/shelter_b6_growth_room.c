#include "rooms/shelter_b6_growth_room.h"

#include "types.h"

#include "shelter_b6_growth_room_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

extern TaskDesc         D_80135E78;
extern TaskMessageEntry D_shelter_b6_growth_room_8017F16C[];

extern EvsCommand D_80136110[];
extern EvsCommand D_80136308[];

extern void func_actor_450900_801327A8(void);
extern void func_actor_450900_80132834(void);

s32 func_shelter_b6_growth_room_8017D5E8(Task*, s32, s32, s32);
s32 func_shelter_b6_growth_room_8017D5F0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b6_growth_room_8017D634(Task*, s32, s32, s32);
s32 func_shelter_b6_growth_room_8017D6C8(Task*, s32, RoomEventMsg*, s32);

TaskMessageEntry D_shelter_b6_growth_room_8017F16C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b6_growth_room_8017D5F0 },
    { 5105, func_shelter_b6_growth_room_8017D5E8 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b6_growth_room_8017D6C8 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b6_growth_room_8017D634 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_shelter_b6_growth_room_8017D71C(Task* arg0);
static void func_shelter_b6_growth_room_8017D7CC(Task* task);

/// The room's handler for message 0x13F1: accepts it and does nothing.
s32 func_shelter_b6_growth_room_8017D5E8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming `RoomEventMsg` onto
/// the outgoing one, passes both to `func_map_neo_ark_80179B14`, and returns 1.
s32 func_shelter_b6_growth_room_8017D5F0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_shelter_b6_growth_room_8017D634(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(GAME_FLAG_0D8) == 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_80135E78, 3, 0, 0);
        } else {
            Gp_RunCapCmd1(1);
        }
    }
    if (arg2 == 0x10) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(GAME_FLAG_0D8) == 0 ? 0x10 : 0x11, 0);
    }
    return 0;
}

s32 func_shelter_b6_growth_room_8017D6C8(Task* arg0, s32 arg1, RoomEventMsg* arg2, s32 arg3)
{
    if (arg2->warp == 1) {
        func_actor_450900_801327A8();
    }
    if (arg2->warp == 2) {
        func_actor_450900_80132834();
    }
    return 0;
}

static void func_shelter_b6_growth_room_8017D71C(Task* arg0)
{
    arg0->msgTable = D_shelter_b6_growth_room_8017F16C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    Gp_FillAllyHp();
    Gp_ApplyAreaRecs(D_shelter_b6_growth_room_801807C8);
    func_800E8634(D_80136110, 0, D_80136308);
    Task_SpawnFromTable(&D_80135E78, 1, 0, 0);
    Task_SpawnFromTable(&D_80135E78, 2, 0, 0);
    func_800E3FAC(0xA2, 0x33);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_shelter_b6_growth_room_8017D7CC(Task* task)
{
}

/// State table of the room task: set-up, idle, kill.
static const TaskFuncTable3 D_shelter_b6_growth_room_8017D5C4 = {
    {
        func_shelter_b6_growth_room_8017D71C,
        func_shelter_b6_growth_room_8017D7CC,
        taskKill,
    },
};

/// The room task: copies its state table onto the stack and calls the entry
/// for the current state.
void func_shelter_b6_growth_room_8017D7D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_growth_room_8017D5C4;
    sp.funcs[task->state](task);
}
