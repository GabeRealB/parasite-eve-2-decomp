#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"
#include "main/unknown_syms.h"

extern s8       D_801153F1;
extern TaskDesc RoomsShared8017e5b8Desc;
extern s32      D_acropolis_helicopter_landing_pad_80184E50;
extern s32      D_acropolis_helicopter_landing_pad_80187F84;

void func_acropolis_helicopter_landing_pad_8017E5E8(void)
{
    Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 1, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 2, 0, arg0 | (arg1 << 8));
}

void func_acropolis_helicopter_landing_pad_8017E64C(void)
{
    Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 5, 0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E67C(void)
{
    if (D_acropolis_helicopter_landing_pad_80187F84 != 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&D_acropolis_helicopter_landing_pad_80184E50, 0);
    }
}

void func_acropolis_helicopter_landing_pad_8017E6C0(s32 arg0)
{
    Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 6, arg0, 0);
}

void func_acropolis_helicopter_landing_pad_8017E6F0(void)
{
    Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(0), 0x1B);
    D_801153F1 = 3;
}
