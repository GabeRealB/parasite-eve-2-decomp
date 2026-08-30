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
} ShelterLaboratoryCapParams;

extern TaskDesc       D_shelter_b2_laboratory_80182A08[];
extern GpAreaApplyRec D_80188888[];
extern Task*          D_shelter_b2_laboratory_801864A4;

/// Endpoint pool for the room's view-specific debug/ambient line overlays.
/// `func_..._80180AB4` draws the segment `pt[n]`..`pt[n + 1]`;
/// `func_..._801812F8` and `func_..._8018176C` take a single point.
extern SVECTOR D_shelter_b2_laboratory_80182AA0[];
extern SVECTOR D_shelter_b2_laboratory_80182AB0[];
extern SVECTOR D_shelter_b2_laboratory_80182B00[];
extern SVECTOR D_shelter_b2_laboratory_80182B20[];
extern SVECTOR D_shelter_b2_laboratory_80182B60[];
extern SVECTOR D_shelter_b2_laboratory_80182B70[];
extern SVECTOR D_shelter_b2_laboratory_80182BF0[];
extern SVECTOR D_shelter_b2_laboratory_80182C00[];

/// Set to 0 when the overlay task starts; non-zero selects the brighter pass.
extern u16 D_shelter_b2_laboratory_80186540;

void func_shelter_b2_laboratory_80180AB4(SVECTOR* line, s32 arg1, s32 arg2);
void func_shelter_b2_laboratory_801812F8(SVECTOR* pos, s32 arg1, s32 arg2);
void func_shelter_b2_laboratory_8018176C(SVECTOR* pos, s32 arg1, s32 arg2);

void func_shelter_b2_laboratory_8017F4D8(Task* task)
{
    s32                         out;
    s32                         var_a0;
    s32                         var_a1;
    s32                         temp;
    ShelterLaboratoryCapParams* p;

    p = task->spawnArg2;
    switch (task->state) {
        case 0:
            D_shelter_b2_laboratory_801864A4 = NULL;
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
            D_shelter_b2_laboratory_801864A4 = Task_SpawnFromTable(D_shelter_b2_laboratory_80182A08, 1, 0, p->field_10);
            Gp_StartCapSlot(p->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(p->field_10, 1);
                Task_Kill(D_shelter_b2_laboratory_801864A4);
                task->state++;
            } else if (Task_PollKill(D_shelter_b2_laboratory_801864A4, &out) != 0) {
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
                    Gp_ApplyAreaRecs(D_80188888);
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

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_8017FA44);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_8017FBA8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_8017FD18);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", D_shelter_b2_laboratory_8017D6BC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_8017FEB8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_80180064);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_801800F4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_801800FC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_801801D0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_8018025C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_80180290);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_80180350);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_80180450);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_80180494);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_801804A4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_801804FC);

void func_shelter_b2_laboratory_80180548(Task* task)
{
    if (task->state == 0) {
        D_shelter_b2_laboratory_80186540 = 0;
        task->state                      = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            break;
        case 3:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x444);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[2], 0x180, 0x444);
            break;
        case 4:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[0], 0x200, 0x444);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[4], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[12], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[14], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[16], 0x200, 0x124);
            break;
        case 5:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[12], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[14], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
        case 6:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[6], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[8], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[20], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[22], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[24], 0x200, 0x124);
            break;
        case 7:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[12], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[14], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B60[16], 0x200, 0x124);
            break;
        case 8:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[20], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[22], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B20[24], 0x200, 0x124);
            break;
        case 9:
            func_shelter_b2_laboratory_80180AB4(D_shelter_b2_laboratory_80182BF0, 0x180, 0x124);
            break;
        case 10:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[0], 0x180, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[4], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[26], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182AB0[40], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AB0[42], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AB0[42], 0x60, 0x80);
            }
            break;
        case 12:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[0], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[2], 0x180, 0x333);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[18], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[20], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[22], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B00[30], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B00[32], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B00[32], 0x60, 0x80);
            }
            break;
        case 13:
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_8018176C(D_shelter_b2_laboratory_80182C00, 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_8018176C(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
        case 15:
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[0], 0x200, 0x241);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[4], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[6], 0x200, 0x222);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[8], 0x200, 0x124);
            func_shelter_b2_laboratory_80180AB4(&D_shelter_b2_laboratory_80182B70[16], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B70[18], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_80180AB4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_801812F8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_8018176C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_2", func_shelter_b2_laboratory_801820F4);
