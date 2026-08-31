#include "common.h"
#include "gameplay/1BC.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_shelter_b1_sterilization_room_80188C94;
extern s32 D_shelter_b1_sterilization_room_80188E14;

void func_shelter_b1_sterilization_room_801813A0(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(8);
            Game_Session->field_1 = 1;
            arg0->state           = arg0->state + 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                func_800E8634((s32)&D_shelter_b1_sterilization_room_80188C94, 0, (s32)&D_shelter_b1_sterilization_room_80188E14);
                GameFlag_SetNibble(0x77, 1);
                Game_Session->field_128 = 0;
            } else {
                Game_Session->field_1 = 0;
                Gp_MsgPlayerWeapon(1);
            }
            Task_Kill(arg0);
            return;
        default:
            Game_Session->field_1 = 0;
            Task_Kill(arg0);
            return;
    }
}

void func_shelter_b1_sterilization_room_801814B0(void)
{
    Mc_SaveData.field_6 = 0x27;
    Mc_SaveData.field_8 = 3;
    Mc_SaveData.field_5 = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_801814FC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_80181588);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_801815EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_80181634);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_80181658);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_80181698);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_801816E0);

void func_shelter_b1_sterilization_room_801817EC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_RunCapCmd1(task->spawnArg1);
            task->state = task->state + 1;
            /* fallthrough */
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                Task_Kill(task);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_8018188C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_801823D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_7", func_shelter_b1_sterilization_room_801826F0);
