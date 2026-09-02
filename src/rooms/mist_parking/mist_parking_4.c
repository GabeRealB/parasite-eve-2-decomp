#include "common.h"

#include <psyq/libgte.h>

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern s8 D_80072170;

extern TaskDesc D_mist_parking_801869B8;
extern s32      D_mist_parking_80186EFC;
extern TaskDesc D_mist_parking_8018D75C;
extern s32      D_mist_parking_8018F0A4;
extern s32      D_mist_parking_8018F194;
extern TaskDesc RoomsShared8017daf0Desc;

typedef struct _MistParkingSpawnRec {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  field_3;
    /* 0x04 */ u32 field_4;
    /* 0x08 */ u32 field_8;
    /* 0x0C */ u32 field_C;
    /* 0x10 */ u32 field_10;
    /* 0x14 */ u32 field_14;
} MistParkingSpawnRec;

extern MistParkingSpawnRec D_mist_parking_8019533C;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_4", func_mist_parking_80181E8C);

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

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_4", func_mist_parking_80182628);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_4", D_mist_parking_8017D7DC);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_4", D_mist_parking_8017D7F4);
