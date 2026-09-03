#include "common.h"
#include "gameplay/gameplay.h"
#include "gameplay/1A8.h"
#include "main/display.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

extern s8        D_8007106B;
extern TaskDesc  D_acropolis_helicopter_landing_pad_80184E68;
extern GpSaveLoc D_acropolis_helicopter_landing_pad_80187F90;

void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0);
void func_acropolis_helicopter_landing_pad_8017EE2C(Task* arg0);

void func_acropolis_helicopter_landing_pad_8017ED00(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_helicopter_landing_pad_80184E68, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

/// Asks the slot-7 task to warp to stage 0xF, room 3 (message 0x13EE with the
/// room's `GpSaveLoc`); advances on success, otherwise kills the task.
void func_acropolis_helicopter_landing_pad_8017ED50(Task* arg0)
{
    Task* slot = Game_GetPtrSlot(7);

    D_acropolis_helicopter_landing_pad_80187F90.field_4 = 1;
    D_acropolis_helicopter_landing_pad_80187F90.field_3 = 1;
    *(u16*)&D_acropolis_helicopter_landing_pad_80187F90 = 0xF;
    D_acropolis_helicopter_landing_pad_80187F90.field_2 = 3;
    D_acropolis_helicopter_landing_pad_80187F90.field_5 = 0;
    if (Gp_DispatchMsg(slot, 0x13EE, (s32)&D_acropolis_helicopter_landing_pad_80187F90,
                       (s32)&D_acropolis_helicopter_landing_pad_80187F90) != 0) {
        arg0->state += 1;
    } else {
        Task_Kill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EDD4(Task* arg0)
{
    GpMsg3EE msg;
    Task*    slot;

    slot         = Game_GetPtrSlot(3);
    msg.field_10 = 0;
    msg.field_12 = 0;
    msg.field_14 = 0;
    Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_8017EE2C);

void func_acropolis_helicopter_landing_pad_8017EE80(Task* arg0)
{
    s32   args[2];
    Task* slot;

    slot    = Game_GetPtrSlot(3);
    args[0] = 1;
    args[1] = 3;
    Gp_DispatchMsg(slot, 0x3EF, (s32)args, 0);
    arg0->state = arg0->state + 1;
}

void func_acropolis_helicopter_landing_pad_8017EEDC(Task* arg0)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F0, 0, 0) == 0) {
        Mc_SaveData.field_6 = D_acropolis_helicopter_landing_pad_80187F90.field_0;
        Mc_SaveData.field_8 = D_acropolis_helicopter_landing_pad_80187F90.field_2;
        Mc_SaveData.field_5 = D_acropolis_helicopter_landing_pad_80187F90.field_3;
        Task_Spawn(0, 0x11, 0, 0);
        Task_Kill(arg0);
    }
}

void func_acropolis_helicopter_landing_pad_8017EF60(void)
{
    Task_Spawn(2, 0xF, 0, 0);
}

/// Five-state dispatcher of the room's intro task; the handler table is built
/// on the stack. Marks the player actor's `field_930` as 2 before every step.
void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0)
{
    GameActor* actor     = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    TaskFunc   states[5] = {
        func_acropolis_helicopter_landing_pad_8017ED50,
        func_acropolis_helicopter_landing_pad_8017EDD4,
        func_acropolis_helicopter_landing_pad_8017EE2C,
        func_acropolis_helicopter_landing_pad_8017EE80,
        func_acropolis_helicopter_landing_pad_8017EEDC,
    };

    actor->field_930 = 2;
    states[arg0->state](arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_8017F010);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_8017FA30);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_801802E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80180664);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80180A64);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80180E40);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80181064);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_801818F0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_801819C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_5", func_acropolis_helicopter_landing_pad_80181B64);
