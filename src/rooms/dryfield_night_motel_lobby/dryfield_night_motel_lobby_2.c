#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_night_motel_lobby.h"

extern TaskDesc D_dryfield_night_motel_lobby_801827A8;
extern s32      D_dryfield_night_motel_lobby_801827CC;
extern s32      D_dryfield_night_motel_lobby_801844D4;
extern TaskDesc D_dryfield_night_motel_lobby_801827FC;
extern TaskDesc D_dryfield_night_motel_lobby_801828D4;
extern Task*    D_dryfield_night_motel_lobby_801844CC;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017ea68Title);

s32 func_dryfield_night_motel_lobby_8017FB7C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        if (GameFlag_GetNibble(0x16D) == 0) {
            GameFlag_SetNibble(0x16D, 1);
            Gp_RunCapCmd1(0xA);
            return 0;
        }
        D_dryfield_night_motel_lobby_801844E0.field_0 = 5;
        D_dryfield_night_motel_lobby_801844E0.field_1 = 1;
        if (GameFlag_GetNibble(0x7A) < 4) {
            D_dryfield_night_motel_lobby_801844E0.field_14 = 0x380;
            D_dryfield_night_motel_lobby_801844E0.field_3  = 1;
        } else {
            D_dryfield_night_motel_lobby_801844E0.field_14 = 0x3C0;
            D_dryfield_night_motel_lobby_801844E0.field_3  = 2;
        }
        D_dryfield_night_motel_lobby_801844E0.field_2  = 0;
        D_dryfield_night_motel_lobby_801844E0.field_4  = 0x53110003;
        D_dryfield_night_motel_lobby_801844E0.field_8  = 0x53110004;
        D_dryfield_night_motel_lobby_801844E0.field_10 = 0x53110005;
        D_dryfield_night_motel_lobby_801844E0.field_C  = 0x53110006;
        Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801827A8, 0, 4, (s32)&D_dryfield_night_motel_lobby_801844E0);
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FC6C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x74) == 0) {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801827FC, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(8);
        }
    }
    return 0;
}

s32 func_dryfield_night_motel_lobby_8017FCDC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x63) {
        Gp_EnqueueStageSnd6(0x5311000A, 0, 0);
    }
    return 0;
}

void func_dryfield_night_motel_lobby_8017FD10(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            D_dryfield_night_motel_lobby_801844CC = Task_SpawnFromTable(&D_dryfield_night_motel_lobby_801828D4, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_dryfield_night_motel_lobby_801844CC, &poll) != 0) {
                Task_Kill(task);
            }
            return;
    }
}

void func_dryfield_night_motel_lobby_8017FD9C(Task* task)
{
    task->msgTable = &D_dryfield_night_motel_lobby_801827CC;
    Game_SetPtrSlot(task, 7);
    D_dryfield_night_motel_lobby_801844D4 = 1;
    task->state                           = (s32)(task->state + 1);
}

void func_dryfield_night_motel_lobby_8017FDE8(void)
{
    s32 temp_v0;

    temp_v0 = Gp_HasCollectedBit(0x113);
    if ((temp_v0 != 0) && (D_dryfield_night_motel_lobby_801844D4 == 0)) {
        func_800E3FAC(0xA2, 0x14);
    }
    D_dryfield_night_motel_lobby_801844D4 = temp_v0;
}
