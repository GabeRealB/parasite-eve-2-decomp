#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern Task*      D_acropolis_helicopter_landing_pad_80187F80;
extern GpMsgEntry D_acropolis_helicopter_landing_pad_80183710[];
extern s32        D_acropolis_helicopter_landing_pad_80183A04;
extern TaskDesc   D_acropolis_helicopter_landing_pad_80184DA0[];
extern s32        D_acropolis_helicopter_landing_pad_80184D9C;
extern s32        D_acropolis_helicopter_landing_pad_80184E0C;
extern GpObj4A    D_acropolis_helicopter_landing_pad_80185FAC[];
extern s32        D_acropolis_helicopter_landing_pad_80187F84;
extern s32        D_acropolis_helicopter_landing_pad_801837E0[];

extern s32 D_801156A8;

void func_acropolis_helicopter_landing_pad_8017E75C(s32 arg0)
{
    D_acropolis_helicopter_landing_pad_80187F80->state = arg0;
}

void func_acropolis_helicopter_landing_pad_8017E76C(Task* task)
{
    s32 args[2];

    switch (task->state) {
        case 0:
            args[0] = 0xC;
            args[1] = 9;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F2, (s32)&D_acropolis_helicopter_landing_pad_801837E0, (s32)args);
            task->state++;
            break;
        case 1:
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
                Task_Kill(task);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_3", func_acropolis_helicopter_landing_pad_8017E81C);

void func_acropolis_helicopter_landing_pad_8017E974(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(4, 1, 0);
        case 2:
        case 3:
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_801156A8 == 1) {
                    Gp_MsgPlayerWeapon(1);
                    Task_Kill(task);
                    break;
                }
                Display_AcquireRef();
                task->state++;
            }
            task->state++;
            break;
        case 4:
            D_acropolis_helicopter_landing_pad_80184D9C = 3;
            Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 0, 0, 0);
            Task_Kill(task);
            break;
    }
}

void func_acropolis_helicopter_landing_pad_8017EA6C(Task* task)
{
    task->field_24 = D_acropolis_helicopter_landing_pad_80183710;
    Game_SetPtrSlot(task, 7);
    D_acropolis_helicopter_landing_pad_80187F84 = 0;
    D_acropolis_helicopter_landing_pad_80184E0C = 0;
    task->state++;
    func_800E8614((s32)&D_acropolis_helicopter_landing_pad_80183A04, 1);
    D_acropolis_helicopter_landing_pad_80187F80              = Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 7, 0, 0);
    D_acropolis_helicopter_landing_pad_80185FAC[0].field_4A &= 0xBF;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_3", func_acropolis_helicopter_landing_pad_8017EB00);
