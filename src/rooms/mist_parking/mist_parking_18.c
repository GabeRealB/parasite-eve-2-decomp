#include "common.h"

#include "main/mc.h"
#include "rooms/mist_parking.h"

/// The 0xFFFF-terminated item id lists `func_mist_parking_8017D8F8` chooses
/// from.
extern u16 D_mist_parking_80186058[];
extern u16 D_mist_parking_80186060[];
extern u16 D_mist_parking_80186068[];
extern u16 D_mist_parking_80186070[];
extern u16 D_mist_parking_80186080[];
extern u16 D_mist_parking_80186090[];
extern u16 D_mist_parking_801860A0[];
extern u16 D_mist_parking_801860A8[];
extern u16 D_mist_parking_801860B8[];
extern u16 D_mist_parking_801860C8[];
extern u16 D_mist_parking_801860D8[];
extern u16 D_mist_parking_801860E0[];
extern u16 D_mist_parking_801860F4[];
extern u16 D_mist_parking_8018610C[];
extern u16 D_mist_parking_80186120[];
extern u16 D_mist_parking_80186128[];
extern u16 D_mist_parking_80186138[];
extern u16 D_mist_parking_80186150[];
extern u16 D_mist_parking_80186164[];
extern u16 D_mist_parking_8018616C[];
extern u16 D_mist_parking_80186180[];
extern u16 D_mist_parking_8018619C[];
extern u16 D_mist_parking_801861AC[];
extern u16 D_mist_parking_801861B8[];
extern u16 D_mist_parking_801861D0[];
extern u16 D_mist_parking_801861EC[];
extern u16 D_mist_parking_80186200[];
extern u16 D_mist_parking_80186208[];
extern u16 D_mist_parking_8018621C[];
extern u16 D_mist_parking_8018623C[];
extern u16 D_mist_parking_8018624C[];
extern u16 D_mist_parking_80186258[];
extern u16 D_mist_parking_80186270[];
extern u16 D_mist_parking_80186274[];
extern u16 D_mist_parking_80186278[];
extern u16 D_mist_parking_80186280[];
extern u16 D_mist_parking_80186290[];
extern u16 D_mist_parking_80186298[];
extern u16 D_mist_parking_801862A0[];
extern u16 D_mist_parking_801862A8[];
extern u16 D_mist_parking_801862B4[];
extern u16 D_mist_parking_801862BC[];
extern u16 D_mist_parking_801862C8[];
extern u16 D_mist_parking_801862D0[];
extern u16 D_mist_parking_801862DC[];
extern u16 D_mist_parking_801862E8[];
extern u16 D_mist_parking_801862F0[];
extern u16 D_mist_parking_801862F8[];
extern u16 D_mist_parking_80186304[];
extern u16 D_mist_parking_80186310[];
extern u16 D_mist_parking_80186318[];
extern u16 D_mist_parking_80186324[];
extern u16 D_mist_parking_80186330[];
extern u16 D_mist_parking_8018633C[];
extern u16 D_mist_parking_80186340[];
extern u16 D_mist_parking_8018634C[];
extern u16 D_mist_parking_80186358[];
extern u16 D_mist_parking_80186364[];
extern u16 D_mist_parking_8018636C[];
extern u16 D_mist_parking_80186378[];
extern u16 D_mist_parking_80186384[];
extern u16 D_mist_parking_80186390[];
extern u16 D_mist_parking_80186398[];
extern u16 D_mist_parking_801863A4[];
extern u16 D_mist_parking_80186534[];

/// Returns the 0xFFFF-terminated item id list the shop list starts from. The
/// low halfword of `mode` picks a group of lists and the high halfword one of
/// the group's four; `Mc_SaveData.gameMode` 2 and above has groups of its own,
/// and anything unmatched falls back to `D_mist_parking_80186534`.
u16* func_mist_parking_8017D8F8(s32 mode)
{
    if (Mc_SaveData.gameMode < 2) {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186138;
                    case 1:
                        return D_mist_parking_80186150;
                    case 2:
                        return D_mist_parking_80186164;
                    case 3:
                        return D_mist_parking_8018616C;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186180;
                    case 1:
                        return D_mist_parking_8018619C;
                    case 2:
                        return D_mist_parking_801861AC;
                    case 3:
                        return D_mist_parking_801861B8;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801861D0;
                    case 1:
                        return D_mist_parking_801861EC;
                    case 2:
                        return D_mist_parking_80186200;
                    case 3:
                        return D_mist_parking_80186208;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_8018621C;
                    case 1:
                        return D_mist_parking_8018623C;
                    case 2:
                        return D_mist_parking_8018624C;
                    case 3:
                        return D_mist_parking_80186258;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186080;
                    case 1:
                        return D_mist_parking_80186090;
                    case 2:
                        return D_mist_parking_801860A0;
                    case 3:
                        return D_mist_parking_801860A8;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801860F4;
                    case 1:
                        return D_mist_parking_8018610C;
                    case 2:
                        return D_mist_parking_80186120;
                    case 3:
                        return D_mist_parking_80186128;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801860B8;
                    case 1:
                        return D_mist_parking_801860C8;
                    case 2:
                        return D_mist_parking_801860D8;
                    case 3:
                        return D_mist_parking_801860E0;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186058;
                    case 1:
                        return D_mist_parking_80186060;
                    case 2:
                        return D_mist_parking_80186068;
                    case 3:
                        return D_mist_parking_80186070;
                }
                break;
        }
    } else {
        switch ((u16)mode) {
            case 0x30:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186304;
                    case 1:
                        return D_mist_parking_80186310;
                    case 2:
                        return D_mist_parking_80186318;
                    case 3:
                        return D_mist_parking_80186324;
                }
            case 0x31:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186330;
                    case 1:
                        return D_mist_parking_8018633C;
                    case 2:
                        return D_mist_parking_80186340;
                    case 3:
                        return D_mist_parking_8018634C;
                }
            case 0x32:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186358;
                    case 1:
                        return D_mist_parking_80186364;
                    case 2:
                        return D_mist_parking_8018636C;
                    case 3:
                        return D_mist_parking_80186378;
                }
            case 0x33:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186384;
                    case 1:
                        return D_mist_parking_80186390;
                    case 2:
                        return D_mist_parking_80186398;
                    case 3:
                        return D_mist_parking_801863A4;
                }
            case 0x20:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186290;
                    case 1:
                        return D_mist_parking_80186298;
                    case 2:
                        return D_mist_parking_801862A0;
                    case 3:
                        return D_mist_parking_801862A8;
                }
                break;
            case 0x21:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801862DC;
                    case 1:
                        return D_mist_parking_801862E8;
                    case 2:
                        return D_mist_parking_801862F0;
                    case 3:
                        return D_mist_parking_801862F8;
                }
                break;
            case 0x40:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_801862B4;
                    case 1:
                        return D_mist_parking_801862BC;
                    case 2:
                        return D_mist_parking_801862C8;
                    case 3:
                        return D_mist_parking_801862D0;
                }
                break;
            default:
                switch ((u32)mode >> 16) {
                    case 0:
                        return D_mist_parking_80186270;
                    case 1:
                        return D_mist_parking_80186274;
                    case 2:
                        return D_mist_parking_80186278;
                    case 3:
                        return D_mist_parking_80186280;
                }
                break;
        }
    }
    return D_mist_parking_80186534;
}
