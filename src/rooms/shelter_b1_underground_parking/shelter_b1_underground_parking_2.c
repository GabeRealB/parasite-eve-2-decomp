#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern s16 D_80114D08;
extern s8  D_801153F4;
/// Saved `Mc_SaveData.field_4` (area id), restored when the cutscene ends.
extern s32 D_80115694;
/// `Mc_SaveData.field_4`, i.e. the four-byte `GpAreaKey` prefix, read as one
/// word: bits 16..31 are `field_6` / `field_7`.
extern u32 D_8007216C;

/// `Task::spawnArg2` of the cap (cutscene) task this room family spawns.
/// `field_0` is the area id forced for the duration of the scene (negative =
/// keep the current one); `field_1` selects the cap slot / command;
/// `field_2` skips straight to the abort state; `field_3` is the cap file to
/// load. The four s32s are sound-event ids, and `field_14` / `field_16` are
/// the `func_800E6D4C` fade pair.
typedef struct {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} ShelterParkingCapParams;

extern TaskDesc       D_shelter_b1_underground_parking_8018720C[];
extern GpAreaApplyRec D_shelter_b1_underground_parking_80188888[];
extern Task*          D_shelter_b1_underground_parking_8018D754;

void func_shelter_b1_underground_parking_80182154(Task* task)
{
    s32                      out;
    s32                      var_a0;
    s32                      var_a1;
    s32                      temp;
    s32                      cmd;
    ShelterParkingCapParams* p;

    p = task->spawnArg2;
    switch (task->state) {
        case 0:
            D_shelter_b1_underground_parking_8018D754 = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData.field_13 == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (p->field_0 > 0) {
                D_80115694          = Mc_SaveData.field_4;
                Mc_SaveData.field_4 = p->field_0;
            } else {
                D_80115694 = -p->field_0;
            }
            Game_Session->field_68 = 1;
            Game_Session->field_1  = 1;
            D_801153F4             = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (p->field_4 != 0) {
                SndEvt_EnqueueType6(p->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (p->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(p->field_3);
                var_a0 = p->field_14;
                var_a1 = 0;
                if (var_a0 == 0) {
                    var_a0 = 0x3C0;
                } else {
                    var_a1 = p->field_16;
                }
                func_800E6D4C(var_a0, var_a1);
            }
            if (p->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_shelter_b1_underground_parking_8018D754 = Task_SpawnFromTable(D_shelter_b1_underground_parking_8018720C, 1, 0, p->field_10);
            Gp_StartCapSlot(p->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(p->field_10, 1);
                Task_Kill(D_shelter_b1_underground_parking_8018D754);
                task->state++;
            } else if (Task_PollKill(D_shelter_b1_underground_parking_8018D754, &out) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (p->field_2 == 0) {
                SndEvt_EnqueueType6(p->field_C, 0, 0);
            }
            temp = GameFlag_GetNibble(0x7A);
            if (temp > 0) {
                if (temp >= 5) {
                    if (temp == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (p->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(p->field_1, 0);
            }
            if ((GameFlag_GetNibble(0x7A) == 1) && (GameFlag_GetNibble(0) == 2)) {
                GameFlag_SetNibble(0, 3);
                GameFlag_SetNibble(0xE, 4);
                if ((D_8007216C & 0xFFFF0000) == 0x01010000) {
                    Gp_ApplyAreaRecs(D_shelter_b1_underground_parking_80188888);
                    func_800E3FAC(0xA2, 5);
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData.field_4 = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(p->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData.field_13 == 1) {
                Gp_MsgAllyWeapon(1);
            }
            Game_Session->field_68 = 0;
            Game_Session->field_1  = 0;
            D_801153F4             = 0;
            if (p->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            Task_Kill(task);
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801826C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", D_shelter_b1_underground_parking_8017D7F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80182830);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80182A60);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80182DB4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80182FC8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183124);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801831F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183284);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183360);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801833DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183410);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801834D4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183560);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_8018363C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801836D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183714);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801837D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183804);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183810);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801838B4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_8018390C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183958);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183B9C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", D_shelter_b1_underground_parking_8017D9A4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80183CEC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_8018414C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184234);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184284);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184304);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801843F0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184468);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184594);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801845F8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801846EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184778);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801847D0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801848A4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801848BC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_8018491C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184964);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184A18);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80184C54);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_8018543C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_801857E0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80185A94);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80185F08);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking_2", func_shelter_b1_underground_parking_80186890);
