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
} MistParkingCapParams;

extern TaskDesc       D_mist_parking_801869B8[];
extern GpAreaApplyRec D_mist_parking_80188888[];
extern Task*          D_mist_parking_80195318;

void func_mist_parking_80181E8C(Task* task)
{
    s32                   out;
    s32                   var_a0;
    s32                   var_a1;
    s32                   temp;
    s32                   cmd;
    MistParkingCapParams* p;

    p = task->spawnArg2;
    switch (task->state) {
        case 0:
            D_mist_parking_80195318 = NULL;
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
            D_mist_parking_80195318 = Task_SpawnFromTable(D_mist_parking_801869B8, 1, 0, p->field_10);
            Gp_StartCapSlot(p->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(p->field_10, 1);
                Task_Kill(D_mist_parking_80195318);
                task->state++;
            } else if (Task_PollKill(D_mist_parking_80195318, &out) != 0) {
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
                    Gp_ApplyAreaRecs(D_mist_parking_80188888);
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

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801823F8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80182628);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801826B8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801826C0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801826E8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80182750);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801827A0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801827C0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80182888);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80182898);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801828F0);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", D_mist_parking_8017D7F4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80182A44);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80182F60);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018307C);

void func_mist_parking_801830F8(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183100);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018312C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018316C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801831F0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018326C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801832AC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183304);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801833F8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183434);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018345C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801834D4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018354C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018357C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183600);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183634);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183688);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801836CC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183708);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183780);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801837A4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801837B8);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018397C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801839CC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183A28);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183AC4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183B40);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183BAC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183D58);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80183EAC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801842DC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184408);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184428);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184468);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801844EC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018451C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018459C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801845D0);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184624);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184668);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_801846A4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8018471C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184728);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184A18);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80184E8C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_80185814);

INCLUDE_RODATA("rooms/nonmatchings/mist_parking/mist_parking_2", func_mist_parking_8017D8F8);
