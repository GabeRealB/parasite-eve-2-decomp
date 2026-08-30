#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Spawn payload handed to `D_shelter_b1_underground_parking_8018720C` as
/// `Task_SpawnFromTable` arg3 by the day-13 branch of
/// `func_shelter_b1_underground_parking_80182A60`. `field_0` is the script id
/// and `field_4`..`field_10` are the four cap keys it replays.
typedef struct {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   field_3;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
    /* 0x14 */ byte unknown_14[0xC];
} ShelterParkingSpawnArg;

extern s32                    D_shelter_b1_underground_parking_8018D758;
extern ShelterParkingSpawnArg D_shelter_b1_underground_parking_8018D75C;
extern TaskDesc               D_shelter_b1_underground_parking_8018720C[];
extern TaskDesc               D_shelter_b1_underground_parking_80187260[];
extern TaskDesc               D_shelter_b1_underground_parking_8018726C[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80182154);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_801826C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", D_shelter_b1_underground_parking_8017D7F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80182830);

s32 func_shelter_b1_underground_parking_80182A60(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    ShelterParkingSpawnArg* st;

    switch (arg2) {
        case 1:
            if (Game_Session->field_5 < 6) {
                if (GameFlag_GetNibble(0xC7) == 0) {
                    Gp_RunCapCmd1(1);
                } else if (GameFlag_GetNibble(0xE7) == 0) {
                    if (GameFlag_GetNibble(0xE8) == 0) {
                        Gp_RunCapCmd1(2);
                    } else {
                        Gp_MsgPlayerWeapon(0);
                        Gp_MsgPlayer3F3(0);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                    }
                } else {
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgPlayer3F3(0);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(0x10);
            }
            break;
        case 7:
            switch (Game_Session->field_5) {
                case 1:
                    Gp_RunCapCmd1(7);
                    break;
                case 2:
                    Gp_RunCapCmd1(8);
                    break;
                case 3:
                    Gp_RunCapCmd1(0xF);
                    break;
                case 4:
                    Gp_RunCapCmd1(9);
                    break;
                case 6:
                case 7:
                case 8:
                    Gp_RunCapCmd1(0xE);
                    break;
            }
            break;
        case 4:
            switch (Game_Session->field_5) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    Gp_RunCapCmd1(4);
                    break;
                case 6:
                    Gp_RunCapCmd1(5);
                    break;
                case 7:
                    Gp_RunCapCmd1(6);
                    break;
                case 8:
                    Gp_RunCapCmd1(0x10);
                    break;
            }
            break;
        case 2:
        case 3:
        case 5:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, arg2, 0);
            break;
        case 13:
            st           = &D_shelter_b1_underground_parking_8018D75C;
            st->field_4  = 0x5414000B;
            st->field_8  = 0x5414000E;
            st->field_10 = 0x5414000C;
            st->field_C  = 0x5414000D;
            st->field_0  = 0x14;
            if (D_shelter_b1_underground_parking_8018D758 == 0) {
                if (GameFlag_GetNibble(0x158) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    GameFlag_SetNibble(0x158, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 1, 0);
                } else {
                    st->field_1 = 1;
                    st->field_3 = 1;
                    st->field_2 = 0;
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, 9, (s32)st);
                }
            } else {
                D_shelter_b1_underground_parking_8018D758 = 0;
                st->field_1                               = 0x1F;
                st->field_3                               = 0;
                st->field_2                               = 1;
                Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 0, arg2, (s32)st);
            }
            break;
        case 22:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_80187260, 0, arg2, 0);
            break;
        case 48:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 6, 4, 0);
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80182DB4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80182FC8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80183124);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_801831F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80183284);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80183360);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_801833DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80183410);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_801834D4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_80183560);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_8018363C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_801836D8);

void func_shelter_b1_underground_parking_80183714(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x300, 0);
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", func_shelter_b1_underground_parking_801837D8);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", D_shelter_b1_underground_parking_8017D9A4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", jtbl_shelter_b1_underground_parking_8017D9C4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_5", jtbl_shelter_b1_underground_parking_8017D9F4);
