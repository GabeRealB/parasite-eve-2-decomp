#include "common.h"

#include "gameplay/3CD8.h"

#include "main/session.h"

s32 func_8017A038(void);

extern u8 D_80071075;
extern s8 D_80114C12;

extern s32 D_mist_r18_8018576C;
extern s32 D_mist_r18_80185AE4;
extern s32 D_mist_r18_80185EBC;
extern s32 D_mist_r18_8018603C;
extern s32 D_mist_r18_801861BC;
extern s32 D_mist_r18_8018645C;
extern s32 D_mist_r18_8018651C;
extern s32 D_mist_r18_80186564;
extern s32 D_mist_r18_80186E9C;
extern s32 D_mist_r18_80186EA0;

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017D5F4);

void func_mist_r18_8017D960(void)
{
    s32 state;

    if ((Game_Session->field_1 == 0) && (D_80114C12 != 1) && (D_80071075 == 0)) {
        state = D_mist_r18_80186E9C;
        if (state == 1) {
            func_800E8634((s32)&D_mist_r18_80185EBC, 0, (s32)&D_mist_r18_80186564);
            D_mist_r18_80186E9C = 2;
        } else if (state == 2) {
            func_800E8634((s32)&D_mist_r18_8018576C, 0, (s32)&D_mist_r18_8018645C);
            D_mist_r18_80186EA0 = 0;
            D_mist_r18_80186E9C = 3;
        } else if (state == 3) {
            func_800E8614((s32)&D_mist_r18_8018603C, 0);
            D_mist_r18_80186E9C = 4;
        } else if (state == 4) {
            if (func_8017A038() != 1) {
                func_800E8614((s32)&D_mist_r18_801861BC, 0);
                D_mist_r18_80186EA0 = 1;
                return;
            }
            func_800E8634((s32)&D_mist_r18_80185AE4, 0, (s32)&D_mist_r18_8018651C);
            D_mist_r18_80186E9C = 5;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017DA8C);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017DBB8);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017DD7C);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017DF80);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017E144);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18", func_mist_r18_8017E2C8);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/mist_r18/mist_r18", D_mist_r18_8017D5DC);
