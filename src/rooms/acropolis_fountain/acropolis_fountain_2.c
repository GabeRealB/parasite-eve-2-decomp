#include "common.h"
#include "main/task.h"
#include "main/display.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/acropolis_fountain.h"

extern s16 D_80071076;

extern u8    D_acropolis_fountain_80183BB0;
extern Task* D_acropolis_fountain_80183BB4;

extern GpMsgEntry D_acropolis_fountain_8017E764[];

extern GpObj4A D_acropolis_fountain_8017E7A4;
extern GpObj4A D_acropolis_fountain_8017FB3C;

extern SVECTOR  D_acropolis_fountain_8017E7F0;
extern TaskDesc D_acropolis_fountain_8017E7FC;

void func_acropolis_fountain_8017DA1C(void);

s32 func_acropolis_fountain_8017D77C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 args[2];

    if (arg2 == 3) {
        Gp_RunCapCmd1(((GameFlag_GetNibble(9) & 2) == 0) ? 3 : 6);
    }
    if (arg2 == 4) {
        Gp_StartCapSlot(4, 1, 0);
        func_acropolis_fountain_8017DA1C();
        GameFlag_SetNibble(0x12, 1);
    }
    return 0;
}

s32 func_acropolis_fountain_8017D7F4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            SndEvt_EnqueueType6(0x51080003, 0, 0);
            break;
        case 4:
            SndEvt_EnqueueType6(0x51080004, 0, 0);
            break;
        case 9:
            SndEvt_EnqueueType6(0x51080009, 0, 0);
            break;
    }
    return 0;
}

void func_acropolis_fountain_8017D868(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(1);
            task->state = task->state + 1;
            break;

        case 1:
            if (Gp_CapBusy() == 0) {
                task->state = task->state + 1;
            }
            /* fallthrough */

        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_6 = 3;
            Mc_SaveData.field_5 = 3;
            Mc_SaveData.field_8 = D_acropolis_fountain_80183BB0;
            D_80071076          = 1;
            Task_Spawn(0, 0x11, 0, 0);
            GameFlag_SetNibble(0, 5);
            Task_Kill(task);
            break;
    }
}

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

void func_acropolis_fountain_8017DBAC(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

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

/// Six-state dispatcher of the fountain cutscene task; the handler table is
/// built on the stack from the overlay's rodata block.
void func_acropolis_fountain_8017DCD4(Task* arg0)
{
    TaskFunc states[6] = {
        func_acropolis_fountain_8017DAA4,
        func_acropolis_fountain_8017DB00,
        func_acropolis_fountain_8017DB54,
        func_acropolis_fountain_8017DBAC,
        func_acropolis_fountain_8017DC00,
        func_acropolis_fountain_8017DC6C,
    };

    states[arg0->state](arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017DD44);

void func_acropolis_fountain_8017E014(Task* task)
{
    AcropolisFountainSplash* splash;
    GsCOORDINATE2*           coord;
    s32                      view;
    s32                      one;
    s32                      mask;
    s32                      bit;
    s16                      id;

    coord  = ((TmdObject*)task->extra)->field_8;
    splash = task->spawnArg2;
    view   = Gp_GetViewIndex();
    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60088, coord, 0, &D_acropolis_fountain_8017E7F0);
            splash->viewIndex = view & 0xFF;
            task->state       = 1;
            /* fallthrough */
        case 1:
            mask = 0x100FE;
            bit  = 1 << (splash->viewIndex - 1);
            if (bit & mask) {
                Task_SpawnFromTable(&D_acropolis_fountain_8017E7FC, 0, 0, 0);
            }
            task->state = 2;
            break;
        case 2:
            one = 1;
            id  = splash->viewIndex;
            bit = one << (id - 1);
            if (id != (view & 0xFF)) {
                if (bit & 0x100FE) {
                    Task_SpawnFromTable(&D_acropolis_fountain_8017E7FC, 1, 0, 0);
                }
                splash->viewIndex = (u8)view;
                task->state       = 1;
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017E15C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fountain/acropolis_fountain_2", func_acropolis_fountain_8017E3D4);

void func_acropolis_fountain_8017E72C(Task* arg0)
{
    Task_Kill(D_acropolis_fountain_80183BB4);
    Task_Kill(arg0);
}
