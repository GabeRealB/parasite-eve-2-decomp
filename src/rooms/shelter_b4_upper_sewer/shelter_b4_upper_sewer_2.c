#include "common.h"
#include "main/fs.h"
#include "main/session.h"

extern u8   D_8007216C;
extern u8   D_shelter_b4_upper_sewer_80188D2C;
extern void func_shelter_b4_upper_sewer_8017E59C(s32);

extern s32 D_8007107C;
extern s8  D_8007217B;
extern s16 D_shelter_b4_upper_sewer_80186438;
extern u8  D_shelter_b4_upper_sewer_80186448[];
extern u8  D_shelter_b4_upper_sewer_80186454[];
extern u8* D_shelter_b4_upper_sewer_80188D30;

void func_shelter_b4_upper_sewer_8017DD98(s32, u8*, s16, u8);

void func_shelter_b4_upper_sewer_8017DC88(s32 arg0)
{
    s16 w;
    u8  c;
    s32 h;

    if (D_8007217B == 0) {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    if (D_shelter_b4_upper_sewer_80186438 < -0x640) {
        D_shelter_b4_upper_sewer_80186438 = -0x640;
    } else if (D_shelter_b4_upper_sewer_80186438 > 0) {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    w                    = D_shelter_b4_upper_sewer_80186438;
    gGameSession->waterY = w;
    h                    = w;
    c                    = (-h * 16) / 225;
    if (gGameSession->at4.loc.view != 0xC) {
        func_shelter_b4_upper_sewer_8017DD98(arg0, D_shelter_b4_upper_sewer_80186448, h, c);
    } else {
        func_shelter_b4_upper_sewer_8017DD98(arg0, D_shelter_b4_upper_sewer_80186454, h, c);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer_2", func_shelter_b4_upper_sewer_8017DD98);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer_2", func_shelter_b4_upper_sewer_8017E4F4);
