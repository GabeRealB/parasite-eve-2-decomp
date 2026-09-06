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

void Room_Draw01(SVECTOR* line, s32 arg1, s32 arg2);

void func_shelter_b2_laboratory_801812F8(SVECTOR* pos, s32 arg1, s32 arg2);

void Room_Draw05(SVECTOR* pos, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_laboratory/shelter_b2_laboratory_4", func_shelter_b2_laboratory_801804FC);

void func_shelter_b2_laboratory_80180548(Task* task)
{
    if (task->state == 0) {
        D_shelter_b2_laboratory_80186540 = 0;
        task->state                      = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            break;
        case 3:
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x444);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[2], 0x180, 0x444);
            break;
        case 4:
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[0], 0x200, 0x444);
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[4], 0x200, 0x241);
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[12], 0x200, 0x124);
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[14], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[16], 0x200, 0x124);
            break;
        case 5:
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[12], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[14], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            Room_Draw01(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
        case 6:
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[0], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[2], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[4], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[6], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[8], 0x180, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[20], 0x200, 0x124);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[22], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[24], 0x200, 0x124);
            break;
        case 7:
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[0], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[12], 0x200, 0x124);
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[14], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B60[16], 0x200, 0x124);
            break;
        case 8:
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[0], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[2], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[20], 0x200, 0x124);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[22], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B20[24], 0x200, 0x124);
            break;
        case 9:
            Room_Draw01(D_shelter_b2_laboratory_80182BF0, 0x180, 0x124);
            break;
        case 10:
            Room_Draw01(&D_shelter_b2_laboratory_80182AB0[0], 0x180, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182AB0[2], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AB0[4], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182AB0[26], 0x200, 0x241);
            Room_Draw01(&D_shelter_b2_laboratory_80182AB0[40], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AB0[42], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AB0[42], 0x60, 0x80);
            }
            break;
        case 12:
            Room_Draw01(&D_shelter_b2_laboratory_80182B00[0], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B00[2], 0x180, 0x333);
            Room_Draw01(&D_shelter_b2_laboratory_80182B00[18], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B00[20], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B00[22], 0x200, 0x124);
            Room_Draw01(&D_shelter_b2_laboratory_80182B00[30], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B00[32], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B00[32], 0x60, 0x80);
            }
            break;
        case 13:
            if (D_shelter_b2_laboratory_80186540 != 0) {
                Room_Draw05(D_shelter_b2_laboratory_80182C00, 0x180, 0x80);
            } else {
                Room_Draw05(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
        case 15:
            Room_Draw01(&D_shelter_b2_laboratory_80182B70[0], 0x200, 0x241);
            Room_Draw01(&D_shelter_b2_laboratory_80182B70[4], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B70[6], 0x200, 0x222);
            Room_Draw01(&D_shelter_b2_laboratory_80182B70[8], 0x200, 0x124);
            Room_Draw01(&D_shelter_b2_laboratory_80182B70[16], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182B70[18], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(D_shelter_b2_laboratory_80182C00, 0x60, 0x80);
            }
            break;
    }
}
