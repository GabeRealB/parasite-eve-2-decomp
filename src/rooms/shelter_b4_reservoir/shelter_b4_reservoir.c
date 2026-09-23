#include "common.h"
#include "gameplay/3CD8.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/shelter_b4_reservoir.h"
extern s16 D_shelter_b4_reservoir_80184F82;

extern s32 D_shelter_b4_reservoir_8018492C;
extern s16 D_shelter_b4_reservoir_80184F80;

extern u8       D_shelter_b4_reservoir_80184F78;
extern u8       D_shelter_b4_reservoir_80184F79;
extern u8       D_shelter_b4_reservoir_80184F7A;
extern s16      D_shelter_b4_reservoir_80184F7C;
extern s32      D_shelter_b4_reservoir_80187510;
extern Task*    D_shelter_b4_reservoir_80184930;
extern u8       D_8007216C;
extern u8       D_801153F4;
extern TaskDesc D_shelter_b4_reservoir_801848EC;
INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017DE8C);

void func_shelter_b4_reservoir_8017E068(void)
{
    D_shelter_b4_reservoir_80187510 = (D_shelter_b4_reservoir_80184F78 << 0x18) | (D_shelter_b4_reservoir_80184F7A << 0xC) | (D_shelter_b4_reservoir_80184F79 << 0x10) | D_shelter_b4_reservoir_80184F7C;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E0AC);

s32 func_shelter_b4_reservoir_8017E25C(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E264);

s32 func_shelter_b4_reservoir_8017E354(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        Gp_MsgPlayer3F3(0);
        Gp_MsgAlly3F3(0);
        Gp_MsgPlayerWeapon(0);
        Gp_MsgAllyWeapon(0);
        D_8007216C = 6;
        D_801153F4 = 2;
        Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3C4(void)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3CC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        SndEvt_EnqueueType6(0x542D0000 | 2, 0, 0);
    }
    return 0;
}

void func_shelter_b4_reservoir_8017E400(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32)(arg0->killCountdown * 0x5DC) / (s32)arg0->spawnArg1;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F80 = temp_v0 - 0x7D0;
    if (arg0->spawnArg1 < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

void func_shelter_b4_reservoir_8017E4B0(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32) - (arg0->killCountdown * 0x708) / (s32)arg0->spawnArg1;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F82 = (s16)temp_v0;
    if (arg0->spawnArg1 < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E558);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E610);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E690);

void func_shelter_b4_reservoir_8017E770(s32 arg0)
{
    D_shelter_b4_reservoir_80184930->state = arg0;
}

void func_shelter_b4_reservoir_8017E780(s32 arg0)
{
    func_shelter_b4_reservoir_80182B04(10, arg0, 0x140);
}
