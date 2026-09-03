#include "common.h"
#include "main/task.h"
#include "main/display.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"

extern Task* D_acropolis_fountain_80183BB4;

extern GpMsgEntry D_acropolis_fountain_8017E764[];

extern GpObj4A D_acropolis_fountain_8017E7A4;
extern GpObj4A D_acropolis_fountain_8017FB3C;

void func_acropolis_fountain_8017DA1C(void);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D77C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D7F4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017D868);

void func_acropolis_fountain_8017D960(Task* arg0)
{
    arg0->field_24 = D_acropolis_fountain_8017E764;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x12) != 0) {
        func_acropolis_fountain_8017DA1C();
    }
    arg0->state = (s32)(arg0->state + 1);
}

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

void func_acropolis_fountain_8017DAA4(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    slot         = Game_GetPtrSlot(3);
    msg.field_10 = 0;
    msg.field_12 = 0x800;
    msg.field_14 = 0;
    Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_fountain_8017DB00(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_acropolis_fountain_8017DB54(Task* arg0)
{
    s32   args[2];
    Task* slot;

    slot    = Game_GetPtrSlot(3);
    args[0] = 0;
    args[1] = 1;
    Gp_DispatchMsg(slot, 0x3EF, (s32)args, 0);
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DBAC);

void func_acropolis_fountain_8017DC00(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    Game_GetPtrSlot(3);
    slot        = Game_GetPtrSlot(3);
    msg.field_0 = 0xA27;
    msg.field_4 = -0xC8;
    msg.field_8 = -0x17A6;
    Gp_DispatchMsg(slot, 0x3F2, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

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
