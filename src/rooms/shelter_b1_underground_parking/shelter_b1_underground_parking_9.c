#include "common.h"

#include <psyq/libgte.h>

#include "rooms/shelter_b1_underground_parking.h"

extern DVECTOR D_shelter_b1_underground_parking_801876D4[];
extern u8      D_shelter_b1_underground_parking_8018D789;

void func_shelter_b1_underground_parking_80183B9C(void)
{
    if (D_shelter_b1_underground_parking_8018D789 & 8) {
        func_shelter_b1_underground_parking_801857E0(-0x46, 0x54, 7, 0xF00);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 4) {
        func_shelter_b1_underground_parking_801857E0(-0x28, 0x54, 7, 0xF0);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 2) {
        func_shelter_b1_underground_parking_801857E0(-0xE, 0x54, 7, 0xF);
    }
    if (D_shelter_b1_underground_parking_8018D789 & 1) {
        func_shelter_b1_underground_parking_801857E0(0xC, 0x54, 7, 0xFF0);
    }
    func_shelter_b1_underground_parking_801857E0(0x11, -0x3D, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x3D, -0x3D, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x10, 9, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(-0x42, -0x19, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(-0x17, 0x2B, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(0x3D, 0x2B, 0xA, 0x111);
    func_shelter_b1_underground_parking_801857E0(
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vx,
        D_shelter_b1_underground_parking_801876D4[D_shelter_b1_underground_parking_8018D789].vy, 7, 0xF0);
}
