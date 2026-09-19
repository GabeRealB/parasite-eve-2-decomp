#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s16 D_80071076;

extern s32            D_acropolis_cafeteria_8018363C;
extern s32            D_acropolis_cafeteria_80183DBC;
extern GpAreaApplyRec D_acropolis_cafeteria_8018C9D4[];

void func_acropolis_cafeteria_8017DD1C(Task* task)
{
    char pad[8];

    switch (task->state) {
        case 0:
            if (Gp_GetCurBit2Flag(3) == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
                task->state = task->state + 1;
            } else {
                taskKill(task);
            }
            break;

        case 1:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_RunCapCmd1(3);
                task->state = task->state + 1;
            }
            break;

        case 2:
            if (Gp_CapBusy() == 0) {
                if (Gp_GetCurBit2Flag(3) == 1) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
                    task->state = task->state + 1;
                } else {
                    task->state = 6;
                }
            }
            break;

        case 3:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;

        case 6:
            func_800E8634((s32)&D_acropolis_cafeteria_8018363C, 0, (s32)&D_acropolis_cafeteria_80183DBC);
            task->state = task->state + 1;
            break;

        case 7:
            if (gGameSession->eventState != 1) {
                task->state = task->state + 1;
            }
            break;

        case 8:
            GameFlag_SetNibble(0, 2);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 4);
            GameFlag_SetNibble(0xE, 1);
            Gp_ApplyAreaRecs(D_acropolis_cafeteria_8018C9D4);
            Mc_SaveData.field_5C5 = 4;
            func_800E3FAC(0xA2, 4);
            func_800ABFF8();
            func_800AC000();
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.stage = 1;
            Mc_SaveData.at4.loc.area  = 3;
            Mc_SaveData.at4.loc.warp  = 3;
            Mc_SaveData.at4.loc.room  = 3;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
