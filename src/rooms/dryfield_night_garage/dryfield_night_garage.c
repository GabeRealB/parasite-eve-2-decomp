#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Cutscene script blob argument of `func_800E8634`.
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

/// Byte at 0x8007272D, written when the garage scene ends.
extern s8 D_8007272D;

/// One entry of the room's 0x98-byte display-object table. Only the flag byte
/// at 0x4A is touched here: bit 6 shows the entry, clearing it hides it.
typedef struct {
    /* 0x00 */ u8 pad_0[0x4A];
    /* 0x4A */ u8 field_4A;
    /* 0x4B */ u8 pad_4B[0x4D];
} DryfieldNightGarageObj;
STATIC_ASSERT_SIZEOF(DryfieldNightGarageObj, 0x98);

extern TaskDesc               D_8013B11C[];
extern s32                    D_dryfield_night_garage_80182DE0;
extern s32                    D_dryfield_night_garage_80182DE4;
extern TaskDesc               D_dryfield_night_garage_80182C98[];
extern s32                    D_dryfield_night_garage_80182DF8;
extern s32                    D_dryfield_night_garage_801831B8;
extern DryfieldNightGarageObj D_dryfield_night_garage_80186E60[];

s32 func_800D4D2C(s32 arg0);
s32 func_dryfield_night_garage_80180A64(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017DDC4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E250);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E39C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", D_dryfield_night_garage_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E768);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017E9B8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", D_dryfield_night_garage_8017D6D8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017EBD8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017ED80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017EF64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F178);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F2F8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017F794);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FC14);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FCD0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FDF8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017FF2C);

s32 func_dryfield_night_garage_801800C8(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    DryfieldNightGarageObj* base;
    DryfieldNightGarageObj* obj;

    if (msg->field_2 == 6) {
        if (Game_Session->field_9 == 2) {
            if (GameFlag_GetNibble(0x6C) == 0) {
                if (Gp_HasCollectedBit(0x113) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 6, 0);
                } else if (Gp_HasCollectedBit(0x117) == 0 && Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 7, 0);
                } else if (Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 8, 0);
                } else if (GameFlag_GetNibble(0x6C) == 0) {
                    base            = D_dryfield_night_garage_80186E60;
                    obj             = base + 1;
                    base->field_4A |= 0x40;
                    obj->field_4A  &= 0xBF;
                    func_800E8634((s32)&D_dryfield_night_garage_80182DF8, 0,
                                  (s32)&D_dryfield_night_garage_801831B8);
                    GameFlag_SetNibble(0x6C, 1);
                    func_800E3FAC(0xA2, 0x17);
                    Gp_ClearCollectedBit(0x118);
                    D_8007272D = 5;
                }
            } else {
                Gp_MsgPlayerWeapon(0);
                if (GameFlag_GetNibble(0x6C) == 1) {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0xA, 0);
                } else {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0x15, 0);
                }
            }
        }
    }
    if (msg->field_2 == 1) {
        if (GameFlag_GetNibble(0x97) != 0) {
            Gp_StartCapSlot(0x14, 1, 0);
        } else {
            Gp_SpawnIfCapIdle(0x36, 0);
        }
    }
    if (msg->field_2 == 2 && Game_Session->field_9 == 3 && Game_GetPtrSlot(0xA) != NULL) {
        Task_SpawnFromTable(D_8013B11C, 1, 0, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180300);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180358);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180360);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801803A4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801803AC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801803BC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180414);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180604);

void func_dryfield_night_garage_801807E4(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_StartCapSlot((s16)arg0->spawnArg1, 0, 0);
            Gp_DispatchMsg(
                (Task*)func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE0, 0);
            goto block_12;
        case 1:
            if (Gp_CapBusy() == 0) {
                func_800D4D2C(0x20);
                goto block_12;
            }
            return;
        case 2:
            Gp_StartCapSlot((s16)arg0->spawnArg1, 0, (s16)(GameFlag_GetNibble(0x107) + 1));
            if (GameFlag_GetNibble(0x107) == 0) {
                GameFlag_SetNibble(0x107, 1);
            }
        block_12:
            arg0->state = arg0->state + 1;
            return;
        case 3:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_MsgPlayerWeapon(1);
            Gp_DispatchMsg(
                (Task*)func_dryfield_night_garage_80180A64(0), 0x7DB, (s32)&D_dryfield_night_garage_80182DE4, 0);
        default:
            Task_Kill(arg0);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180924);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180944);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180964);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180984);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_801809A4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180A64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180AB0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180B20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180CEC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180D4C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80180D9C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_80181518);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_garage/dryfield_night_garage", func_dryfield_night_garage_8017D754);
