#include "common.h"
#include "rooms/shelter_b1_underground_parking.h"

#include "gameplay/D4.h"

#include <psyq/libgte.h>

extern SVECTOR D_shelter_b1_underground_parking_80187714[];
extern SVECTOR D_shelter_b1_underground_parking_8018771C[];
extern SVECTOR D_shelter_b1_underground_parking_80187724[];
extern SVECTOR D_shelter_b1_underground_parking_80187784[];
extern SVECTOR D_shelter_b1_underground_parking_801877A4[];

void func_shelter_b1_underground_parking_80184A18(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
        case 10:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                Room_Draw18(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                Room_Draw18(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            Room_Draw30(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            Room_Draw12(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 3:
        case 7:
        case 11:
        case 12:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                Room_Draw18(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                Room_Draw18(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            Room_Draw30(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            Room_Draw12(&D_shelter_b1_underground_parking_8018771C[11], 0x200, 0, 0x111);
            break;
        case 8:
        case 13:
            Room_Draw12(D_shelter_b1_underground_parking_801877A4, 0x200, 0, 0x210);
        case 4:
        case 14:
            Room_Draw12(&D_shelter_b1_underground_parking_80187724[0], 0x200, -0x400, 0x111);
            Room_Draw12(&D_shelter_b1_underground_parking_80187724[2], 0x200, -0x400, 0x111);
            Room_Draw12(&D_shelter_b1_underground_parking_80187724[4], 0x200, 0x800, 0x111);
            Room_Draw12(&D_shelter_b1_underground_parking_80187724[6], 0x200, 0x800, 0x111);
            Room_Draw12(&D_shelter_b1_underground_parking_80187724[8], 0x200, 0, 0x111);
            break;
        case 9:
        case 15:
        case 22:
        case 24:
            Room_Draw12(&D_shelter_b1_underground_parking_80187784[0], 0x200, 0, 0x111);
            Room_Draw12(&D_shelter_b1_underground_parking_80187784[2], 0x200, 0x800, 0x111);
            break;
        case 16:
            Room_Draw30(D_shelter_b1_underground_parking_8018771C, 0x300, 0x10);
            break;
        case 18:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                Room_Draw18(D_shelter_b1_underground_parking_80187714, 0x180, 0xC0);
            } else {
                Room_Draw18(D_shelter_b1_underground_parking_80187714, 0x60, 0xC0);
            }
            break;
        case 20:
            if (D_shelter_b1_underground_parking_8018D78C != 0) {
                Room_Draw05(D_shelter_b1_underground_parking_80187714, 0x180, 0x80);
            } else {
                Room_Draw05(D_shelter_b1_underground_parking_80187714, 0x60, 0x80);
            }
            break;
    }
}
