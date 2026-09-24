#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "gameplay/1A8.h"
#include "main/gameflag.h"
#include "rooms/rooms_shared_80181228.h"
#include "rooms/shelter_b1_sterilization_room.h"

extern void                   func_800E8634(s32 arg0, s32 arg1, s32 arg2);
extern TaskDesc               D_shelter_b1_sterilization_room_80188504[];
extern s32                    D_shelter_b1_sterilization_room_8018873C;
extern s32                    D_shelter_b1_sterilization_room_80188AB4;
extern s32                    D_80135AC0;
extern s32                    D_80135D78;
extern s32                    D_80136258;
extern s32                    D_shelter_b1_sterilization_room_80184E40;
extern s32                    D_shelter_b1_sterilization_room_80184E7C;
extern GpObj4A                D_shelter_b1_sterilization_room_8018BF30[];
extern GpAreaApplyRec         D_shelter_b1_sterilization_room_8018C334;
extern TaskDesc               D_shelter_b1_sterilization_room_80184E70;
extern s32                    D_shelter_b1_sterilization_room_80188ED4;
extern s32                    D_shelter_b1_sterilization_room_80188FDC;
extern RoomsShared80181228Rec D_shelter_b1_sterilization_room_8018C344;

void func_shelter_b1_sterilization_room_80180340(s32 arg0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room", D_shelter_b1_sterilization_room_8017D6A4);

void func_shelter_b1_sterilization_room_8017FABC(Task* task)
{
    Task* target;

    task->msgTable = &D_shelter_b1_sterilization_room_80184E40;
    Game_SetPtrSlot(task, 7);
    if (gGameSession->at4.loc.place == 5 && GameFlag_GetNibble(0xEA) == 0) {
        GameFlag_SetNibble(0xF4, 3);
        Gp_ApplyAreaRecs(&D_shelter_b1_sterilization_room_8018C334);
        if (gameGetPtrSlot(0xA) != NULL) {
            GameFlag_SetNibble(0x116, 1);
            GameFlag_SetNibble(0xEA, 2);
            GameFlag_SetNibble(0x4B, 8);
            func_800E8634((s32)&D_80135D78, 0, (s32)&D_80136258);
            Gp_SetAreaObjId((GpAreaKey*)&gGameSession->at4.loc, 6, 1);
        } else {
            GameFlag_SetNibble(0x116, 2);
            GameFlag_SetNibble(0xEA, 1);
            func_800E8634((s32)&D_80135AC0, 0, (s32)&D_80136258);
        }
    }
    func_shelter_b1_sterilization_room_80180340(0);
    if (gGameSession->at4.loc.place == 5) {
        target = (Task*)Gp_LookupSlot4(0);
        if (target != NULL) {
            Gp_DispatchMsg(target, 0x7DB, (s32)&D_shelter_b1_sterilization_room_80184E7C, 0);
        }
    }
    if (GameFlag_GetNibble(0xEA) != 1) {
        D_shelter_b1_sterilization_room_8018BF30[0].field_4A &= 0xBF;
    }
    if (gGameSession->at4.loc.place == 1) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 6, 0, 0);
    }
    task->state++;
}

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

s32 func_shelter_b1_sterilization_room_8017FF80(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x11) {
        if (GameFlag_GetNibble(0x161) == 0) {
            GameFlag_SetNibble(0x161, 1);
            Gp_SpawnIfCapIdle(0x18, 1);
            return;
        }
        D_shelter_b1_sterilization_room_8018C344.field_0  = 0x13;
        D_shelter_b1_sterilization_room_8018C344.field_1  = 1;
        D_shelter_b1_sterilization_room_8018C344.field_3  = 2;
        D_shelter_b1_sterilization_room_8018C344.field_2  = 0;
        D_shelter_b1_sterilization_room_8018C344.field_4  = 0x5410000C;
        D_shelter_b1_sterilization_room_8018C344.field_8  = 0x5410000F;
        D_shelter_b1_sterilization_room_8018C344.field_10 = 0x5410000D;
        D_shelter_b1_sterilization_room_8018C344.field_C  = 0x5410000E;
        Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80184E1C, 0, 6, (s32)&D_shelter_b1_sterilization_room_8018C344);
    }
    if (arg2 == 0x15) {
        Gp_RunCapCmd1(arg2);
    }
    if (arg2 == 4) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 0, 0);
    }
    if (arg2 == 0x13) {
        Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 7, 1, 0);
    }
    if (arg2 == 0xE) {
        if (GameFlag_GetNibble(0x76) == 1 && GameFlag_GetNibble(0x77) == 0) {
            if (GameFlag_GetNibble(0x14F) == 0) {
                Gp_MsgPlayerWeapon(0);
                func_800E8634((s32)&D_shelter_b1_sterilization_room_80188ED4, 0, (s32)&D_shelter_b1_sterilization_room_80188FDC);
                GameFlag_SetNibble(0x14F, 1);
            } else {
                Task_SpawnFromTable(D_shelter_b1_sterilization_room_80188504, 8, 0xB, 0);
                GameFlag_SetNibble(0x14F, 2);
            }
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xC || arg2 == 0xD) {
        if (gGameSession->at4.loc.room == 3) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80184E70, 0, arg2, 0);
        }
    }
    return 0;
}

void func_shelter_b1_sterilization_room_80180188(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x2C0, 0x100);
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
            task->state++;
            break;
        case 1:
            task->state++;
            break;
        case 2:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 3:
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 5:
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
            task->state++;
            break;
        case 6:
            task->state++;
            break;
        case 7:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                task->state++;
            }
            break;
        case 8:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room", D_shelter_b1_sterilization_room_8017D700);
