#include "common.h"

#include "main/mc.h"

#include "rooms/shelter_b1_underground_parking.h"

extern u16 D_shelter_b1_underground_parking_80186BBC[];
extern u16 D_shelter_b1_underground_parking_80186BC4[];
extern u16 D_shelter_b1_underground_parking_80186BCC[];
extern u16 D_shelter_b1_underground_parking_80186BD4[];
extern u16 D_shelter_b1_underground_parking_80186BE4[];
extern u16 D_shelter_b1_underground_parking_80186BF4[];
extern u16 D_shelter_b1_underground_parking_80186C04[];
extern u16 D_shelter_b1_underground_parking_80186C0C[];
extern u16 D_shelter_b1_underground_parking_80186C1C[];
extern u16 D_shelter_b1_underground_parking_80186C2C[];
extern u16 D_shelter_b1_underground_parking_80186C3C[];
extern u16 D_shelter_b1_underground_parking_80186C44[];
extern u16 D_shelter_b1_underground_parking_80186C58[];
extern u16 D_shelter_b1_underground_parking_80186C70[];
extern u16 D_shelter_b1_underground_parking_80186C84[];
extern u16 D_shelter_b1_underground_parking_80186C8C[];
extern u16 D_shelter_b1_underground_parking_80186C9C[];
extern u16 D_shelter_b1_underground_parking_80186CB4[];
extern u16 D_shelter_b1_underground_parking_80186CC8[];
extern u16 D_shelter_b1_underground_parking_80186CD0[];
extern u16 D_shelter_b1_underground_parking_80186CE4[];
extern u16 D_shelter_b1_underground_parking_80186D00[];
extern u16 D_shelter_b1_underground_parking_80186D10[];
extern u16 D_shelter_b1_underground_parking_80186D1C[];
extern u16 D_shelter_b1_underground_parking_80186D34[];
extern u16 D_shelter_b1_underground_parking_80186D50[];
extern u16 D_shelter_b1_underground_parking_80186D64[];
extern u16 D_shelter_b1_underground_parking_80186D6C[];
extern u16 D_shelter_b1_underground_parking_80186D80[];
extern u16 D_shelter_b1_underground_parking_80186DA0[];
extern u16 D_shelter_b1_underground_parking_80186DB0[];
extern u16 D_shelter_b1_underground_parking_80186DBC[];
extern u16 D_shelter_b1_underground_parking_80186DD4[];
extern u16 D_shelter_b1_underground_parking_80186DD8[];
extern u16 D_shelter_b1_underground_parking_80186DDC[];
extern u16 D_shelter_b1_underground_parking_80186DE4[];
extern u16 D_shelter_b1_underground_parking_80186DF4[];
extern u16 D_shelter_b1_underground_parking_80186DFC[];
extern u16 D_shelter_b1_underground_parking_80186E04[];
extern u16 D_shelter_b1_underground_parking_80186E0C[];
extern u16 D_shelter_b1_underground_parking_80186E18[];
extern u16 D_shelter_b1_underground_parking_80186E20[];
extern u16 D_shelter_b1_underground_parking_80186E2C[];
extern u16 D_shelter_b1_underground_parking_80186E34[];
extern u16 D_shelter_b1_underground_parking_80186E40[];
extern u16 D_shelter_b1_underground_parking_80186E4C[];
extern u16 D_shelter_b1_underground_parking_80186E54[];
extern u16 D_shelter_b1_underground_parking_80186E5C[];
extern u16 D_shelter_b1_underground_parking_80186E68[];
extern u16 D_shelter_b1_underground_parking_80186E74[];
extern u16 D_shelter_b1_underground_parking_80186E7C[];
extern u16 D_shelter_b1_underground_parking_80186E88[];
extern u16 D_shelter_b1_underground_parking_80186E94[];
extern u16 D_shelter_b1_underground_parking_80186EA0[];
extern u16 D_shelter_b1_underground_parking_80186EA4[];
extern u16 D_shelter_b1_underground_parking_80186EB0[];
extern u16 D_shelter_b1_underground_parking_80186EBC[];
extern u16 D_shelter_b1_underground_parking_80186EC8[];
extern u16 D_shelter_b1_underground_parking_80186ED0[];
extern u16 D_shelter_b1_underground_parking_80186EDC[];
extern u16 D_shelter_b1_underground_parking_80186EE8[];
extern u16 D_shelter_b1_underground_parking_80186EF4[];
extern u16 D_shelter_b1_underground_parking_80186EFC[];
extern u16 D_shelter_b1_underground_parking_80186F08[];
extern u16 D_shelter_b1_underground_parking_80187098[];

/// Returns the 0xFFFF-terminated list of item ids the shop list starts from.
/// `Mc_SaveData.gameMode` picks one of two sets of lists (below 2, or 2 and
/// up); within a set the low half of `mode` picks the list group (0x20, 0x21,
/// 0x30-0x33 and 0x40 each have one, every other value shares one) and the
/// high half the entry within it (0-3). A high half outside 0-3 gets a single
/// fallback list.
u16* func_shelter_b1_underground_parking_8017F80C(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C9C;
                    case 1:
                        return D_shelter_b1_underground_parking_80186CB4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186CC8;
                    case 3:
                        return D_shelter_b1_underground_parking_80186CD0;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186CE4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186D00;
                    case 2:
                        return D_shelter_b1_underground_parking_80186D10;
                    case 3:
                        return D_shelter_b1_underground_parking_80186D1C;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186D34;
                    case 1:
                        return D_shelter_b1_underground_parking_80186D50;
                    case 2:
                        return D_shelter_b1_underground_parking_80186D64;
                    case 3:
                        return D_shelter_b1_underground_parking_80186D6C;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186D80;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DA0;
                    case 2:
                        return D_shelter_b1_underground_parking_80186DB0;
                    case 3:
                        return D_shelter_b1_underground_parking_80186DBC;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186BE4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186BF4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C04;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C0C;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C58;
                    case 1:
                        return D_shelter_b1_underground_parking_80186C70;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C84;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C8C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186C1C;
                    case 1:
                        return D_shelter_b1_underground_parking_80186C2C;
                    case 2:
                        return D_shelter_b1_underground_parking_80186C3C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186C44;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186BBC;
                    case 1:
                        return D_shelter_b1_underground_parking_80186BC4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186BCC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186BD4;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E68;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E74;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E7C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E88;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E94;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EA0;
                    case 2:
                        return D_shelter_b1_underground_parking_80186EA4;
                    case 3:
                        return D_shelter_b1_underground_parking_80186EB0;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186EBC;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EC8;
                    case 2:
                        return D_shelter_b1_underground_parking_80186ED0;
                    case 3:
                        return D_shelter_b1_underground_parking_80186EDC;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186EE8;
                    case 1:
                        return D_shelter_b1_underground_parking_80186EF4;
                    case 2:
                        return D_shelter_b1_underground_parking_80186EFC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186F08;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186DF4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DFC;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E04;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E0C;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E40;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E4C;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E54;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E5C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186E18;
                    case 1:
                        return D_shelter_b1_underground_parking_80186E20;
                    case 2:
                        return D_shelter_b1_underground_parking_80186E2C;
                    case 3:
                        return D_shelter_b1_underground_parking_80186E34;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_underground_parking_80186DD4;
                    case 1:
                        return D_shelter_b1_underground_parking_80186DD8;
                    case 2:
                        return D_shelter_b1_underground_parking_80186DDC;
                    case 3:
                        return D_shelter_b1_underground_parking_80186DE4;
                }
                break;
        }
    }
    return D_shelter_b1_underground_parking_80187098;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D750);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D758);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D75C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D764);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D76C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_underground_parking/shelter_b1_underground_parking", D_shelter_b1_underground_parking_8017D774);
