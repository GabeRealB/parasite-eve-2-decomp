#include "common.h"

#include "gameplay/268.h"
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
extern s32 D_8007216C;

/// `Mc_SaveData.field_13` (ally present). A distinct symbol so the restore
/// path does not share the `Mc_SaveData` address with case 0.
extern s8 D_8007217B;

/// Cutscene script blob argument of `func_800E8614` / `func_800E8634`.
extern void func_800E8614(s32 arg0, s32 arg1);
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

/// Byte at 0x8007272D, written when the trailer-coach scene ends.
extern s8 D_8007272D;

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
} DryfieldTrailerCoachCapScript;

STATIC_ASSERT_SIZEOF(DryfieldTrailerCoachCapScript, 0x18);

extern TaskDesc       D_dryfield_trailer_coach_80184F7C;
extern s32            D_dryfield_trailer_coach_80185AFC;
extern s32            D_dryfield_trailer_coach_80185C4C;
extern s32            D_dryfield_trailer_coach_80185D54;
extern s32            D_dryfield_trailer_coach_80186684;
extern s32            D_dryfield_trailer_coach_8018681C;
extern s32            D_dryfield_trailer_coach_80186A74;
extern s32            D_dryfield_trailer_coach_80186BDC;
extern s32            D_dryfield_trailer_coach_80186D2C;
extern s32            D_dryfield_trailer_coach_80187074;
extern GpAreaApplyRec D_dryfield_trailer_coach_80188888[];
extern GpAreaApplyRec D_dryfield_trailer_coach_80189C50;
extern Task*          D_dryfield_trailer_coach_80189C94;

void func_dryfield_trailer_coach_801822F4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            func_800E8614((s32)&D_dryfield_trailer_coach_80185AFC, 1);
            task->state++;
            break;
        case 1:
            if (Game_Session->field_1 == 0) {
                task->state = 2;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xB) {
                func_800E8614((s32)&D_dryfield_trailer_coach_80185C4C, 0);
                task->state++;
                break;
            }
            if (GameFlag_GetNibble(0x28) < 2) {
                func_800E8634((s32)&D_dryfield_trailer_coach_80185D54, 0,
                              (s32)&D_dryfield_trailer_coach_80186684);
                GameFlag_SetNibble(0x28, 2);
                GameFlag_SetNibble(0x3A, 1);
                GameFlag_SetNibble(0x4B, 1);
                func_800E3FAC(0xA2, 0xF);
                D_8007272D = 6;
                SOFT_BARRIER();
                Gp_ApplyAreaRecs(&D_dryfield_trailer_coach_80189C50);
                task->state++;
                break;
            }
            if (Gp_HasCollectedBit(0x111) == 0 && GameFlag_GetNibble(0x4F) != 0) {
                if (GameFlag_GetNibble(0xFD) == 0) {
                    GameFlag_SetNibble(0xFD, 1);
                    func_800E8614((s32)&D_dryfield_trailer_coach_80186D2C, 0);
                } else {
                    func_800E8614((s32)&D_dryfield_trailer_coach_80187074, 0);
                }
                task->state++;
                break;
            }
            if (GameFlag_GetNibble(0x28) == 2) {
                func_800E8634((s32)&D_dryfield_trailer_coach_8018681C, 0,
                              (s32)&D_dryfield_trailer_coach_80186A74);
                GameFlag_SetNibble(0x28, 3);
            } else {
                func_800E8614((s32)&D_dryfield_trailer_coach_80186BDC, 0);
            }
            task->state++;
            break;
        case 3:
            Task_Kill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_2", D_dryfield_trailer_coach_8017D7DC);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_2", D_dryfield_trailer_coach_8017D7E8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_2", RoomsShared8017eb5cIdList);
