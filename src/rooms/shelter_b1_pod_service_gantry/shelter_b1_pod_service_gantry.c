#include "common.h"

#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Work block of the room task, allocated zeroed by its first state: the
/// child task the current step spawned, and the step it dispatches on.
typedef struct {
    Task* child;
    s16   state;
} _GantryWork;

extern s16            D_80071076;
extern TaskDesc       D_8013FB50;
extern TaskDesc       D_8016EA28;
extern TaskDesc       D_801718F0;
extern GpAreaApplyRec D_shelter_b1_pod_service_gantry_80182540;

/// The room's message table, published in `Task::msgTable`.
extern GpMsgEntry D_shelter_b1_pod_service_gantry_8017FAF4[];

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

void func_shelter_b1_pod_service_gantry_8017D628(Task* task);
void func_shelter_b1_pod_service_gantry_8017D81C(Task* arg0);

/// The room task's three states: set-up, the step sequence below, and exit.
const TaskFuncTable3 D_shelter_b1_pod_service_gantry_8017D5C4 = {
    { func_shelter_b1_pod_service_gantry_8017D81C, func_shelter_b1_pod_service_gantry_8017D628, taskKill },
};

void func_shelter_b1_pod_service_gantry_8017D628(Task* task)
{
    u8           param1[4];
    u8           param2[4];
    s32          poll;
    _GantryWork* work = task->work;

    switch (work->state) {
        case 0:
            work->child = Task_SpawnFromTable(&D_801718F0, 0, 0, 0);
            work->state++;
            break;
        case 1:
            if (Task_PollKill(work->child, &poll) == 0) {
                break;
            }
            work->child = Task_SpawnFromTable(&D_8016EA28, 0, 0, 0);
            work->state++;
            break;
        case 3:
            param1[2] = 0x10;
            param1[3] = 0;
            param1[0] = 0;
            param2[0] = 9;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
            goto next;
        case 4:
            if (CdCmd_IsIdle() == 0) {
                break;
            }
            work->child = Task_SpawnFromTable(&D_8013FB50, 0, 0, 0);
            Gp_ApplyAreaRecs(&D_shelter_b1_pod_service_gantry_80182540);
            GameFlag_SetNibble(0x118, 1);
            work->state++;
            break;
        case 5:
            if (Task_PollKill(work->child, &poll) == 0) {
                break;
            }
            gGameSession->unknown_138 = 1;
            Mc_SaveData.at4.loc.stage = 4;
            Mc_SaveData.at4.loc.area  = 0x11;
            Mc_SaveData.at4.loc.warp  = 2;
            Mc_SaveData.at4.loc.room  = 1;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
        case 2:
        next:
            work->state++;
            break;
        case 6:
            break;
    }
}

s32 func_shelter_b1_pod_service_gantry_8017D7C0(void)
{
    return 0;
}

/// Handler of message 0x13EE in the room's message table: copies the incoming
/// record onto the outgoing one, passes both to `func_80179A04` and returns 1.
s32 func_shelter_b1_pod_service_gantry_8017D7C8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

s32 func_shelter_b1_pod_service_gantry_8017D80C(void)
{
    return 0;
}

s32 func_shelter_b1_pod_service_gantry_8017D814(void)
{
    return 0;
}

void func_shelter_b1_pod_service_gantry_8017D81C(Task* arg0)
{
    _GantryWork* work;

    arg0->msgTable = D_shelter_b1_pod_service_gantry_8017FAF4;
    Game_SetPtrSlot(arg0, 7);
    work       = Mem_Malloc(8U, false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0U, 8U);
    SetDispMask(0);
    arg0->state += 1;
}

/// The room task: copies its three-state table
/// `D_shelter_b1_pod_service_gantry_8017D5C4` onto the stack and calls the
/// entry for the task's current state.
void func_shelter_b1_pod_service_gantry_8017D89C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_service_gantry_8017D5C4;
    sp.funcs[task->state](task);
}
