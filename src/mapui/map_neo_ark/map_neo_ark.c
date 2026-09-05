#include "common.h"

#include "main/gameflag.h"
#include "mapui/map_neo_ark.h"

/// Fills in the marker state for one Neo Ark map room. Most rooms have no
/// marker; the five that do read a GameFlag nibble, either straight (plus one,
/// rooms 7 / 13 / 32) or folded into a fixed set of states (rooms 20 and 21).
s32 func_map_neo_ark_80179B14(MapNeoArkRec* arg0, MapNeoArkOut* arg1)
{
    if (arg0->field_5 == 0) {
        switch (arg0->field_0) {
            case 7:
                arg1->field_3 = GameFlag_GetNibble(0xE1) + 1;
                break;
            case 13:
                arg1->field_3 = GameFlag_GetNibble(0xD9) + 1;
                break;
            case 20:
                arg1->field_3 = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        arg1->field_3 = 3;
                    } else {
                        arg1->field_3 = 2;
                    }
                }
                break;
            case 21:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    arg1->field_3 = 4;
                } else {
                    arg1->field_3 = 1;
                }
                break;
            case 32:
                arg1->field_3 = GameFlag_GetNibble(0xDD) + 1;
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            default:
                break;
        }
    }
    return 1;
}

INCLUDE_ASM("mapui/nonmatchings/map_neo_ark/map_neo_ark", func_map_neo_ark_80179BE4);

INCLUDE_RODATA("mapui/nonmatchings/map_neo_ark/map_neo_ark", D_map_neo_ark_801799BC);
