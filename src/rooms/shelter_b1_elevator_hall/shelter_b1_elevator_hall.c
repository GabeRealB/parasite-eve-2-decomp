#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_b1_elevator_hall_80182CB8[];
extern TaskDesc   D_shelter_b1_elevator_hall_80182CAC;
extern TaskDesc   D_shelter_b1_elevator_hall_80182CE8;
extern GpSaveLoc  D_shelter_b1_elevator_hall_801849F8;

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", func_shelter_b1_elevator_hall_8017D620);

s32 func_shelter_b1_elevator_hall_8017D810(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0xF && GameFlag_GetNibble(0xA5) == 0) {
        if (src->field_5 == 0) {
            Gp_SetNibbleIf(src->field_6, 2);
            Gp_RunCapCmd1(2);
        }
        return 0;
    }
    if (*(u16*)src == 0x1A) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            if (src->field_5 == 0) {
                Gp_SetNibbleIf(src->field_6, 2);
                Gp_RunCapCmd1(1);
            }
        } else {
            if (src->field_5 == 0) {
                Gp_RunCapCmd(4, 0);
                Task_SpawnFromTable(&D_shelter_b1_elevator_hall_80182CAC, 0, 0x54090008, 0);
            }
        }
        return 0;
    }
    if (*(u16*)src == 8) {
        if (src->field_5 == 0) {
            D_shelter_b1_elevator_hall_801849F8.field_2 = dst->field_0;
            D_shelter_b1_elevator_hall_801849F8.field_4 = dst->field_2;
            D_shelter_b1_elevator_hall_801849F8.field_1 = dst->field_3;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_elevator_hall_80182CE8, 0, 0, 0);
        }
        return 2;
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_elevator_hall/shelter_b1_elevator_hall", func_shelter_b1_elevator_hall_8017D99C);

s32 func_shelter_b1_elevator_hall_8017DB54(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB5C(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB64(void)
{
    return 0;
}

s32 func_shelter_b1_elevator_hall_8017DB6C(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 6:
            SndEvt_EnqueueType6(0x16, 0, 0);
            break;
        case 8:
            SndEvt_EnqueueType6(0x54090008, 0, 0);
            break;
    }
    return 0;
}

void func_shelter_b1_elevator_hall_8017DBB8(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_elevator_hall_80182CB8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x122) == 0) {
        GameFlag_SetNibble(0x122, 1);
        func_800E3FAC(0xA2, 0x1D);
    }
    arg0->state++;
}

void func_shelter_b1_elevator_hall_8017DC20(void)
{
}
