#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Spawn payload handed to `RoomsShared80181228Desc` as
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
extern TaskDesc               RoomsShared80181228Desc[];
extern TaskDesc               D_shelter_b1_underground_parking_80187260[];
extern TaskDesc               D_shelter_b1_underground_parking_8018726C[];
extern u8                     D_80071075;
extern s8                     D_80114C12;

/// Starts caption slot 0xA and spawns entry 4 of
/// `D_shelter_b1_underground_parking_8018726C` when the player asks for it.
///
/// The player must not be aiming (`field_954 != 2`), captions must be idle,
/// the session room must be 7 or later, and the model root must stand with X
/// below -0x1266 and Z inside [-0x7CF, 0x7D0), with `D_80114C12 != 1` and
/// `D_80071075` clear. Then the 0x1000 pad mask with the yaw in the 0x3FF-wide
/// window opening at 0xA01, or the 0x4000 mask with it in the window at 0x201,
/// takes the weapon away and runs the handoff.
void func_shelter_b1_underground_parking_801826C0(void)
{
    Task*          task;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            z;
    s32            facing;

    task  = gameGetPtrSlot(3);
    actor = (GameActor*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    if ((actor->field_954 != 2) && (Gp_CapBusy() == 0) && (gGameSession->at4.loc.room >= 7) &&
        (coord->coord.t[0] < -0x1266)) {
        z = coord->coord.t[2];
        if (z < 0x7D0) {
            if ((z >= -0x7CF) && (D_80114C12 != 1) && (D_80071075 == 0)) {
                facing = (u16)actor->field_52 & 0xFFF;
                if (Pad_CheckButtons(0, 0, 0x1000) != 0) {
                    if ((u32)(facing - 0xA01) < 0x3FFU) {
                        Gp_MsgPlayerWeapon(0);
                        Gp_StartCapSlot(0xA, 0, 1);
                        Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                    }
                }
                if ((Pad_CheckButtons(0, 0, 0x4000) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                    Gp_MsgPlayerWeapon(0);
                    Gp_StartCapSlot(0xA, 0, 1);
                    Task_SpawnFromTable(D_shelter_b1_underground_parking_8018726C, 4, 0, 0);
                }
            }
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", func_shelter_b1_underground_parking_80182830);

s32 func_shelter_b1_underground_parking_80182A60(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    ShelterParkingSpawnArg* st;

    switch (arg2) {
        case 1:
            if (gGameSession->at4.loc.room < 6) {
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
            switch (gGameSession->at4.loc.room) {
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
            switch (gGameSession->at4.loc.room) {
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
                    Task_SpawnFromTable(RoomsShared80181228Desc, 0, 9, (s32)st);
                }
            } else {
                D_shelter_b1_underground_parking_8018D758 = 0;
                st->field_1                               = 0x1F;
                st->field_3                               = 0;
                st->field_2                               = 1;
                Task_SpawnFromTable(RoomsShared80181228Desc, 0, arg2, (s32)st);
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

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", func_shelter_b1_underground_parking_80182DB4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", func_shelter_b1_underground_parking_80182FC8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", func_shelter_b1_underground_parking_80183124);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_4", D_shelter_b1_underground_parking_8017D9A4);
