#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"

extern u8 D_80071075;
extern s8 D_8007218A;
extern s8 D_8007272D;
extern u8 D_80073BA9;
extern s8 D_80114C12;
extern u8 D_shelter_b1_pod_access_tunnel_80181120[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_2", func_shelter_b1_pod_access_tunnel_8017DEE8);

/// Two-state task: state 0, unless blocked by `D_80114C12` or `D_80071075`,
/// sends the slot-3 task a `GpRec14` built from `D_80073BA9` (msg 0x3E8) and runs
/// `D_shelter_b1_pod_access_tunnel_80181120` through `func_800E8614`; state 1
/// sets `D_8007272D` to 0x1D and kills this task once the session is idle.
void func_shelter_b1_pod_access_tunnel_8017DF40(Task* task)
{
    GpRec14 rec;
    s32     state;
    s32     weaponId;
    s32     id;

    state = task->state;
    switch (state) {
        case 0:
            if (D_80114C12 != 1 && D_80071075 == 0) {
                weaponId     = D_80073BA9;
                id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.field_0  = id;
                rec.field_4  = 1;
                rec.field_8  = 0;
                rec.field_C  = 0;
                rec.field_10 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
                func_800E8614((s32)D_shelter_b1_pod_access_tunnel_80181120, 0);
                task->state = task->state + 1;
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                D_8007272D = 0x1D;
                Task_RequestKill(task, 0);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_pod_access_tunnel/shelter_b1_pod_access_tunnel_2", func_shelter_b1_pod_access_tunnel_8017E048);
