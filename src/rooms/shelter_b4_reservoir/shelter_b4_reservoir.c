#include "common.h"
#include "main/task.h"
extern s16 D_shelter_b4_reservoir_80184F82;

extern s32 D_shelter_b4_reservoir_8018492C;
extern s16 D_shelter_b4_reservoir_80184F80;

extern u8    D_shelter_b4_reservoir_80184F78;
extern u8    D_shelter_b4_reservoir_80184F79;
extern u8    D_shelter_b4_reservoir_80184F7A;
extern s16   D_shelter_b4_reservoir_80184F7C;
extern s32   D_shelter_b4_reservoir_80187510;
extern Task* D_shelter_b4_reservoir_80184930;

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017D650);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", D_shelter_b4_reservoir_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", D_shelter_b4_reservoir_8017D5C4);

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

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E354);

s32 func_shelter_b4_reservoir_8017E3C4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E3CC);

void func_shelter_b4_reservoir_8017E400(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32)(arg0->killCountdown * 0x5DC) / (s32)arg0->spawnArg1;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F80 = temp_v0 - 0x7D0;
    if (arg0->spawnArg1 < temp_v1) {
        Task_Kill(arg0);
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
        Task_Kill(arg0);
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

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", func_shelter_b4_reservoir_8017E780);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", jtbl_shelter_b4_reservoir_8017D604);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", jtbl_shelter_b4_reservoir_8017D624);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_reservoir/shelter_b4_reservoir", jtbl_shelter_b4_reservoir_8017D63C);
