#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s32      D_dryfield_night_motel_lobby_801844D4;
extern TaskDesc D_dryfield_night_motel_lobby_801827FC;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017ef20Title);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017de9cHundred);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017e8b4WeaponTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017e8b4PeTitle);
INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", RoomsShared8017ea68Title);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", func_dryfield_night_motel_lobby_8017FB7C);

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", func_dryfield_night_motel_lobby_8017FCDC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", func_dryfield_night_motel_lobby_8017FD10);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_2", func_dryfield_night_motel_lobby_8017FD9C);

void func_dryfield_night_motel_lobby_8017FDE8(void)
{
    s32 temp_v0;

    temp_v0 = Gp_HasCollectedBit(0x113);
    if ((temp_v0 != 0) && (D_dryfield_night_motel_lobby_801844D4 == 0)) {
        func_800E3FAC(0xA2, 0x14);
    }
    D_dryfield_night_motel_lobby_801844D4 = temp_v0;
}
