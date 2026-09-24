#include "common.h"

#include "main/mc.h"
#include "rooms/shelter_b1_armory.h"

/// The 0xFFFF-terminated item id lists `func_shelter_b1_armory_8017D768`
/// chooses from, and the one it returns when no case matches.
extern u16 D_shelter_b1_armory_80181E9C[];
extern u16 D_shelter_b1_armory_80181EA4[];
extern u16 D_shelter_b1_armory_80181EAC[];
extern u16 D_shelter_b1_armory_80181EB4[];
extern u16 D_shelter_b1_armory_80181EC4[];
extern u16 D_shelter_b1_armory_80181ED4[];
extern u16 D_shelter_b1_armory_80181EE4[];
extern u16 D_shelter_b1_armory_80181EEC[];
extern u16 D_shelter_b1_armory_80181EFC[];
extern u16 D_shelter_b1_armory_80181F0C[];
extern u16 D_shelter_b1_armory_80181F1C[];
extern u16 D_shelter_b1_armory_80181F24[];
extern u16 D_shelter_b1_armory_80181F38[];
extern u16 D_shelter_b1_armory_80181F50[];
extern u16 D_shelter_b1_armory_80181F64[];
extern u16 D_shelter_b1_armory_80181F6C[];
extern u16 D_shelter_b1_armory_80181F7C[];
extern u16 D_shelter_b1_armory_80181F94[];
extern u16 D_shelter_b1_armory_80181FA8[];
extern u16 D_shelter_b1_armory_80181FB0[];
extern u16 D_shelter_b1_armory_80181FC4[];
extern u16 D_shelter_b1_armory_80181FE0[];
extern u16 D_shelter_b1_armory_80181FF0[];
extern u16 D_shelter_b1_armory_80181FFC[];
extern u16 D_shelter_b1_armory_80182014[];
extern u16 D_shelter_b1_armory_80182030[];
extern u16 D_shelter_b1_armory_80182044[];
extern u16 D_shelter_b1_armory_8018204C[];
extern u16 D_shelter_b1_armory_80182060[];
extern u16 D_shelter_b1_armory_80182080[];
extern u16 D_shelter_b1_armory_80182090[];
extern u16 D_shelter_b1_armory_8018209C[];
extern u16 D_shelter_b1_armory_801820B4[];
extern u16 D_shelter_b1_armory_801820B8[];
extern u16 D_shelter_b1_armory_801820BC[];
extern u16 D_shelter_b1_armory_801820C4[];
extern u16 D_shelter_b1_armory_801820D4[];
extern u16 D_shelter_b1_armory_801820DC[];
extern u16 D_shelter_b1_armory_801820E4[];
extern u16 D_shelter_b1_armory_801820EC[];
extern u16 D_shelter_b1_armory_801820F8[];
extern u16 D_shelter_b1_armory_80182100[];
extern u16 D_shelter_b1_armory_8018210C[];
extern u16 D_shelter_b1_armory_80182114[];
extern u16 D_shelter_b1_armory_80182120[];
extern u16 D_shelter_b1_armory_8018212C[];
extern u16 D_shelter_b1_armory_80182134[];
extern u16 D_shelter_b1_armory_8018213C[];
extern u16 D_shelter_b1_armory_80182148[];
extern u16 D_shelter_b1_armory_80182154[];
extern u16 D_shelter_b1_armory_8018215C[];
extern u16 D_shelter_b1_armory_80182168[];
extern u16 D_shelter_b1_armory_80182174[];
extern u16 D_shelter_b1_armory_80182180[];
extern u16 D_shelter_b1_armory_80182184[];
extern u16 D_shelter_b1_armory_80182190[];
extern u16 D_shelter_b1_armory_8018219C[];
extern u16 D_shelter_b1_armory_801821A8[];
extern u16 D_shelter_b1_armory_801821B0[];
extern u16 D_shelter_b1_armory_801821BC[];
extern u16 D_shelter_b1_armory_801821C8[];
extern u16 D_shelter_b1_armory_801821D4[];
extern u16 D_shelter_b1_armory_801821DC[];
extern u16 D_shelter_b1_armory_801821E8[];
extern u16 D_shelter_b1_armory_80182378[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_shelter_b1_armory_80182378`.
u16* func_shelter_b1_armory_8017D768(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181F7C;
                    case 1:
                        return D_shelter_b1_armory_80181F94;
                    case 2:
                        return D_shelter_b1_armory_80181FA8;
                    case 3:
                        return D_shelter_b1_armory_80181FB0;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181FC4;
                    case 1:
                        return D_shelter_b1_armory_80181FE0;
                    case 2:
                        return D_shelter_b1_armory_80181FF0;
                    case 3:
                        return D_shelter_b1_armory_80181FFC;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182014;
                    case 1:
                        return D_shelter_b1_armory_80182030;
                    case 2:
                        return D_shelter_b1_armory_80182044;
                    case 3:
                        return D_shelter_b1_armory_8018204C;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182060;
                    case 1:
                        return D_shelter_b1_armory_80182080;
                    case 2:
                        return D_shelter_b1_armory_80182090;
                    case 3:
                        return D_shelter_b1_armory_8018209C;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181EC4;
                    case 1:
                        return D_shelter_b1_armory_80181ED4;
                    case 2:
                        return D_shelter_b1_armory_80181EE4;
                    case 3:
                        return D_shelter_b1_armory_80181EEC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181F38;
                    case 1:
                        return D_shelter_b1_armory_80181F50;
                    case 2:
                        return D_shelter_b1_armory_80181F64;
                    case 3:
                        return D_shelter_b1_armory_80181F6C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181EFC;
                    case 1:
                        return D_shelter_b1_armory_80181F0C;
                    case 2:
                        return D_shelter_b1_armory_80181F1C;
                    case 3:
                        return D_shelter_b1_armory_80181F24;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80181E9C;
                    case 1:
                        return D_shelter_b1_armory_80181EA4;
                    case 2:
                        return D_shelter_b1_armory_80181EAC;
                    case 3:
                        return D_shelter_b1_armory_80181EB4;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182148;
                    case 1:
                        return D_shelter_b1_armory_80182154;
                    case 2:
                        return D_shelter_b1_armory_8018215C;
                    case 3:
                        return D_shelter_b1_armory_80182168;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182174;
                    case 1:
                        return D_shelter_b1_armory_80182180;
                    case 2:
                        return D_shelter_b1_armory_80182184;
                    case 3:
                        return D_shelter_b1_armory_80182190;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_8018219C;
                    case 1:
                        return D_shelter_b1_armory_801821A8;
                    case 2:
                        return D_shelter_b1_armory_801821B0;
                    case 3:
                        return D_shelter_b1_armory_801821BC;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801821C8;
                    case 1:
                        return D_shelter_b1_armory_801821D4;
                    case 2:
                        return D_shelter_b1_armory_801821DC;
                    case 3:
                        return D_shelter_b1_armory_801821E8;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801820D4;
                    case 1:
                        return D_shelter_b1_armory_801820DC;
                    case 2:
                        return D_shelter_b1_armory_801820E4;
                    case 3:
                        return D_shelter_b1_armory_801820EC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_80182120;
                    case 1:
                        return D_shelter_b1_armory_8018212C;
                    case 2:
                        return D_shelter_b1_armory_80182134;
                    case 3:
                        return D_shelter_b1_armory_8018213C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801820F8;
                    case 1:
                        return D_shelter_b1_armory_80182100;
                    case 2:
                        return D_shelter_b1_armory_8018210C;
                    case 3:
                        return D_shelter_b1_armory_80182114;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_shelter_b1_armory_801820B4;
                    case 1:
                        return D_shelter_b1_armory_801820B8;
                    case 2:
                        return D_shelter_b1_armory_801820BC;
                    case 3:
                        return D_shelter_b1_armory_801820C4;
                }
                break;
        }
    }
    return D_shelter_b1_armory_80182378;
}
