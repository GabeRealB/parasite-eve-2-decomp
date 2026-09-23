#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/shelter_b4_reservoir.h"
extern s16 D_shelter_b4_reservoir_80184F82;

extern s32 D_shelter_b4_reservoir_8018492C;
extern s16 D_shelter_b4_reservoir_80184F80;

extern u8        D_shelter_b4_reservoir_80184F78;
extern u8        D_shelter_b4_reservoir_80184F79;
extern u8        D_shelter_b4_reservoir_80184F7A;
extern s16       D_shelter_b4_reservoir_80184F7C;
extern s32       D_shelter_b4_reservoir_80187510;
extern Task*     D_shelter_b4_reservoir_80184930;
extern u8        D_8007216C;
extern u8        D_801153F4;
extern TaskDesc  D_shelter_b4_reservoir_801848EC;
extern GpSaveLoc D_shelter_b4_reservoir_80187508;

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);
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

s32 func_shelter_b4_reservoir_8017E264(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0x2C) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            if (src->field_5 == 0) {
                Gp_SetNibbleIf(src->field_6, 2);
                Gp_RunCapCmd1(2);
            }
        } else {
            if (src->field_5 == 0) {
                D_shelter_b4_reservoir_80187508.field_2 = dst->field_0;
                D_shelter_b4_reservoir_80187508.field_4 = dst->field_2;
                D_shelter_b4_reservoir_80187508.field_1 = dst->field_3;
                Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 3, 0xB, 0);
            }
        }
        return 0;
    }
    return 1;
}

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

void func_shelter_b4_reservoir_8017E690(s32 arg0)
{
    switch (arg0) {
        case 0:
            D_shelter_b4_reservoir_8018492C = (s32)Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 1, 0x96, 0);
            break;
        case 1:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill((Task*)D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F80 = -0x1F4;
            break;
        case 2:
            D_shelter_b4_reservoir_8018492C = (s32)Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 2, 0x96, 0);
            break;
        case 3:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill((Task*)D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F82 = 0;
            break;
    }
}

void func_shelter_b4_reservoir_8017E770(s32 arg0)
{
    D_shelter_b4_reservoir_80184930->state = arg0;
}

void func_shelter_b4_reservoir_8017E780(s32 arg0)
{
    func_shelter_b4_reservoir_80182B04(10, arg0, 0x140);
}
