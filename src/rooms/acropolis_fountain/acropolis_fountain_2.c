#include "common.h"
#include "main/task.h"
#include "main/display.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
extern Task* D_acropolis_fountain_80183BB4;

extern GpObj4A D_acropolis_fountain_8017E7A4;
extern GpObj4A D_acropolis_fountain_8017FB3C;

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D77C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D7F4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D868);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D960);

void func_acropolis_fountain_8017D9BC(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D9C4);

void func_acropolis_fountain_8017DA1C(void)
{
    Gp_UnlinkObj4A(0, &D_acropolis_fountain_8017FB3C);
    D_acropolis_fountain_8017E7A4.field_8 = &Gfx_ViewCoord;
    Gp_LinkObj4A(0, &D_acropolis_fountain_8017E7A4);
    D_acropolis_fountain_8017E7A4.field_4A |= 0x40;
}

void func_acropolis_fountain_8017DA78(void)
{
    Task_Spawn(2, 0xE, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DAA4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DB00);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DB54);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DBAC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DC00);

void func_acropolis_fountain_8017DC6C(Task* arg0)
{
    Task* temp_v0;

    temp_v0 = Game_GetPtrSlot(3);
    if (Gp_DispatchMsg(temp_v0, 0x3F0, 0, 0) == 0) {
        Gp_DispatchMsg(temp_v0, 0x3F1, 0, 0);
        Task_Kill(arg0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DCD4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DD44);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017E014);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017E15C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017E3D4);

void func_acropolis_fountain_8017E72C(Task* arg0)
{
    Task_Kill(D_acropolis_fountain_80183BB4);
    Task_Kill(arg0);
}
