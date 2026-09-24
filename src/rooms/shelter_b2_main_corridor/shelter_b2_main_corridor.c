#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/shelter_b2_main_corridor.h"

/// `Gp_StateF0.field_4` (0x801153F4). Declared as a one-element array so the
/// store keeps the in-struct memory attribute a struct-member store has, which
/// makes it alias the task's argument load and keeps the two in source order.
extern u8 D_801153F4[1];
extern u8 D_80115690;

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D6BC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D82C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017D9C4);

s32 func_shelter_b2_main_corridor_8017DC88(Task* arg0, s32 arg1, GpMsg13EF* arg2, s32 arg3)
{
    s32 id;

    if (arg2->field_2 == 0xA) {
        if (arg2->field_3 == 7) {
            if (GameFlag_GetNibble(0xAE) != 0) {
                if (GameFlag_GetNibble(0xDA) != 0) {
                    D_shelter_b2_main_corridor_80189684.field_0 = 5;
                    D_shelter_b2_main_corridor_80189684.field_1 = arg2->field_3;
                } else {
                    D_shelter_b2_main_corridor_80189684.field_0 = 4;
                    D_shelter_b2_main_corridor_80189684.field_1 = 0x31;
                }
                D_shelter_b2_main_corridor_80189684.field_3 = 1;
                D_shelter_b2_main_corridor_80189684.field_2 = 1;
                D_shelter_b2_main_corridor_80189684.field_8 = 0;
                D_shelter_b2_main_corridor_80189684.field_4 = -1;
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 0, 6, 0);
            } else {
                Gp_RunCapCmd1(3);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 1, 0x1C4, 0);
            }
        }
        if (arg2->field_3 == 8) {
            if (GameFlag_GetNibble(0xD1) == 2) {
                Gp_RunCapCmd1(4);
                return 0;
            }
            if (GameFlag_GetNibble(0xF8) != 0) {
                Gp_RunCapCmd1(8);
                Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 1, 0x1AF, 0);
                return 0;
            }
            if (GameFlag_GetNibble(0xDF) != 0) {
                id = 0xD;
            } else {
                id = 7;
            }
            D_shelter_b2_main_corridor_80189684.field_0 = 5;
            D_shelter_b2_main_corridor_80189684.field_1 = arg2->field_3;
            D_shelter_b2_main_corridor_80189684.field_3 = 1;
            D_shelter_b2_main_corridor_80189684.field_2 = 1;
            D_shelter_b2_main_corridor_80189684.field_8 = 0;
            D_shelter_b2_main_corridor_80189684.field_4 = -1;
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b2_main_corridor_80182C44, 0, id, 0);
        }
    }
    if (arg2->field_2 == 1 && GameFlag_GetNibble(0x82) >= 2 && GameFlag_GetNibble(0xD3) == 0) {
        GameFlag_SetNibble(0xD3, 1);
        GameFlag_SetNibble(0xAE, 1);
        GameFlag_SetNibble(0x1C4, 0);
        func_800E8614((s32)&D_shelter_b2_main_corridor_80182CA8, 0);
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", RoomsShared8017d878Table);

void func_shelter_b2_main_corridor_8017DEB0(Task* arg0)
{
    RoomEventMsg param;
    s32          (*resolve)(RoomEventMsg*, RoomEventMsg*);

    switch (arg0->state) {
        case 0:
            D_801153F4[0] = 1;
            Gp_RunCapCmd1(arg0->spawnArg1);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            arg0->state++;
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xC) {
                D_801153F4[0] = 0;
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                break;
            }
            arg0->state++;
            break;
        case 3:
            arg0->state++;
            break;
        case 4:
            if (D_shelter_b2_main_corridor_80189684.field_1 == 7 && GameFlag_GetNibble(0xD1) == 2) {
                GameFlag_SetNibble(0x4C, 9);
            }
            if (D_shelter_b2_main_corridor_80189684.field_1 == 0x31) {
                if (GameFlag_GetNibble(0xD1) == 2) {
                    GameFlag_SetNibble(0x4C, 9);
                }
                if (GameFlag_GetNibble(0xDA) == 0) {
                    GameFlag_SetNibble(0xDA, 1);
                    GameFlag_SetNibble(0x7A, 5);
                }
            }
            if (D_shelter_b2_main_corridor_80189684.field_1 == 8) {
                if (GameFlag_GetNibble(0xDF) == 1) {
                    GameFlag_SetNibble(0xF8, 1);
                }
            }
            resolve = func_shelter_b2_main_corridor_8017E0FC;
            Gp_MsgPlayerWeapon(0);
            param.msgId   = D_shelter_b2_main_corridor_80189684.field_1;
            param.field_2 = D_shelter_b2_main_corridor_80189684.field_2;
            param.field_3 = D_shelter_b2_main_corridor_80189684.field_3;
            param.field_5 = 0;
            resolve(&param, &param);
            D_shelter_b2_main_corridor_80189684.field_1 = param.msgId;
            D_shelter_b2_main_corridor_80189684.field_2 = param.field_2;
            D_shelter_b2_main_corridor_80189684.field_3 = param.field_3;
            D_shelter_b2_main_corridor_80189664         = D_shelter_b2_main_corridor_80189684;
            Task_SpawnFromTable(&D_shelter_b2_main_corridor_801828E4, 0, 0, 0);
            taskKill(arg0);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_main_corridor/shelter_b2_main_corridor", func_shelter_b2_main_corridor_8017E0FC);

s32 func_shelter_b2_main_corridor_8017E1CC(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1D4(void)
{
    return 0;
}

s32 func_shelter_b2_main_corridor_8017E1DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 9) {
        SndEvt_EnqueueType6(0x54210000 | 9, 0, 0);
    }
    return 0;
}
