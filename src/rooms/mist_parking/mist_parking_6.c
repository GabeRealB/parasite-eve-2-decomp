#include "common.h"

#include <psyq/libgte.h>

#include "decomp/common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern s8 D_80072170;

extern u8  D_801153F4;
extern s16 D_80114D08;
extern u32 D_80115694;

extern GpAreaApplyRec D_mist_parking_80188888;
extern Task*          D_mist_parking_80195318;

extern TaskDesc D_mist_parking_801869B8;
extern s32      D_mist_parking_80186EFC;
extern TaskDesc D_mist_parking_8018D75C;
extern s32      D_mist_parking_8018F0A4;
extern s32      D_mist_parking_8018F194;
extern TaskDesc RoomsShared8017daf0Desc;

typedef struct _MistParkingSpawnRec {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} MistParkingSpawnRec;

extern MistParkingSpawnRec D_mist_parking_8019533C;

void func_mist_parking_80181E8C(Task* task)
{
    MistParkingSpawnRec* rec;
    s32                  killOut;
    s32                  flag;
    s32                  cmd;
    s32                  fadeA;
    s32                  fadeB;

    rec = (MistParkingSpawnRec*)task->spawnArg2;
    switch (task->state) {
        case 0:
            D_mist_parking_80195318 = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData.field_13 == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694          = Mc_SaveData.field_4;
                Mc_SaveData.field_4 = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            Game_Session->field_68 = 1;
            Game_Session->field_1  = 1;
            D_801153F4             = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_mist_parking_80195318 = Task_SpawnFromTable(&D_mist_parking_801869B8, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                Task_Kill(D_mist_parking_80195318);
                task->state++;
            } else if (Task_PollKill(D_mist_parking_80195318, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((*(u32*)&Mc_SaveData.field_4 & 0xFFFF0000) == 0x1010000) {
                        Gp_ApplyAreaRecs(&D_mist_parking_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData.field_4 = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData.field_13 == 1) {
                Gp_MsgAllyWeapon(1);
            }
            Game_Session->field_68 = 0;
            Game_Session->field_1  = 0;
            D_801153F4             = 0;
            if (rec->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            Task_Kill(task);
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

s32 func_mist_parking_801823F8(s32 arg0, s32 arg1, s32 arg2)
{
    GameSession* session;
    u8           temp;

    switch (arg2) {
        case 15:
            temp = Game_Session->field_9;
            if (temp == 2) {
                if (GameFlag_GetNibble(0xF1) == 1) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614((s32)&D_mist_parking_8018F0A4, 1);
                    GameFlag_SetNibble(0xF1, 2);
                } else if (GameFlag_GetNibble(0xF1) == temp) {
                    Gp_MsgPlayerWeapon(0);
                    func_800E8614((s32)&D_mist_parking_8018F194, 1);
                    GameFlag_SetNibble(0xF1, 3);
                } else if (GameFlag_GetNibble(0xF1) == 3) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(&D_mist_parking_8018D75C, 8, 0, 0);
                }
            } else if (GameFlag_GetNibble(0xED) == 1) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&RoomsShared8017daf0Desc, 4, 0, 0);
            }
            break;
        case 8:
            D_mist_parking_8019533C.field_0  = 9;
            D_mist_parking_8019533C.field_1  = 1;
            D_mist_parking_8019533C.field_3  = 3;
            D_mist_parking_8019533C.field_2  = 0;
            D_mist_parking_8019533C.field_4  = 0x51130003;
            D_mist_parking_8019533C.field_8  = 0x51130004;
            D_mist_parking_8019533C.field_10 = 0x5113000B;
            D_mist_parking_8019533C.field_C  = 0x51130012;
            Task_SpawnFromTable(&D_mist_parking_801869B8, 0, 4, (s32)&D_mist_parking_8019533C);
            session          = Game_Session;
            D_80072170       = 2;
            session->field_8 = 2;
            break;
        case 18:
            Gp_MsgPlayerWeapon(0);
            if (Game_Session->field_9 == 1) {
                Task_SpawnFromTable(&RoomsShared8017daf0Desc, 3, 0, 0);
            } else {
                Task_SpawnFromTable(&D_mist_parking_8018D75C, 7, 0, 0);
            }
            break;
        case 1:
            func_800E8614((s32)&D_mist_parking_80186EFC, 1);
            break;
    }
    return 0;
}

void func_mist_parking_80182628(Task* task)
{
    s32 zero;

    switch (task->state) {
        case 0x50:
        case 0x0:
            zero = 0;
            TOUCH_REG(zero);
            SndEvt_EnqueueType6((s32)task->spawnArg2, zero, zero);
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            return;
    }
    task->state += 1;
}

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_6", D_mist_parking_8017D7DC);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_6", D_mist_parking_8017D7F4);
