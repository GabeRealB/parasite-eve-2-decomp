#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "rooms/room_common.h"
#include "rooms/shelter_1f_tent.h"

extern void func_801322B8(void);
extern void func_80132390(void);

extern TaskDesc D_shelter_1f_tent_80181CB8;

s32 func_shelter_1f_tent_8017FCA0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x169) == 0) {
            GameFlag_SetNibble(0x169, 1);
            Gp_RunCapCmd1(0x18);
            return 0;
        }
        D_shelter_1f_tent_801843C4.field_0  = 5;
        D_shelter_1f_tent_801843C4.field_1  = arg2;
        D_shelter_1f_tent_801843C4.field_3  = arg2;
        D_shelter_1f_tent_801843C4.field_2  = 0;
        D_shelter_1f_tent_801843C4.field_4  = 0x551C0003;
        D_shelter_1f_tent_801843C4.field_8  = 0x551C0006;
        D_shelter_1f_tent_801843C4.field_10 = 0x551C0004;
        D_shelter_1f_tent_801843C4.field_C  = 0x551C0005;
        Task_SpawnFromTable(&D_shelter_1f_tent_80181CB8, 0, 0xC, (s32)&D_shelter_1f_tent_801843C4);
    }
    return 0;
}

s32 func_shelter_1f_tent_8017FD54(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    if (arg2->field_2 == 1) {
        func_801322B8();
    }
    if (arg2->field_2 == 2) {
        func_80132390();
    }
    return 0;
}
