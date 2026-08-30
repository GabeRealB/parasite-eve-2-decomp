#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"

extern SVECTOR D_mist_shooting_gallery_80185550[];
extern SVECTOR D_mist_shooting_gallery_80185570[];
extern SVECTOR D_mist_shooting_gallery_801855C0[];
extern SVECTOR D_mist_shooting_gallery_801855F0[];
extern SVECTOR D_mist_shooting_gallery_80185610[];
extern SVECTOR D_mist_shooting_gallery_80185670[];
extern SVECTOR D_mist_shooting_gallery_80185678[];
extern SVECTOR D_mist_shooting_gallery_80185680[];
extern SVECTOR D_mist_shooting_gallery_80185688[];
extern SVECTOR D_mist_shooting_gallery_80185690[];
extern SVECTOR D_mist_shooting_gallery_801856B0[];

void Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw31(SVECTOR* v, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8017FEB8);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180000);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8018008C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8018018C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_801801E4);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180390);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8018055C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180728);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180A00);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180B34);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180B64);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180F2C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_801810D8);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_801811C0);

void func_mist_shooting_gallery_801811EC(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw01(&D_mist_shooting_gallery_80185550[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185550[8], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185550[10], 0x200, 0x222);
            break;
        case 3:
            Room_Draw01(&D_mist_shooting_gallery_80185570[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[10], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[12], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[14], 0x200, 0x222);
            break;
        case 7:
            Room_Draw01(&D_mist_shooting_gallery_801855C0[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[4], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[6], 0x200, 0x222);
            break;
        case 8:
            Room_Draw01(&D_mist_shooting_gallery_80185610[0], 0x200, 0x222);
            break;
        case 9:
        case 18:
            Room_Draw01(&D_mist_shooting_gallery_801855F0[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[6], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[8], 0x200, 0x222);
            Room_Draw31(&D_mist_shooting_gallery_801855F0[16], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
        case 10:
            Room_Draw31(&D_mist_shooting_gallery_80185678[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[2], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[4], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[6], 0x300, 0x111);
            break;
        case 11:
            Room_Draw31(&D_mist_shooting_gallery_80185680[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[1], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[3], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[4], 0x300, 0x111);
            break;
        case 12:
            Room_Draw31(&D_mist_shooting_gallery_80185690[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185690[1], 0x300, 0x111);
            break;
        case 13:
            Room_Draw31(&D_mist_shooting_gallery_80185688[0], 0x300, 0x111);
            break;
        case 14:
            Room_Draw31(&D_mist_shooting_gallery_80185670[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[1], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[3], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[5], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[7], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", D_mist_shooting_gallery_8017DB80);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", D_mist_shooting_gallery_8017DB8C);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", jtbl_mist_shooting_gallery_8017DBA0);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", jtbl_mist_shooting_gallery_8017DBE8);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", jtbl_mist_shooting_gallery_8017DC00);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", jtbl_mist_shooting_gallery_8017DC30);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", jtbl_mist_shooting_gallery_8017DC58);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", jtbl_mist_shooting_gallery_8017DC80);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", jtbl_mist_shooting_gallery_8017DC98);
