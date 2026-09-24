#include "common.h"

#include "main/mc.h"
#include "rooms/dryfield_trailer_coach.h"

/// The 0xFFFF-terminated item id lists `func_dryfield_trailer_coach_8017D7F4`
/// chooses from.
extern u16 D_dryfield_trailer_coach_80183950[];
extern u16 D_dryfield_trailer_coach_80183958[];
extern u16 D_dryfield_trailer_coach_80183960[];
extern u16 D_dryfield_trailer_coach_80183968[];
extern u16 D_dryfield_trailer_coach_80183978[];
extern u16 D_dryfield_trailer_coach_80183988[];
extern u16 D_dryfield_trailer_coach_80183998[];
extern u16 D_dryfield_trailer_coach_801839A0[];
extern u16 D_dryfield_trailer_coach_801839B0[];
extern u16 D_dryfield_trailer_coach_801839C0[];
extern u16 D_dryfield_trailer_coach_801839D0[];
extern u16 D_dryfield_trailer_coach_801839D8[];
extern u16 D_dryfield_trailer_coach_801839EC[];
extern u16 D_dryfield_trailer_coach_80183A04[];
extern u16 D_dryfield_trailer_coach_80183A18[];
extern u16 D_dryfield_trailer_coach_80183A20[];
extern u16 D_dryfield_trailer_coach_80183A30[];
extern u16 D_dryfield_trailer_coach_80183A48[];
extern u16 D_dryfield_trailer_coach_80183A5C[];
extern u16 D_dryfield_trailer_coach_80183A64[];
extern u16 D_dryfield_trailer_coach_80183A78[];
extern u16 D_dryfield_trailer_coach_80183A94[];
extern u16 D_dryfield_trailer_coach_80183AA4[];
extern u16 D_dryfield_trailer_coach_80183AB0[];
extern u16 D_dryfield_trailer_coach_80183AC8[];
extern u16 D_dryfield_trailer_coach_80183AE4[];
extern u16 D_dryfield_trailer_coach_80183AF8[];
extern u16 D_dryfield_trailer_coach_80183B00[];
extern u16 D_dryfield_trailer_coach_80183B14[];
extern u16 D_dryfield_trailer_coach_80183B34[];
extern u16 D_dryfield_trailer_coach_80183B44[];
extern u16 D_dryfield_trailer_coach_80183B50[];
extern u16 D_dryfield_trailer_coach_80183B68[];
extern u16 D_dryfield_trailer_coach_80183B6C[];
extern u16 D_dryfield_trailer_coach_80183B70[];
extern u16 D_dryfield_trailer_coach_80183B78[];
extern u16 D_dryfield_trailer_coach_80183B88[];
extern u16 D_dryfield_trailer_coach_80183B90[];
extern u16 D_dryfield_trailer_coach_80183B98[];
extern u16 D_dryfield_trailer_coach_80183BA0[];
extern u16 D_dryfield_trailer_coach_80183BAC[];
extern u16 D_dryfield_trailer_coach_80183BB4[];
extern u16 D_dryfield_trailer_coach_80183BC0[];
extern u16 D_dryfield_trailer_coach_80183BC8[];
extern u16 D_dryfield_trailer_coach_80183BD4[];
extern u16 D_dryfield_trailer_coach_80183BE0[];
extern u16 D_dryfield_trailer_coach_80183BE8[];
extern u16 D_dryfield_trailer_coach_80183BF0[];
extern u16 D_dryfield_trailer_coach_80183BFC[];
extern u16 D_dryfield_trailer_coach_80183C08[];
extern u16 D_dryfield_trailer_coach_80183C10[];
extern u16 D_dryfield_trailer_coach_80183C1C[];
extern u16 D_dryfield_trailer_coach_80183C28[];
extern u16 D_dryfield_trailer_coach_80183C34[];
extern u16 D_dryfield_trailer_coach_80183C38[];
extern u16 D_dryfield_trailer_coach_80183C44[];
extern u16 D_dryfield_trailer_coach_80183C50[];
extern u16 D_dryfield_trailer_coach_80183C5C[];
extern u16 D_dryfield_trailer_coach_80183C64[];
extern u16 D_dryfield_trailer_coach_80183C70[];
extern u16 D_dryfield_trailer_coach_80183C7C[];
extern u16 D_dryfield_trailer_coach_80183C88[];
extern u16 D_dryfield_trailer_coach_80183C90[];
extern u16 D_dryfield_trailer_coach_80183C9C[];

/// The list returned when no case matches.
extern u16 D_dryfield_trailer_coach_80183E2C[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_dryfield_trailer_coach_80183E2C`.
u16* func_dryfield_trailer_coach_8017D7F4(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183A30;
                    case 1:
                        return D_dryfield_trailer_coach_80183A48;
                    case 2:
                        return D_dryfield_trailer_coach_80183A5C;
                    case 3:
                        return D_dryfield_trailer_coach_80183A64;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183A78;
                    case 1:
                        return D_dryfield_trailer_coach_80183A94;
                    case 2:
                        return D_dryfield_trailer_coach_80183AA4;
                    case 3:
                        return D_dryfield_trailer_coach_80183AB0;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183AC8;
                    case 1:
                        return D_dryfield_trailer_coach_80183AE4;
                    case 2:
                        return D_dryfield_trailer_coach_80183AF8;
                    case 3:
                        return D_dryfield_trailer_coach_80183B00;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183B14;
                    case 1:
                        return D_dryfield_trailer_coach_80183B34;
                    case 2:
                        return D_dryfield_trailer_coach_80183B44;
                    case 3:
                        return D_dryfield_trailer_coach_80183B50;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183978;
                    case 1:
                        return D_dryfield_trailer_coach_80183988;
                    case 2:
                        return D_dryfield_trailer_coach_80183998;
                    case 3:
                        return D_dryfield_trailer_coach_801839A0;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_801839EC;
                    case 1:
                        return D_dryfield_trailer_coach_80183A04;
                    case 2:
                        return D_dryfield_trailer_coach_80183A18;
                    case 3:
                        return D_dryfield_trailer_coach_80183A20;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_801839B0;
                    case 1:
                        return D_dryfield_trailer_coach_801839C0;
                    case 2:
                        return D_dryfield_trailer_coach_801839D0;
                    case 3:
                        return D_dryfield_trailer_coach_801839D8;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183950;
                    case 1:
                        return D_dryfield_trailer_coach_80183958;
                    case 2:
                        return D_dryfield_trailer_coach_80183960;
                    case 3:
                        return D_dryfield_trailer_coach_80183968;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183BFC;
                    case 1:
                        return D_dryfield_trailer_coach_80183C08;
                    case 2:
                        return D_dryfield_trailer_coach_80183C10;
                    case 3:
                        return D_dryfield_trailer_coach_80183C1C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183C28;
                    case 1:
                        return D_dryfield_trailer_coach_80183C34;
                    case 2:
                        return D_dryfield_trailer_coach_80183C38;
                    case 3:
                        return D_dryfield_trailer_coach_80183C44;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183C50;
                    case 1:
                        return D_dryfield_trailer_coach_80183C5C;
                    case 2:
                        return D_dryfield_trailer_coach_80183C64;
                    case 3:
                        return D_dryfield_trailer_coach_80183C70;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183C7C;
                    case 1:
                        return D_dryfield_trailer_coach_80183C88;
                    case 2:
                        return D_dryfield_trailer_coach_80183C90;
                    case 3:
                        return D_dryfield_trailer_coach_80183C9C;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183B88;
                    case 1:
                        return D_dryfield_trailer_coach_80183B90;
                    case 2:
                        return D_dryfield_trailer_coach_80183B98;
                    case 3:
                        return D_dryfield_trailer_coach_80183BA0;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183BD4;
                    case 1:
                        return D_dryfield_trailer_coach_80183BE0;
                    case 2:
                        return D_dryfield_trailer_coach_80183BE8;
                    case 3:
                        return D_dryfield_trailer_coach_80183BF0;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183BAC;
                    case 1:
                        return D_dryfield_trailer_coach_80183BB4;
                    case 2:
                        return D_dryfield_trailer_coach_80183BC0;
                    case 3:
                        return D_dryfield_trailer_coach_80183BC8;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_trailer_coach_80183B68;
                    case 1:
                        return D_dryfield_trailer_coach_80183B6C;
                    case 2:
                        return D_dryfield_trailer_coach_80183B70;
                    case 3:
                        return D_dryfield_trailer_coach_80183B78;
                }
                break;
        }
    }
    return D_dryfield_trailer_coach_80183E2C;
}
