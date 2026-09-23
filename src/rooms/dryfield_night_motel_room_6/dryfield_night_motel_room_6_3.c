#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "rooms/rooms_shared_80181228.h"

extern s16 D_80071076;

extern TaskDesc D_dryfield_night_motel_room_6_80182EE0;

extern GpAreaApplyRec D_dryfield_night_motel_room_6_80186270[];
extern GpAreaApplyRec D_dryfield_night_motel_room_6_801862B0[];

extern TaskDesc               D_dryfield_night_motel_room_6_80182E8C;
extern RoomsShared80181228Rec D_dryfield_night_motel_room_6_801862B8;

s32 func_dryfield_night_motel_room_6_80181A9C(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_dryfield_night_motel_room_6_8018175C(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 count;

    count = 0;
    if (arg2 == 0x16) {
        D_dryfield_night_motel_room_6_801862B8.field_0 = 0xC;
        D_dryfield_night_motel_room_6_801862B8.field_1 = 1;
        switch (GameFlag_GetNibble(0x7A)) {
            case 0 ... 3:
                if (gGameSession->at4.loc.stage == 2) {
                    count                                           = 4;
                    D_dryfield_night_motel_room_6_801862B8.field_14 = 0x3C0;
                    D_dryfield_night_motel_room_6_801862B8.field_3  = 1;
                } else {
                    count                                           = 2;
                    D_dryfield_night_motel_room_6_801862B8.field_14 = 0x380;
                    D_dryfield_night_motel_room_6_801862B8.field_3  = 1;
                }
                break;
            case 4 ... 6:
                count                                           = 2;
                D_dryfield_night_motel_room_6_801862B8.field_14 = 0x3C0;
                D_dryfield_night_motel_room_6_801862B8.field_3  = count;
                break;
        }
        D_dryfield_night_motel_room_6_801862B8.field_2  = 0;
        D_dryfield_night_motel_room_6_801862B8.field_4  = Gp_PackStageSndId(0x521E0008);
        D_dryfield_night_motel_room_6_801862B8.field_8  = Gp_PackStageSndId(0x521E000B);
        D_dryfield_night_motel_room_6_801862B8.field_10 = Gp_PackStageSndId(0x521E0009);
        D_dryfield_night_motel_room_6_801862B8.field_C  = Gp_PackStageSndId(0x521E000A);
        Task_SpawnFromTable(&D_dryfield_night_motel_room_6_80182E8C, 0, count, (s32)&D_dryfield_night_motel_room_6_801862B8);
    } else {
        func_dryfield_night_motel_room_6_80181A9C(arg0, arg1, arg2, arg3);
    }
    return 0;
}

void func_dryfield_night_motel_room_6_8018189C(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto advance;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    Gp_MsgPlayerWeapon(0);
    Gp_RunCapCmd1(0x10);
    goto advance;

L_case2:
    if (Gp_GetCapEventKey() == 0xB) {
        taskKill(task);
        Gp_MsgPlayerWeapon(1);
    }
    goto advance;

L_case3:
    GameFlag_SetNibble(0x70, 2);
    goto advance;

L_case4:
    if (Gp_CapBusy() != 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case5:
    Gp_FillPlayerHpMp();
    SndEvt_EnqueueType7(0x80000000, 0);
    Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_80186270);
    if (GameFlag_GetNibble(0xCE) != 0) {
        Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_801862B0);
    }
    GameFlag_SetNibble(0x59, 1);
    GameFlag_SetNibble(0x5A, 2);
    GameFlag_SetNibble(0x30, 0);
    Mc_SaveData.at4.loc.area = 8;
    Mc_SaveData.at4.loc.warp = 1;
    Mc_SaveData.at4.loc.room = 1;
    D_80071076               = 1;
    Task_Spawn(0, 0x11, 0, 0);
    taskKill(task);
}
