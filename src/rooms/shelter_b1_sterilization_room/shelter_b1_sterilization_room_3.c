#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

extern TaskDesc D_shelter_b1_sterilization_room_80188504[];
extern s32      D_shelter_b1_sterilization_room_8018873C;
extern s32      D_shelter_b1_sterilization_room_80188AB4;

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_8017FABC);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", D_shelter_b1_sterilization_room_8017D6A4);

s32 func_shelter_b1_sterilization_room_8017FC78(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    s32 cmd;
    s32 mask;
    s32 flags;

    switch (msg->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x76) == 0) {
                if (GameFlag_GetNibble(0x84) != 0) {
                    func_800E8634((s32)&D_shelter_b1_sterilization_room_8018873C, 0,
                                  (s32)&D_shelter_b1_sterilization_room_80188AB4);
                    GameFlag_SetNibble(0x76, 1);
                }
            }
            break;
        case 2:
            if (GameFlag_GetNibble(0x76) == 1) {
                if (GameFlag_GetNibble(0x77) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 1, 0, 0);
                } else {
                    Gp_RunCapCmd1(0x17);
                }
            } else {
                Gp_RunCapCmd1(0x16);
            }
            break;
        case 5:
        case 8:
            switch (msg->field_2) {
                case 5:
                    cmd  = 5;
                    mask = 2;
                    break;
                case 8:
                    cmd  = 3;
                    mask = 8;
                    break;
                default:
                    cmd  = 0;
                    mask = 0xFF;
                    break;
            }
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                flags = GameFlag_GetNibble(0xF2);
                if (cmd != 0 && !(flags & mask)) {
                    Gp_RunCapCmd1(cmd);
                    GameFlag_SetNibble(0xF2, flags | mask);
                }
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 3:
        case 6:
        case 7:
        case 10:
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 4, msg->field_2 - 3, 0);
            } else {
                Gp_RunCapCmd1(0xA);
            }
            break;
        case 9:
            if (GameFlag_GetNibble(0x76) == 0 || GameFlag_GetNibble(0x77) == 1) {
                if (GameFlag_GetNibble(0x149) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 1, 0);
                } else {
                    if (GameFlag_GetNibble(0x150) == 0) {
                        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 2, 0);
                        GameFlag_SetNibble(0x150, 1);
                    }
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
                }
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x77) == 0) {
                if (GameFlag_GetNibble(0x151) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 3, 0);
                    GameFlag_SetNibble(0x151, 1);
                }
            } else if (GameFlag_GetNibble(0x151) < 2) {
                if (GameFlag_GetNibble(0x14A) == 0) {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 4, 0);
                } else {
                    Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 5, 0);
                }
                GameFlag_SetNibble(0x151, 2);
            }
            Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 2, msg->field_2 - 3, 0);
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_8017FF80);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180188);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801802B0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180340);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801803E4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801803EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180430);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180464);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_8018049C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180518);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180570);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180828);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180A2C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180BF0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180D74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80180F74);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_8018118C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801811E0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80181244);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801812A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80181308);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801813A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801814B0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801814FC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80181588);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801815EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80181634);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80181658);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80181698);

void func_shelter_b1_sterilization_room_801816E0(Task* task)
{
    s32 cmd;
    s32 flag;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            if (task->spawnArg1 != 0) {
                flag = GameFlag_GetNibble(0x77);
                cmd  = 8;
                if (flag == 0) {
                    cmd = 7;
                }
                Gp_RunCapCmd1(cmd);
                GameFlag_SetNibble(0x149, 1);
            } else {
                flag = GameFlag_GetNibble(0x77);
                cmd  = 6;
                if (flag != 0) {
                    GameFlag_SetNibble(0x14A, 1);
                    GameFlag_SetNibble(0x151, 1);
                    cmd = 9;
                }
                Gp_RunCapCmd1(cmd);
            }
            task->state++;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                Task_Kill(task);
            }
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801817EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_8018188C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801823D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_801826F0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80182B34);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80183378);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80183718);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_3", func_shelter_b1_sterilization_room_80183B8C);
