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

extern GpAreaApplyRec RoomsShared80181228AreaRecs;
extern Task*          RoomsShared80181228Task;

extern TaskDesc RoomsShared80181228Desc;
extern s32      D_mist_parking_80186EFC;
extern TaskDesc RoomsShared8017e5b8Desc;
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
                    Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 8, 0, 0);
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
            Task_SpawnFromTable(&RoomsShared80181228Desc, 0, 4, (s32)&D_mist_parking_8019533C);
            session          = Game_Session;
            D_80072170       = 2;
            session->field_8 = 2;
            break;
        case 18:
            Gp_MsgPlayerWeapon(0);
            if (Game_Session->field_9 == 1) {
                Task_SpawnFromTable(&RoomsShared8017daf0Desc, 3, 0, 0);
            } else {
                Task_SpawnFromTable(&RoomsShared8017e5b8Desc, 7, 0, 0);
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

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_5", D_mist_parking_8017D7DC);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_5", D_mist_parking_8017D7F4);
