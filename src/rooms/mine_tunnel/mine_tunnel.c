#include "rooms/mine_tunnel.h"

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

/// Cutscene script blob handed to `func_800E8614`; unnamed in the gameplay
/// map, which keeps the raw address.
extern GpEvsCmd D_mine_tunnel_8017E024[];

/// The room's message table: 0x13EE is handled by `func_mine_tunnel_8017D5EC`,
/// 0x13F1 by `func_mine_tunnel_8017D5E4`, 0x13EF by `func_mine_tunnel_8017D670`
/// and 0x13F0 by `func_mine_tunnel_8017D630`.
extern GpMsgEntry D_mine_tunnel_8017DFC4[];

s32 func_mine_tunnel_8017D5E4(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_mine_tunnel_8017D5EC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_mine_tunnel_8017D630(Task*, s32, s32, GpMessageArg);
s32 func_mine_tunnel_8017D670(Task*, s32, RoomEventMsg*, s32);

extern GpAnimSet D_mine_tunnel_8017DF9C;

extern GpAnimArg D_mine_tunnel_8017DFFC;
extern GpCopyArg D_mine_tunnel_8017DFF4;
void             func_mine_tunnel_8017D6E0(s32);

AnimationPackedPose D_mine_tunnel_8017DB54[10] = {
#include "assets/mine_tunnel_animation_009DC_bank1.inc"
};

AnimationPackedRotation D_mine_tunnel_8017DBCC[95] = {
#include "assets/mine_tunnel_animation_009DC_bank4.inc"
};

GpAnimRec D_mine_tunnel_8017DD48[139] = {
#include "assets/mine_tunnel_animation_009DC_records.inc"
};

u16 D_mine_tunnel_8017DF74[20] = {
#include "assets/mine_tunnel_animation_009DC_indices.inc"
};

GpAnimSet D_mine_tunnel_8017DF9C = {
    D_mine_tunnel_8017DD48,
    D_mine_tunnel_8017DF74,
    { NULL, D_mine_tunnel_8017DB54, NULL, NULL, D_mine_tunnel_8017DBCC, NULL, NULL, NULL },
};

GpMsgEntry D_mine_tunnel_8017DFC4[5] = {
    { 5102, func_mine_tunnel_8017D5EC },
    { 5105, func_mine_tunnel_8017D5E4 },
    { 5103, func_mine_tunnel_8017D670 },
    { 5104, func_mine_tunnel_8017D630 },
    { 0x7FFFFFFF, NULL },
};

GpAnimSet* D_mine_tunnel_8017DFEC[2] = {
    &D_mine_tunnel_8017DF9C,
    NULL,
};

GpCopyArg D_mine_tunnel_8017DFF4 = { { .sets = D_mine_tunnel_8017DFEC }, 2 };

GpAnimArg D_mine_tunnel_8017DFFC = { { .index = 1 }, 47, 0, 0, 1 };

// Retained parameter record; layout follows the adjacent script arguments.
GpAnimArg D_mine_tunnel_8017E010 = { { .index = 1 }, 7, 1, 10, 0 };

GpEvsCmd D_mine_tunnel_8017E024[11] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_mine_tunnel_8017DFF4 }, { .value = 0 } },
    { 13, { .callback = func_mine_tunnel_8017D6E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_tunnel_8017DFFC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

static void func_mine_tunnel_8017D6EC(Task* arg0);
static void func_mine_tunnel_8017D774(Task* task);

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_mine_tunnel_8017D5E4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `func_map_shelter_80179A04` and returns 1.
s32 func_mine_tunnel_8017D5EC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_mine_tunnel_8017D630(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 2) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x11A) >= 2 ? 3 : 2);
    }
    return 0;
}

s32 func_mine_tunnel_8017D670(Task* arg0, s32 arg1, RoomEventMsg* msg, s32 arg3)
{
    u8 temp_v1;

    temp_v1 = msg->field_2;
    if ((temp_v1 == 1) && (gGameSession->at4.loc.place == temp_v1) && (GameFlag_GetNibble(0xA1) == 0)) {
        GameFlag_SetNibble(0xA1, 1);
        Gp_MsgPlayerWeapon(0);
        func_800E8614(D_mine_tunnel_8017E024, 1);
    }
    return 0;
}

/// Stores its argument in `Gp_StateF0.field_1C`; the room's event task calls it with 2
/// on entry to the tunnel once flag 0xA1 is set.
void func_mine_tunnel_8017D6E0(s32 arg0)
{
    Gp_StateF0.field_1C = arg0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and - when the session is at place 1
/// and flag 0xA1 is 1 - calls `func_mine_tunnel_8017D6E0` with 2. Then sets
/// scene music entry 1 and advances to state 1.
static void func_mine_tunnel_8017D6EC(Task* arg0)
{
    arg0->msgTable = D_mine_tunnel_8017DFC4;
    Game_SetPtrSlot(arg0, 7);
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0xA1) == 1)) {
        func_mine_tunnel_8017D6E0(2);
    }
    arg0->state           = (s32)(arg0->state + 1);
    gStageSceneMusicEntry = 1;
}

/// State 1 of the room's event task: does nothing, so the task idles here.
static void func_mine_tunnel_8017D774(Task* task)
{
}

/// The room event task's three states: install the message table, idle, and
/// kill.
static const TaskFuncTable3 D_mine_tunnel_8017D5C4 = {
    {
        func_mine_tunnel_8017D6EC,
        func_mine_tunnel_8017D774,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_mine_tunnel_8017D77C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_tunnel_8017D5C4;
    sp.funcs[task->state](task);
}
