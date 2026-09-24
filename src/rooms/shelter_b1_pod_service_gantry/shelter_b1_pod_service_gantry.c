#include "common.h"

#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

/// Work block of the task below: the child task the current state spawned,
/// and the state it dispatches on.
typedef struct {
    Task* child;
    s16   state;
} _GantryWork;

extern s16            D_80071076;
extern TaskDesc       D_8013FB50;
extern TaskDesc       D_8016EA28;
extern TaskDesc       D_801718F0;
extern GpAreaApplyRec D_shelter_b1_pod_service_gantry_80182540;
INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_pod_service_gantry/shelter_b1_pod_service_gantry", D_shelter_b1_pod_service_gantry_8017D5C4);

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
