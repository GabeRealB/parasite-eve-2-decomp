#include "common.h"

#include "main/mc.h"
#include "rooms/dryfield_night_garage.h"

/// The 0xFFFF-terminated item id lists `func_dryfield_night_garage_8017D754`
/// chooses from.
extern u16 D_dryfield_night_garage_801815F8[];
extern u16 D_dryfield_night_garage_80181600[];
extern u16 D_dryfield_night_garage_80181608[];
extern u16 D_dryfield_night_garage_80181610[];
extern u16 D_dryfield_night_garage_80181620[];
extern u16 D_dryfield_night_garage_80181630[];
extern u16 D_dryfield_night_garage_80181640[];
extern u16 D_dryfield_night_garage_80181648[];
extern u16 D_dryfield_night_garage_80181658[];
extern u16 D_dryfield_night_garage_80181668[];
extern u16 D_dryfield_night_garage_80181678[];
extern u16 D_dryfield_night_garage_80181680[];
extern u16 D_dryfield_night_garage_80181694[];
extern u16 D_dryfield_night_garage_801816AC[];
extern u16 D_dryfield_night_garage_801816C0[];
extern u16 D_dryfield_night_garage_801816C8[];
extern u16 D_dryfield_night_garage_801816D8[];
extern u16 D_dryfield_night_garage_801816F0[];
extern u16 D_dryfield_night_garage_80181704[];
extern u16 D_dryfield_night_garage_8018170C[];
extern u16 D_dryfield_night_garage_80181720[];
extern u16 D_dryfield_night_garage_8018173C[];
extern u16 D_dryfield_night_garage_8018174C[];
extern u16 D_dryfield_night_garage_80181758[];
extern u16 D_dryfield_night_garage_80181770[];
extern u16 D_dryfield_night_garage_8018178C[];
extern u16 D_dryfield_night_garage_801817A0[];
extern u16 D_dryfield_night_garage_801817A8[];
extern u16 D_dryfield_night_garage_801817BC[];
extern u16 D_dryfield_night_garage_801817DC[];
extern u16 D_dryfield_night_garage_801817EC[];
extern u16 D_dryfield_night_garage_801817F8[];
extern u16 D_dryfield_night_garage_80181810[];
extern u16 D_dryfield_night_garage_80181814[];
extern u16 D_dryfield_night_garage_80181818[];
extern u16 D_dryfield_night_garage_80181820[];
extern u16 D_dryfield_night_garage_80181830[];
extern u16 D_dryfield_night_garage_80181838[];
extern u16 D_dryfield_night_garage_80181840[];
extern u16 D_dryfield_night_garage_80181848[];
extern u16 D_dryfield_night_garage_80181854[];
extern u16 D_dryfield_night_garage_8018185C[];
extern u16 D_dryfield_night_garage_80181868[];
extern u16 D_dryfield_night_garage_80181870[];
extern u16 D_dryfield_night_garage_8018187C[];
extern u16 D_dryfield_night_garage_80181888[];
extern u16 D_dryfield_night_garage_80181890[];
extern u16 D_dryfield_night_garage_80181898[];
extern u16 D_dryfield_night_garage_801818A4[];
extern u16 D_dryfield_night_garage_801818B0[];
extern u16 D_dryfield_night_garage_801818B8[];
extern u16 D_dryfield_night_garage_801818C4[];
extern u16 D_dryfield_night_garage_801818D0[];
extern u16 D_dryfield_night_garage_801818DC[];
extern u16 D_dryfield_night_garage_801818E0[];
extern u16 D_dryfield_night_garage_801818EC[];
extern u16 D_dryfield_night_garage_801818F8[];
extern u16 D_dryfield_night_garage_80181904[];
extern u16 D_dryfield_night_garage_8018190C[];
extern u16 D_dryfield_night_garage_80181918[];
extern u16 D_dryfield_night_garage_80181924[];
extern u16 D_dryfield_night_garage_80181930[];
extern u16 D_dryfield_night_garage_80181938[];
extern u16 D_dryfield_night_garage_80181944[];

/// The list returned when no case matches.
extern u16 D_dryfield_night_garage_80181AD4[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_dryfield_night_garage_80181AD4`.
u16* func_dryfield_night_garage_8017D754(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801816D8;
                    case 1:
                        return D_dryfield_night_garage_801816F0;
                    case 2:
                        return D_dryfield_night_garage_80181704;
                    case 3:
                        return D_dryfield_night_garage_8018170C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181720;
                    case 1:
                        return D_dryfield_night_garage_8018173C;
                    case 2:
                        return D_dryfield_night_garage_8018174C;
                    case 3:
                        return D_dryfield_night_garage_80181758;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181770;
                    case 1:
                        return D_dryfield_night_garage_8018178C;
                    case 2:
                        return D_dryfield_night_garage_801817A0;
                    case 3:
                        return D_dryfield_night_garage_801817A8;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801817BC;
                    case 1:
                        return D_dryfield_night_garage_801817DC;
                    case 2:
                        return D_dryfield_night_garage_801817EC;
                    case 3:
                        return D_dryfield_night_garage_801817F8;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181620;
                    case 1:
                        return D_dryfield_night_garage_80181630;
                    case 2:
                        return D_dryfield_night_garage_80181640;
                    case 3:
                        return D_dryfield_night_garage_80181648;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181694;
                    case 1:
                        return D_dryfield_night_garage_801816AC;
                    case 2:
                        return D_dryfield_night_garage_801816C0;
                    case 3:
                        return D_dryfield_night_garage_801816C8;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181658;
                    case 1:
                        return D_dryfield_night_garage_80181668;
                    case 2:
                        return D_dryfield_night_garage_80181678;
                    case 3:
                        return D_dryfield_night_garage_80181680;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801815F8;
                    case 1:
                        return D_dryfield_night_garage_80181600;
                    case 2:
                        return D_dryfield_night_garage_80181608;
                    case 3:
                        return D_dryfield_night_garage_80181610;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801818A4;
                    case 1:
                        return D_dryfield_night_garage_801818B0;
                    case 2:
                        return D_dryfield_night_garage_801818B8;
                    case 3:
                        return D_dryfield_night_garage_801818C4;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801818D0;
                    case 1:
                        return D_dryfield_night_garage_801818DC;
                    case 2:
                        return D_dryfield_night_garage_801818E0;
                    case 3:
                        return D_dryfield_night_garage_801818EC;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_801818F8;
                    case 1:
                        return D_dryfield_night_garage_80181904;
                    case 2:
                        return D_dryfield_night_garage_8018190C;
                    case 3:
                        return D_dryfield_night_garage_80181918;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181924;
                    case 1:
                        return D_dryfield_night_garage_80181930;
                    case 2:
                        return D_dryfield_night_garage_80181938;
                    case 3:
                        return D_dryfield_night_garage_80181944;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181830;
                    case 1:
                        return D_dryfield_night_garage_80181838;
                    case 2:
                        return D_dryfield_night_garage_80181840;
                    case 3:
                        return D_dryfield_night_garage_80181848;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_8018187C;
                    case 1:
                        return D_dryfield_night_garage_80181888;
                    case 2:
                        return D_dryfield_night_garage_80181890;
                    case 3:
                        return D_dryfield_night_garage_80181898;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181854;
                    case 1:
                        return D_dryfield_night_garage_8018185C;
                    case 2:
                        return D_dryfield_night_garage_80181868;
                    case 3:
                        return D_dryfield_night_garage_80181870;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_garage_80181810;
                    case 1:
                        return D_dryfield_night_garage_80181814;
                    case 2:
                        return D_dryfield_night_garage_80181818;
                    case 3:
                        return D_dryfield_night_garage_80181820;
                }
                break;
        }
    }
    return D_dryfield_night_garage_80181AD4;
}
