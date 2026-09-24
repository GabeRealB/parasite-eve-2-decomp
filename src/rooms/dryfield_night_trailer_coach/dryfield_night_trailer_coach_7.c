#include "common.h"

#include "main/mc.h"
#include "rooms/dryfield_night_trailer_coach.h"

/// The 0xFFFF-terminated item id lists `func_dryfield_night_trailer_coach_8017D81C`
/// chooses from.
extern u16 D_dryfield_night_trailer_coach_8018409C[];
extern u16 D_dryfield_night_trailer_coach_801840A4[];
extern u16 D_dryfield_night_trailer_coach_801840AC[];
extern u16 D_dryfield_night_trailer_coach_801840B4[];
extern u16 D_dryfield_night_trailer_coach_801840C4[];
extern u16 D_dryfield_night_trailer_coach_801840D4[];
extern u16 D_dryfield_night_trailer_coach_801840E4[];
extern u16 D_dryfield_night_trailer_coach_801840EC[];
extern u16 D_dryfield_night_trailer_coach_801840FC[];
extern u16 D_dryfield_night_trailer_coach_8018410C[];
extern u16 D_dryfield_night_trailer_coach_8018411C[];
extern u16 D_dryfield_night_trailer_coach_80184124[];
extern u16 D_dryfield_night_trailer_coach_80184138[];
extern u16 D_dryfield_night_trailer_coach_80184150[];
extern u16 D_dryfield_night_trailer_coach_80184164[];
extern u16 D_dryfield_night_trailer_coach_8018416C[];
extern u16 D_dryfield_night_trailer_coach_8018417C[];
extern u16 D_dryfield_night_trailer_coach_80184194[];
extern u16 D_dryfield_night_trailer_coach_801841A8[];
extern u16 D_dryfield_night_trailer_coach_801841B0[];
extern u16 D_dryfield_night_trailer_coach_801841C4[];
extern u16 D_dryfield_night_trailer_coach_801841E0[];
extern u16 D_dryfield_night_trailer_coach_801841F0[];
extern u16 D_dryfield_night_trailer_coach_801841FC[];
extern u16 D_dryfield_night_trailer_coach_80184214[];
extern u16 D_dryfield_night_trailer_coach_80184230[];
extern u16 D_dryfield_night_trailer_coach_80184244[];
extern u16 D_dryfield_night_trailer_coach_8018424C[];
extern u16 D_dryfield_night_trailer_coach_80184260[];
extern u16 D_dryfield_night_trailer_coach_80184280[];
extern u16 D_dryfield_night_trailer_coach_80184290[];
extern u16 D_dryfield_night_trailer_coach_8018429C[];
extern u16 D_dryfield_night_trailer_coach_801842B4[];
extern u16 D_dryfield_night_trailer_coach_801842B8[];
extern u16 D_dryfield_night_trailer_coach_801842BC[];
extern u16 D_dryfield_night_trailer_coach_801842C4[];
extern u16 D_dryfield_night_trailer_coach_801842D4[];
extern u16 D_dryfield_night_trailer_coach_801842DC[];
extern u16 D_dryfield_night_trailer_coach_801842E4[];
extern u16 D_dryfield_night_trailer_coach_801842EC[];
extern u16 D_dryfield_night_trailer_coach_801842F8[];
extern u16 D_dryfield_night_trailer_coach_80184300[];
extern u16 D_dryfield_night_trailer_coach_8018430C[];
extern u16 D_dryfield_night_trailer_coach_80184314[];
extern u16 D_dryfield_night_trailer_coach_80184320[];
extern u16 D_dryfield_night_trailer_coach_8018432C[];
extern u16 D_dryfield_night_trailer_coach_80184334[];
extern u16 D_dryfield_night_trailer_coach_8018433C[];
extern u16 D_dryfield_night_trailer_coach_80184348[];
extern u16 D_dryfield_night_trailer_coach_80184354[];
extern u16 D_dryfield_night_trailer_coach_8018435C[];
extern u16 D_dryfield_night_trailer_coach_80184368[];
extern u16 D_dryfield_night_trailer_coach_80184374[];
extern u16 D_dryfield_night_trailer_coach_80184380[];
extern u16 D_dryfield_night_trailer_coach_80184384[];
extern u16 D_dryfield_night_trailer_coach_80184390[];
extern u16 D_dryfield_night_trailer_coach_8018439C[];
extern u16 D_dryfield_night_trailer_coach_801843A8[];
extern u16 D_dryfield_night_trailer_coach_801843B0[];
extern u16 D_dryfield_night_trailer_coach_801843BC[];
extern u16 D_dryfield_night_trailer_coach_801843C8[];
extern u16 D_dryfield_night_trailer_coach_801843D4[];
extern u16 D_dryfield_night_trailer_coach_801843DC[];
extern u16 D_dryfield_night_trailer_coach_801843E8[];

/// The list returned when no case matches.
extern u16 D_dryfield_night_trailer_coach_80184578[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists (0x20, 0x21, 0x30-0x33, 0x40
/// or any other value) and the high halfword one of the group's four;
/// `Mc_SaveData.gameMode` 2 and above has groups of its own. A high halfword
/// above 3 falls through the 0x30-0x33 groups in turn and on into 0x20's;
/// every other miss returns `D_dryfield_night_trailer_coach_80184578`.
u16* func_dryfield_night_trailer_coach_8017D81C(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_8018417C;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184194;
                    case 2:
                        return D_dryfield_night_trailer_coach_801841A8;
                    case 3:
                        return D_dryfield_night_trailer_coach_801841B0;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801841C4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801841E0;
                    case 2:
                        return D_dryfield_night_trailer_coach_801841F0;
                    case 3:
                        return D_dryfield_night_trailer_coach_801841FC;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184214;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184230;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184244;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018424C;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184260;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184280;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184290;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018429C;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801840C4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801840D4;
                    case 2:
                        return D_dryfield_night_trailer_coach_801840E4;
                    case 3:
                        return D_dryfield_night_trailer_coach_801840EC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184138;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184150;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184164;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018416C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801840FC;
                    case 1:
                        return D_dryfield_night_trailer_coach_8018410C;
                    case 2:
                        return D_dryfield_night_trailer_coach_8018411C;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184124;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_8018409C;
                    case 1:
                        return D_dryfield_night_trailer_coach_801840A4;
                    case 2:
                        return D_dryfield_night_trailer_coach_801840AC;
                    case 3:
                        return D_dryfield_night_trailer_coach_801840B4;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184348;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184354;
                    case 2:
                        return D_dryfield_night_trailer_coach_8018435C;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184368;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184374;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184380;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184384;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184390;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_8018439C;
                    case 1:
                        return D_dryfield_night_trailer_coach_801843A8;
                    case 2:
                        return D_dryfield_night_trailer_coach_801843B0;
                    case 3:
                        return D_dryfield_night_trailer_coach_801843BC;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801843C8;
                    case 1:
                        return D_dryfield_night_trailer_coach_801843D4;
                    case 2:
                        return D_dryfield_night_trailer_coach_801843DC;
                    case 3:
                        return D_dryfield_night_trailer_coach_801843E8;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801842D4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801842DC;
                    case 2:
                        return D_dryfield_night_trailer_coach_801842E4;
                    case 3:
                        return D_dryfield_night_trailer_coach_801842EC;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_80184320;
                    case 1:
                        return D_dryfield_night_trailer_coach_8018432C;
                    case 2:
                        return D_dryfield_night_trailer_coach_80184334;
                    case 3:
                        return D_dryfield_night_trailer_coach_8018433C;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801842F8;
                    case 1:
                        return D_dryfield_night_trailer_coach_80184300;
                    case 2:
                        return D_dryfield_night_trailer_coach_8018430C;
                    case 3:
                        return D_dryfield_night_trailer_coach_80184314;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_dryfield_night_trailer_coach_801842B4;
                    case 1:
                        return D_dryfield_night_trailer_coach_801842B8;
                    case 2:
                        return D_dryfield_night_trailer_coach_801842BC;
                    case 3:
                        return D_dryfield_night_trailer_coach_801842C4;
                }
                break;
        }
    }
    return D_dryfield_night_trailer_coach_80184578;
}
