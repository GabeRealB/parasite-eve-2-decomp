#include "common.h"

#include "main/mc.h"

/// The 0xFFFF-terminated item id lists `func_shelter_1f_heliport_8017D730`
/// chooses from, and the one it returns when no case matches.
extern u16 D_shelter_1f_heliport_80180B54[];
extern u16 D_shelter_1f_heliport_80180B5C[];
extern u16 D_shelter_1f_heliport_80180B64[];
extern u16 D_shelter_1f_heliport_80180B6C[];
extern u16 D_shelter_1f_heliport_80180B7C[];
extern u16 D_shelter_1f_heliport_80180B8C[];
extern u16 D_shelter_1f_heliport_80180B9C[];
extern u16 D_shelter_1f_heliport_80180BA4[];
extern u16 D_shelter_1f_heliport_80180BB4[];
extern u16 D_shelter_1f_heliport_80180BC4[];
extern u16 D_shelter_1f_heliport_80180BD4[];
extern u16 D_shelter_1f_heliport_80180BDC[];
extern u16 D_shelter_1f_heliport_80180BF0[];
extern u16 D_shelter_1f_heliport_80180C08[];
extern u16 D_shelter_1f_heliport_80180C1C[];
extern u16 D_shelter_1f_heliport_80180C24[];
extern u16 D_shelter_1f_heliport_80180C34[];
extern u16 D_shelter_1f_heliport_80180C4C[];
extern u16 D_shelter_1f_heliport_80180C60[];
extern u16 D_shelter_1f_heliport_80180C68[];
extern u16 D_shelter_1f_heliport_80180C7C[];
extern u16 D_shelter_1f_heliport_80180C98[];
extern u16 D_shelter_1f_heliport_80180CA8[];
extern u16 D_shelter_1f_heliport_80180CB4[];
extern u16 D_shelter_1f_heliport_80180CCC[];
extern u16 D_shelter_1f_heliport_80180CE8[];
extern u16 D_shelter_1f_heliport_80180CFC[];
extern u16 D_shelter_1f_heliport_80180D04[];
extern u16 D_shelter_1f_heliport_80180D18[];
extern u16 D_shelter_1f_heliport_80180D38[];
extern u16 D_shelter_1f_heliport_80180D48[];
extern u16 D_shelter_1f_heliport_80180D54[];
extern u16 D_shelter_1f_heliport_80180D6C[];
extern u16 D_shelter_1f_heliport_80180D70[];
extern u16 D_shelter_1f_heliport_80180D74[];
extern u16 D_shelter_1f_heliport_80180D7C[];
extern u16 D_shelter_1f_heliport_80180D8C[];
extern u16 D_shelter_1f_heliport_80180D94[];
extern u16 D_shelter_1f_heliport_80180D9C[];
extern u16 D_shelter_1f_heliport_80180DA4[];
extern u16 D_shelter_1f_heliport_80180DB0[];
extern u16 D_shelter_1f_heliport_80180DB8[];
extern u16 D_shelter_1f_heliport_80180DC4[];
extern u16 D_shelter_1f_heliport_80180DCC[];
extern u16 D_shelter_1f_heliport_80180DD8[];
extern u16 D_shelter_1f_heliport_80180DE4[];
extern u16 D_shelter_1f_heliport_80180DEC[];
extern u16 D_shelter_1f_heliport_80180DF4[];
extern u16 D_shelter_1f_heliport_80180E00[];
extern u16 D_shelter_1f_heliport_80180E0C[];
extern u16 D_shelter_1f_heliport_80180E14[];
extern u16 D_shelter_1f_heliport_80180E20[];
extern u16 D_shelter_1f_heliport_80180E2C[];
extern u16 D_shelter_1f_heliport_80180E38[];
extern u16 D_shelter_1f_heliport_80180E3C[];
extern u16 D_shelter_1f_heliport_80180E48[];
extern u16 D_shelter_1f_heliport_80180E54[];
extern u16 D_shelter_1f_heliport_80180E60[];
extern u16 D_shelter_1f_heliport_80180E68[];
extern u16 D_shelter_1f_heliport_80180E74[];
extern u16 D_shelter_1f_heliport_80180E80[];
extern u16 D_shelter_1f_heliport_80180E8C[];
extern u16 D_shelter_1f_heliport_80180E94[];
extern u16 D_shelter_1f_heliport_80180EA0[];
extern u16 D_shelter_1f_heliport_80181030[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_shelter_1f_heliport_80181030`.
u16* func_shelter_1f_heliport_8017D730(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180C34;
                    case 1:
                        return D_shelter_1f_heliport_80180C4C;
                    case 2:
                        return D_shelter_1f_heliport_80180C60;
                    case 3:
                        return D_shelter_1f_heliport_80180C68;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180C7C;
                    case 1:
                        return D_shelter_1f_heliport_80180C98;
                    case 2:
                        return D_shelter_1f_heliport_80180CA8;
                    case 3:
                        return D_shelter_1f_heliport_80180CB4;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180CCC;
                    case 1:
                        return D_shelter_1f_heliport_80180CE8;
                    case 2:
                        return D_shelter_1f_heliport_80180CFC;
                    case 3:
                        return D_shelter_1f_heliport_80180D04;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180D18;
                    case 1:
                        return D_shelter_1f_heliport_80180D38;
                    case 2:
                        return D_shelter_1f_heliport_80180D48;
                    case 3:
                        return D_shelter_1f_heliport_80180D54;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180B7C;
                    case 1:
                        return D_shelter_1f_heliport_80180B8C;
                    case 2:
                        return D_shelter_1f_heliport_80180B9C;
                    case 3:
                        return D_shelter_1f_heliport_80180BA4;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180BF0;
                    case 1:
                        return D_shelter_1f_heliport_80180C08;
                    case 2:
                        return D_shelter_1f_heliport_80180C1C;
                    case 3:
                        return D_shelter_1f_heliport_80180C24;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180BB4;
                    case 1:
                        return D_shelter_1f_heliport_80180BC4;
                    case 2:
                        return D_shelter_1f_heliport_80180BD4;
                    case 3:
                        return D_shelter_1f_heliport_80180BDC;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180B54;
                    case 1:
                        return D_shelter_1f_heliport_80180B5C;
                    case 2:
                        return D_shelter_1f_heliport_80180B64;
                    case 3:
                        return D_shelter_1f_heliport_80180B6C;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E00;
                    case 1:
                        return D_shelter_1f_heliport_80180E0C;
                    case 2:
                        return D_shelter_1f_heliport_80180E14;
                    case 3:
                        return D_shelter_1f_heliport_80180E20;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E2C;
                    case 1:
                        return D_shelter_1f_heliport_80180E38;
                    case 2:
                        return D_shelter_1f_heliport_80180E3C;
                    case 3:
                        return D_shelter_1f_heliport_80180E48;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E54;
                    case 1:
                        return D_shelter_1f_heliport_80180E60;
                    case 2:
                        return D_shelter_1f_heliport_80180E68;
                    case 3:
                        return D_shelter_1f_heliport_80180E74;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180E80;
                    case 1:
                        return D_shelter_1f_heliport_80180E8C;
                    case 2:
                        return D_shelter_1f_heliport_80180E94;
                    case 3:
                        return D_shelter_1f_heliport_80180EA0;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180D8C;
                    case 1:
                        return D_shelter_1f_heliport_80180D94;
                    case 2:
                        return D_shelter_1f_heliport_80180D9C;
                    case 3:
                        return D_shelter_1f_heliport_80180DA4;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180DD8;
                    case 1:
                        return D_shelter_1f_heliport_80180DE4;
                    case 2:
                        return D_shelter_1f_heliport_80180DEC;
                    case 3:
                        return D_shelter_1f_heliport_80180DF4;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180DB0;
                    case 1:
                        return D_shelter_1f_heliport_80180DB8;
                    case 2:
                        return D_shelter_1f_heliport_80180DC4;
                    case 3:
                        return D_shelter_1f_heliport_80180DCC;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_1f_heliport_80180D6C;
                    case 1:
                        return D_shelter_1f_heliport_80180D70;
                    case 2:
                        return D_shelter_1f_heliport_80180D74;
                    case 3:
                        return D_shelter_1f_heliport_80180D7C;
                }
                break;
        }
    }
    return D_shelter_1f_heliport_80181030;
}
