#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_acropolis_east_elevator_hall_80185C8C;
extern s32 D_acropolis_east_elevator_hall_8018621C;
extern s32 D_acropolis_east_elevator_hall_801862F4;

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F378);

s32 func_acropolis_east_elevator_hall_8017F420(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        func_800E8614((s32)&D_acropolis_east_elevator_hall_8018621C, 0);
    }
    return 0;
}

void func_acropolis_east_elevator_hall_8017F450(void)
{
    Gp_StartCapSlot(0x10, 1, 0);
}

void func_acropolis_east_elevator_hall_8017F478(Task* task)
{
    task->field_24 = &D_acropolis_east_elevator_hall_801862F4;
    Game_SetPtrSlot(task, 7);
    Gp_MsgSlot4Chain(0, 1);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_acropolis_east_elevator_hall_80185C8C, 0);
    task->state++;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F4E8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F55C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F5B4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F77C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017FAAC);
